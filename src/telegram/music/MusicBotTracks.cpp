#include "MusicBotTracks.h"

#include <QJsonValue>
#include <QVariant>

#include <algorithm>
#include <initializer_list>
#include <limits>

namespace beygram::telegram {
namespace {

qint64 jsonInt64(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().toLongLong();
    if (value.isDouble())
        return value.toVariant().toLongLong();
    return 0;
}

// A negative or malformed duration means "unknown" and becomes 0; a value
// beyond the int range is clamped instead of wrapping.
int clampedDuration(const QJsonObject& audio)
{
    qint64 duration = 0;
    for (const char* key : {"duration", "duration_seconds", "durationSeconds"}) {
        const QJsonValue value = audio.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        duration = jsonInt64(value);
        break;
    }

    if (duration <= 0)
        return 0;
    return static_cast<int>(std::min<qint64>(duration, std::numeric_limits<int>::max()));
}

// A TDLib file id is a positive int32; zero, negatives and out-of-range values
// all mean "no file".
int positiveFileId(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

int fileIdFrom(const QJsonObject& audio)
{
    for (const char* key : {"fileId", "file_id", "audioFileId", "audio_file_id"}) {
        const QJsonValue value = audio.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        const int id = positiveFileId(value);
        if (id > 0)
            return id;
    }

    // A TDLib audio object keeps its file in the nested `audio` field.
    int id = positiveFileId(audio.value(QStringLiteral("audio"))
                                .toObject()
                                .value(QStringLiteral("id")));
    if (id > 0)
        return id;

    return positiveFileId(audio.value(QStringLiteral("file"))
                              .toObject()
                              .value(QStringLiteral("id")));
}

QString trimmedString(const QJsonValue& value)
{
    return value.toString().trimmed();
}

QString fileUniqueIdFrom(const QJsonObject& audio)
{
    // The stable identity of a Telegram file is remote.unique_id; unlike a
    // file id it survives across sessions. A TDLib audio object nests its file
    // under `audio`, a bare file object under `file`.
    for (const char* key : {"audio", "file"}) {
        const QString id = trimmedString(audio.value(QString::fromLatin1(key))
                                             .toObject()
                                             .value(QStringLiteral("remote"))
                                             .toObject()
                                             .value(QStringLiteral("unique_id")));
        if (!id.isEmpty())
            return id;
    }

    const QString nested = trimmedString(audio.value(QStringLiteral("remote"))
                                             .toObject()
                                             .value(QStringLiteral("unique_id")));
    if (!nested.isEmpty())
        return nested;

    for (const char* key : {"fileUniqueId", "file_unique_id"}) {
        const QString id = trimmedString(audio.value(QString::fromLatin1(key)));
        if (!id.isEmpty())
            return id;
    }

    return {};
}

bool sameTrack(const BotTrack& left, const BotTrack& right)
{
    return left.trackId == right.trackId
        && left.title == right.title
        && left.performer == right.performer
        && left.fileUniqueId == right.fileUniqueId
        && left.durationSeconds == right.durationSeconds
        && left.fileId == right.fileId
        && left.cachedAt == right.cachedAt;
}

} // namespace

BotTrack botTrackFromMessage(const QJsonObject& message)
{
    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    if (content.value(QStringLiteral("@type")).toString() != QLatin1String("messageAudio"))
        return {};

    const QJsonObject audio = content.value(QStringLiteral("audio")).toObject();
    if (audio.isEmpty())
        return {};

    BotTrack track;
    track.title = audio.value(QStringLiteral("title")).toString();
    track.performer = audio.value(QStringLiteral("performer")).toString();
    track.fileUniqueId = fileUniqueIdFrom(audio);
    track.durationSeconds = clampedDuration(audio);
    track.fileId = fileIdFrom(audio);

    // The bot gives each result a title; it is the name the user picked and
    // the only identity available when the inline result carried no file. A
    // result without a title still needs an address, so the file's stable
    // unique id takes over.
    track.trackId = track.title.trimmed();
    if (track.trackId.isEmpty())
        track.trackId = track.fileUniqueId;

    return track;
}

MusicBotCache::MusicBotCache(QObject* parent)
    : QObject(parent)
{
}

void MusicBotCache::setCapacity(int capacity)
{
    const int clamped = qBound(kMinimumCapacity, capacity, kMaximumCapacity);
    if (clamped == m_capacity)
        return;

    m_capacity = clamped;
    evictOverflow();
    emit changed();
}

int MusicBotCache::capacity() const
{
    return m_capacity;
}

void MusicBotCache::put(const BotTrack& track, qint64 nowUnix)
{
    // The trackId is the identity of the row; a track without one cannot be
    // looked up later and would only waste a slot.
    BotTrack stored = track;
    stored.trackId = track.trackId.trimmed();
    if (stored.trackId.isEmpty())
        return;

    // The cache owns recency: the caller's cachedAt is only a placeholder, and
    // the moment of this put() is what eviction order is measured by.
    stored.cachedAt = nowUnix;

    const int existing = indexOf(stored.trackId);
    if (existing >= 0) {
        if (sameTrack(m_entries.at(existing), stored))
            return;
        m_entries[existing] = stored;
    } else {
        m_entries.append(stored);
    }

    const bool added = existing < 0;
    evictOverflow();

    // A newcomer older than everything already stored is itself the victim, so
    // the eviction removed exactly what this call added: nothing really changed.
    if (added && !contains(stored.trackId))
        return;

    emit changed();
}

BotTrack MusicBotCache::get(const QString& trackId) const
{
    const int index = indexOf(trackId);
    return index >= 0 ? m_entries.at(index) : BotTrack();
}

bool MusicBotCache::contains(const QString& trackId) const
{
    return indexOf(trackId) >= 0;
}

void MusicBotCache::remove(const QString& trackId)
{
    const int index = indexOf(trackId);
    if (index < 0)
        return;
    m_entries.removeAt(index);
    emit changed();
}

void MusicBotCache::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    emit changed();
}

int MusicBotCache::count() const
{
    return int(m_entries.size());
}

int MusicBotCache::indexOf(const QString& trackId) const
{
    const QString key = trackId.trimmed();
    if (key.isEmpty())
        return -1;

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).trackId == key)
            return i;
    }
    return -1;
}

void MusicBotCache::evictOverflow()
{
    while (m_entries.size() > m_capacity) {
        int victim = 0;
        for (int i = 1; i < m_entries.size(); ++i) {
            const BotTrack& candidate = m_entries.at(i);
            const BotTrack& oldest = m_entries.at(victim);
            if (candidate.cachedAt < oldest.cachedAt
                || (candidate.cachedAt == oldest.cachedAt
                    && candidate.trackId < oldest.trackId)) {
                victim = i;
            }
        }
        m_entries.removeAt(victim);
    }
}

} // namespace beygram::telegram
