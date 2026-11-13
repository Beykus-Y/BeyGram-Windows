#include "MusicModels.h"

#include <QJsonValue>
#include <QSet>
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
    return {};
}

QString firstTrimmedString(const QJsonObject& object,
                           std::initializer_list<const char*> keys)
{
    return firstString(object, keys).trimmed();
}

// A JSON id may be written as a number or as a string. Zero and an empty
// string both mean "no id"; every other value is kept as text.
QString jsonIdText(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().trimmed();
    if (value.isDouble()) {
        const qint64 id = value.toVariant().toLongLong();
        if (id != 0)
            return QString::number(id);
    }
    return {};
}

int positiveFileId(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

// The first present key wins; a missing, negative or malformed value is
// reported as zero.
int nonNegativeInt(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        const qint64 number = jsonInt64(value);
        if (number <= 0)
            return 0;
        return static_cast<int>(std::min<qint64>(number, std::numeric_limits<int>::max()));
    }
    return 0;
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
    int id = positiveFileId(audio.value(QStringLiteral("audio")).toObject()
                                .value(QStringLiteral("id")));
    if (id > 0)
        return id;

    return positiveFileId(audio.value(QStringLiteral("file")).toObject()
                              .value(QStringLiteral("id")));
}

QString localPathFrom(const QJsonObject& audio)
{
    QString path = firstString(audio, {"localPath", "local_path"});
    if (!path.isEmpty())
        return path;

    path = audio.value(QStringLiteral("audio"))
               .toObject()
               .value(QStringLiteral("local"))
               .toObject()
               .value(QStringLiteral("path"))
               .toString();
    if (!path.isEmpty())
        return path;

    path = audio.value(QStringLiteral("file"))
               .toObject()
               .value(QStringLiteral("local"))
               .toObject()
               .value(QStringLiteral("path"))
               .toString();
    if (!path.isEmpty())
        return path;

    return audio.value(QStringLiteral("local"))
        .toObject()
        .value(QStringLiteral("path"))
        .toString();
}

// A TDLib audio object nests its file under `audio`; a message nests the
// audio under `content.audio`. The file object itself has none of these
// metadata keys, so a bare audio object is not mistaken for its own file.
bool looksLikeAudioMetadata(const QJsonObject& object)
{
    if (object.isEmpty())
        return false;

    for (const char* key : {"title", "performer", "artist", "album", "duration",
                            "durationSeconds", "duration_seconds", "file_name",
                            "mime_type", "track_number", "trackNumber"}) {
        if (object.contains(QString::fromLatin1(key)))
            return true;
    }
    return false;
}

QJsonObject audioObjectFrom(const QJsonObject& entry, QJsonObject* wrapper)
{
    *wrapper = entry;

    const QJsonObject content = entry.value(QStringLiteral("content")).toObject();
    if (!content.isEmpty()) {
        const QJsonObject audio = content.value(QStringLiteral("audio")).toObject();
        if (!audio.isEmpty())
            return audio;
    }

    const QJsonObject nested = entry.value(QStringLiteral("audio")).toObject();
    if (looksLikeAudioMetadata(nested))
        return nested;

    return entry;
}

QString trackIdFrom(const QJsonObject& wrapper, const QJsonObject& audio)
{
    for (const char* key : {"id", "message_id", "messageId", "track_id", "trackId"}) {
        const QString id = jsonIdText(wrapper.value(QString::fromLatin1(key)));
        if (!id.isEmpty())
            return id;
    }

    for (const char* key : {"id", "track_id", "trackId"}) {
        const QString id = jsonIdText(audio.value(QString::fromLatin1(key)));
        if (!id.isEmpty())
            return id;
    }

    // TDLib keeps a stable identity for a file in remote.unique_id; unlike a
    // file id it survives across sessions, so a bare audio object can still be
    // addressed after a restart.
    for (const char* key : {"audio", "file"}) {
        const QString id = audio.value(QString::fromLatin1(key))
                               .toObject()
                               .value(QStringLiteral("remote"))
                               .toObject()
                               .value(QStringLiteral("unique_id"))
                               .toString()
                               .trimmed();
        if (!id.isEmpty())
            return id;
    }

    return audio.value(QStringLiteral("remote"))
        .toObject()
        .value(QStringLiteral("unique_id"))
        .toString()
        .trimmed();
}

} // namespace

MusicTrackModel musicTrackFromAudio(const QJsonObject& audio)
{
    QJsonObject wrapper;
    const QJsonObject object = audioObjectFrom(audio, &wrapper);

    MusicTrackModel track;
    track.id = trackIdFrom(wrapper, object);
    track.title = firstString(object, {"title", "name"});

    // A TDLib audio object carries tags, not catalogue ids. When an explicit
    // id is absent the tag text is the only stable identity available, so it
    // becomes the grouping id.
    track.artistId = firstTrimmedString(
        object, {"artist_id", "artistId", "performer_id", "performerId"});
    if (track.artistId.isEmpty())
        track.artistId = firstTrimmedString(object, {"performer", "artist"});

    track.albumId = firstTrimmedString(object, {"album_id", "albumId"});
    if (track.albumId.isEmpty())
        track.albumId = firstTrimmedString(object, {"album"});

    track.durationSeconds = nonNegativeInt(
        object, {"duration", "duration_seconds", "durationSeconds"});
    track.fileId = fileIdFrom(object);
    track.localPath = localPathFrom(object);
    track.trackNumber = nonNegativeInt(object, {"track_number", "trackNumber"});
    return track;
}

int musicAlbumYear(int year)
{
    if (year == 0)
        return 0;
    if (year < 1000 || year > 2100)
        return 0;
    return year;
}

QVector<QString> musicDedupedTrackIds(const QVector<QString>& trackIds)
{
    QVector<QString> ids;
    ids.reserve(trackIds.size());

    QSet<QString> seen;
    seen.reserve(trackIds.size());

    for (const QString& rawId : trackIds) {
        const QString id = rawId.trimmed();
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);
        ids.append(id);
    }

    return ids;
}

QString musicDurationText(int seconds)
{
    if (seconds < 0)
        seconds = 0;

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

} // namespace beygram::telegram
