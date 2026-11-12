#include "BackgroundMusic.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QStringList>
#include <QVariant>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int kMinLimit = 1;
constexpr int kMaxLimit = 100;

qint64 jsonInt64(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().toLongLong();
    if (value.isDouble())
        return value.toVariant().toLongLong();
    return 0;
}

QString firstString(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (!value.isString())
            continue;
        const QString text = value.toString();
        if (!text.isEmpty())
            return text;
    }
    return QString();
}

QString artistsString(const QJsonObject& object)
{
    const QJsonArray artists = object.value(QStringLiteral("artists")).toArray();
    QStringList names;
    names.reserve(artists.size());
    for (const QJsonValue& value : artists) {
        if (value.isString() && !value.toString().trimmed().isEmpty())
            names.append(value.toString());
    }
    return names.join(QStringLiteral(", "));
}

int nonNegativeSeconds(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        const qint64 seconds = jsonInt64(value);
        if (seconds <= 0)
            return 0;
        return static_cast<int>(std::min<qint64>(seconds, std::numeric_limits<int>::max()));
    }
    return 0;
}

int positiveFileId(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

int fileIdFromTrack(const QJsonObject& track)
{
    for (const char* key : {"fileId", "file_id", "audioFileId", "audio_file_id"}) {
        const QJsonValue value = track.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        const int id = positiveFileId(value);
        if (id > 0)
            return id;
    }

    int id = positiveFileId(track.value(QStringLiteral("file")).toObject()
                                .value(QStringLiteral("id")));
    if (id > 0)
        return id;

    // A TDLib audio object keeps its file in the nested `audio` field.
    id = positiveFileId(track.value(QStringLiteral("audio")).toObject()
                            .value(QStringLiteral("id")));
    return id;
}

QString localPathFromTrack(const QJsonObject& track)
{
    QString path = firstString(track, {"localPath", "local_path"});
    if (!path.isEmpty())
        return path;

    path = track.value(QStringLiteral("file"))
               .toObject()
               .value(QStringLiteral("local"))
               .toObject()
               .value(QStringLiteral("path"))
               .toString();
    if (!path.isEmpty())
        return path;

    path = track.value(QStringLiteral("audio"))
               .toObject()
               .value(QStringLiteral("local"))
               .toObject()
               .value(QStringLiteral("path"))
               .toString();
    if (!path.isEmpty())
        return path;

    return track.value(QStringLiteral("local"))
        .toObject()
        .value(QStringLiteral("path"))
        .toString();
}

bool looksLikeAudioMetadata(const QJsonObject& object)
{
    if (object.isEmpty())
        return false;
    return object.contains(QStringLiteral("title"))
        || object.contains(QStringLiteral("performer"))
        || object.contains(QStringLiteral("artist"))
        || object.contains(QStringLiteral("duration"))
        || object.contains(QStringLiteral("durationSeconds"))
        || object.contains(QStringLiteral("duration_seconds"))
        || object.contains(QStringLiteral("durationSec"))
        || object.contains(QStringLiteral("audio"))
        || object.contains(QStringLiteral("file"));
}

struct TrackFields {
    QJsonObject object;
    qint64 id = 0;
};

/// A response may wrap a track in `track`, put it in a TDLib message's
/// `content.audio`, or carry the audio object directly.
TrackFields fieldsFromEntry(const QJsonObject& entry)
{
    TrackFields fields;

    const QJsonObject wrapped = entry.value(QStringLiteral("track")).toObject();
    if (!wrapped.isEmpty()) {
        fields.object = wrapped;
        fields.id = jsonInt64(wrapped.value(QStringLiteral("id")));
        if (fields.id <= 0)
            fields.id = jsonInt64(entry.value(QStringLiteral("id")));
        return fields;
    }

    const QJsonObject content = entry.value(QStringLiteral("content")).toObject();
    if (!content.isEmpty()) {
        const QJsonObject audio = content.value(QStringLiteral("audio")).toObject();
        if (!audio.isEmpty()) {
            fields.object = audio;
            fields.id = jsonInt64(entry.value(QStringLiteral("id")));
            return fields;
        }
    }

    const QJsonObject audio = entry.value(QStringLiteral("audio")).toObject();
    if (looksLikeAudioMetadata(audio)) {
        fields.object = audio;
        fields.id = jsonInt64(entry.value(QStringLiteral("id")));
        if (fields.id <= 0)
            fields.id = jsonInt64(audio.value(QStringLiteral("id")));
        return fields;
    }

    fields.object = entry;
    fields.id = jsonInt64(entry.value(QStringLiteral("id")));
    if (fields.id <= 0)
        fields.id = jsonInt64(entry.value(QStringLiteral("track_id")));
    return fields;
}

QJsonArray tracksArrayFromResponse(const QJsonObject& response)
{
    const QJsonValue direct = response.value(QStringLiteral("tracks"));
    if (direct.isArray())
        return direct.toArray();

    const QJsonValue music = response.value(QStringLiteral("music"));
    if (music.isArray())
        return music.toArray();

    const QJsonValue results = response.value(QStringLiteral("results"));
    if (results.isArray())
        return results.toArray();

    const QJsonValue messages = response.value(QStringLiteral("messages"));
    if (messages.isArray())
        return messages.toArray();

    const QJsonValue playlistTracks =
        response.value(QStringLiteral("playlist")).toObject().value(QStringLiteral("tracks"));
    if (playlistTracks.isArray())
        return playlistTracks.toArray();

    return QJsonArray();
}

MusicTrack trackFromEntry(const QJsonObject& entry)
{
    const TrackFields fields = fieldsFromEntry(entry);

    MusicTrack track;
    track.id = fields.id;
    track.title = firstString(fields.object, {"title", "name"});
    track.performer = firstString(fields.object, {"performer", "artist"});
    if (track.performer.isEmpty())
        track.performer = artistsString(fields.object);
    track.durationSeconds = nonNegativeSeconds(
        fields.object, {"durationSeconds", "duration_seconds", "duration", "durationSec"});
    track.fileId = fileIdFromTrack(fields.object);
    track.localPath = localPathFromTrack(fields.object);
    return track;
}

QVector<MusicTrack> cleanTracks(const QVector<MusicTrack>& tracks)
{
    QVector<MusicTrack> kept;
    kept.reserve(tracks.size());

    QSet<qint64> seenIds;
    seenIds.reserve(tracks.size());

    for (MusicTrack track : tracks) {
        if (track.id <= 0 || seenIds.contains(track.id))
            continue;
        track.durationSeconds = std::max(0, track.durationSeconds);
        seenIds.insert(track.id);
        kept.append(std::move(track));
    }

    return kept;
}

} // namespace

QJsonObject searchMusicRequest(const QString& query, int limit)
{
    const QString text = query.trimmed();
    if (text.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchMusic")},
        {QStringLiteral("query"), text},
        {QStringLiteral("limit"), std::clamp(limit, kMinLimit, kMaxLimit)},
    };
}

QJsonObject getMusicPlaylistRequest(qint64 playlistId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMusicPlaylist")},
        {QStringLiteral("playlist_id"), playlistId},
    };
}

QVector<MusicTrack> musicTracksFromResponse(const QJsonObject& response)
{
    QVector<MusicTrack> tracks;

    const QJsonArray entries = tracksArrayFromResponse(response);
    if (entries.isEmpty())
        return tracks;

    tracks.reserve(entries.size());
    QSet<qint64> seenIds;
    seenIds.reserve(entries.size());

    for (const QJsonValue& value : entries) {
        if (!value.isObject())
            continue;

        const MusicTrack track = trackFromEntry(value.toObject());
        if (track.id <= 0 || seenIds.contains(track.id))
            continue;

        seenIds.insert(track.id);
        tracks.append(track);
    }

    return tracks;
}

QString formatDuration(int seconds)
{
    if (seconds < 0)
        return QStringLiteral("0:00");

    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int secs = seconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(secs, 2, 10, QLatin1Char('0'));
}

MusicQueue::MusicQueue(QObject* parent) : QObject(parent)
{
}

void MusicQueue::setTracks(QVector<MusicTrack> incoming)
{
    QVector<MusicTrack> cleaned = cleanTracks(incoming);
    if (m_index < 0 && cleaned == m_tracks)
        return;

    m_tracks = std::move(cleaned);
    m_index = -1;
    m_repeatAll = false;
    emit changed();
}

QVector<MusicTrack> MusicQueue::tracks() const
{
    return m_tracks;
}

int MusicQueue::currentIndex() const
{
    return m_index;
}

MusicTrack MusicQueue::current() const
{
    if (m_index < 0 || m_index >= m_tracks.size())
        return MusicTrack{};
    return m_tracks.at(m_index);
}

void MusicQueue::playAt(int index)
{
    if (index < 0 || index >= m_tracks.size() || index == m_index)
        return;

    m_index = index;
    emit changed();
}

bool MusicQueue::next(bool repeatAll)
{
    m_repeatAll = repeatAll;

    const int total = static_cast<int>(m_tracks.size());
    if (total == 0)
        return false;

    if (m_index < 0 || m_index >= total) {
        playAt(0);
        return true;
    }

    if (m_index + 1 < total) {
        playAt(m_index + 1);
        return true;
    }

    if (!repeatAll)
        return false;

    playAt(0);
    return true;
}

bool MusicQueue::previous()
{
    const int total = static_cast<int>(m_tracks.size());
    if (total == 0)
        return false;

    if (m_index < 0 || m_index >= total) {
        playAt(total - 1);
        return true;
    }

    if (m_index > 0) {
        playAt(m_index - 1);
        return true;
    }

    if (!m_repeatAll)
        return false;

    playAt(total - 1);
    return true;
}

void MusicQueue::clear()
{
    if (m_tracks.isEmpty() && m_index < 0) {
        m_repeatAll = false;
        return;
    }

    m_tracks.clear();
    m_index = -1;
    m_repeatAll = false;
    emit changed();
}

} // namespace beygram::telegram
