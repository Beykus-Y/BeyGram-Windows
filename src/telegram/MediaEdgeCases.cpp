#include "MediaEdgeCases.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

QString typeOf(const QJsonObject& object)
{
    return object.value(QStringLiteral("@type")).toString();
}

/// Accepts either a bare content object or a message that nests one under
/// "content". The nested object wins only when it names a type, so a message
/// whose content is null or malformed still falls back to the outer object.
QJsonObject contentOf(const QJsonObject& object)
{
    const QJsonObject nested = object.value(QStringLiteral("content")).toObject();
    if (!typeOf(nested).isEmpty())
        return nested;
    return object;
}

int nonNegativeInt(const QJsonValue& value)
{
    const qint64 number = jsonInt64(value);
    if (number <= 0)
        return 0;
    return static_cast<int>(qMin<qint64>(number, std::numeric_limits<int>::max()));
}

qint64 nonNegativeInt64(const QJsonValue& value)
{
    const qint64 number = jsonInt64(value);
    return number > 0 ? number : 0;
}

qint64 fileIdOf(const QJsonObject& file)
{
    const qint64 id = jsonInt64(file.value(QStringLiteral("id")));
    return id > 0 ? id : 0;
}

/// TDLib stores the video note's media file under "video"; some persisted
/// wrappers use the generic "file" name, so both are accepted.
QJsonObject videoNoteFile(const QJsonObject& note)
{
    QJsonObject file = note.value(QStringLiteral("video")).toObject();
    if (fileIdOf(file) == 0)
        file = note.value(QStringLiteral("file")).toObject();
    return file;
}

/// tdjson sends the waveform as a base64 string; a stored wrapper may keep the
/// decoded values as an array. Either non-empty form means the note has one.
bool waveformPresent(const QJsonObject& note)
{
    const QJsonValue waveform = note.value(QStringLiteral("waveform"));
    if (waveform.isString())
        return !waveform.toString().trimmed().isEmpty();
    if (waveform.isArray())
        return !waveform.toArray().isEmpty();
    return false;
}

QString labelForKind(const QString& kind)
{
    if (kind == QLatin1String("photo"))
        return QStringLiteral("Photo");
    if (kind == QLatin1String("video"))
        return QStringLiteral("Video");
    if (kind == QLatin1String("animation"))
        return QStringLiteral("GIF");
    if (kind == QLatin1String("audio"))
        return QStringLiteral("Audio");
    if (kind == QLatin1String("voice"))
        return QStringLiteral("Voice");
    if (kind == QLatin1String("document"))
        return QStringLiteral("Document");
    if (kind == QLatin1String("sticker"))
        return QStringLiteral("Sticker");
    if (kind == QLatin1String("location"))
        return QStringLiteral("Location");
    if (kind == QLatin1String("contact"))
        return QStringLiteral("Contact");
    if (kind == QLatin1String("poll"))
        return QStringLiteral("Poll");
    return {};
}

/// A photo has one file per size. The largest one is the full image, so it is
/// the file whose local state decides whether the photo is available.
QJsonObject largestPhotoFile(const QJsonObject& photo)
{
    QJsonObject best;
    qint64 bestArea = -1;
    const QJsonArray sizes = photo.value(QStringLiteral("sizes")).toArray();
    for (const QJsonValue& value : sizes) {
        const QJsonObject size = value.toObject();
        const QJsonObject file = size.value(QStringLiteral("photo")).toObject();
        if (fileIdOf(file) == 0)
            continue;
        const qint64 width = nonNegativeInt(size.value(QStringLiteral("width")));
        const qint64 height = nonNegativeInt(size.value(QStringLiteral("height")));
        const qint64 area = width * height;
        if (area > bestArea) {
            bestArea = area;
            best = file;
        }
    }
    return best;
}

/// The media file that has to be on disk before the content can be shown.
/// Content without such a file yields an empty object.
QJsonObject primaryFile(const QJsonObject& content)
{
    const QString type = typeOf(content);

    if (type == QLatin1String("messagePhoto"))
        return largestPhotoFile(content.value(QStringLiteral("photo")).toObject());
    if (type == QLatin1String("messageVideo"))
        return content.value(QStringLiteral("video"))
            .toObject()
            .value(QStringLiteral("video"))
            .toObject();
    if (type == QLatin1String("messageVideoNote"))
        return videoNoteFile(content.value(QStringLiteral("video_note")).toObject());
    if (type == QLatin1String("messageAnimation"))
        return content.value(QStringLiteral("animation"))
            .toObject()
            .value(QStringLiteral("animation"))
            .toObject();
    if (type == QLatin1String("messageAudio"))
        return content.value(QStringLiteral("audio"))
            .toObject()
            .value(QStringLiteral("audio"))
            .toObject();
    if (type == QLatin1String("messageVoiceNote"))
        return content.value(QStringLiteral("voice_note"))
            .toObject()
            .value(QStringLiteral("voice"))
            .toObject();
    if (type == QLatin1String("messageDocument"))
        return content.value(QStringLiteral("document"))
            .toObject()
            .value(QStringLiteral("document"))
            .toObject();
    if (type == QLatin1String("messageSticker") || type == QLatin1String("messageAnimatedEmoji"))
        return content.value(QStringLiteral("sticker"))
            .toObject()
            .value(QStringLiteral("sticker"))
            .toObject();

    return {};
}

/// TDLib's path is filled for a partial download too, so only the completion
/// flag turns a path into a usable local file.
bool isLocallyComplete(const QJsonObject& file)
{
    const QJsonObject local = file.value(QStringLiteral("local")).toObject();
    if (!local.value(QStringLiteral("is_downloading_completed")).toBool())
        return false;
    return !local.value(QStringLiteral("path")).toString().isEmpty();
}

} // namespace

VideoNoteProbe videoNoteProbe(const QJsonObject& message)
{
    VideoNoteProbe probe;
    const QJsonObject content = contentOf(message);
    if (typeOf(content) != QLatin1String("messageVideoNote"))
        return probe;

    const QJsonObject note = content.value(QStringLiteral("video_note")).toObject();
    const QJsonObject file = videoNoteFile(note);

    probe.fileId = fileIdOf(file);
    probe.hasVideo = probe.fileId > 0;
    probe.hasWaveform = waveformPresent(note);
    probe.durationSeconds = nonNegativeInt(note.value(QStringLiteral("duration")));

    // TDLib's videoNote.length is the square side in pixels, not a duration.
    const int side = nonNegativeInt(note.value(QStringLiteral("length")));
    probe.width = nonNegativeInt(note.value(QStringLiteral("width")));
    if (probe.width == 0)
        probe.width = side;
    probe.height = nonNegativeInt(note.value(QStringLiteral("height")));
    if (probe.height == 0)
        probe.height = side;

    probe.likelyLossy = probe.durationSeconds > 0
        && (!probe.hasWaveform || !probe.hasVideo);
    return probe;
}

MediaBadge mediaBadgeFromContent(const QJsonObject& content)
{
    MediaBadge badge;
    const QJsonObject resolved = contentOf(content);
    const QString type = typeOf(resolved);

    badge.kind = mediaKindFromContent(resolved);
    badge.label = labelForKind(badge.kind);
    badge.spoiler = resolved.value(QStringLiteral("has_spoiler")).toBool()
        || resolved.value(QStringLiteral("is_secret")).toBool();
    badge.expired = type.startsWith(QLatin1String("messageExpired"))
        || resolved.value(QStringLiteral("expired")).toBool()
        || resolved.value(QStringLiteral("is_expired")).toBool();

    qint64 ttl = nonNegativeInt64(resolved.value(QStringLiteral("ttl")));
    if (ttl == 0)
        ttl = nonNegativeInt64(resolved.value(QStringLiteral("ttl_seconds")));
    badge.ttlSeconds = ttl;
    return badge;
}

QString mediaKindFromContent(const QJsonObject& content)
{
    const QString type = typeOf(contentOf(content));

    if (type == QLatin1String("messagePhoto") || type == QLatin1String("messageExpiredPhoto"))
        return QStringLiteral("photo");
    if (type == QLatin1String("messageVideo") || type == QLatin1String("messageVideoNote")
        || type == QLatin1String("messageExpiredVideo")
        || type == QLatin1String("messageExpiredVideoNote")) {
        return QStringLiteral("video");
    }
    if (type == QLatin1String("messageAnimation"))
        return QStringLiteral("animation");
    if (type == QLatin1String("messageAudio"))
        return QStringLiteral("audio");
    if (type == QLatin1String("messageVoiceNote")
        || type == QLatin1String("messageExpiredVoiceNote")) {
        return QStringLiteral("voice");
    }
    if (type == QLatin1String("messageDocument"))
        return QStringLiteral("document");
    if (type == QLatin1String("messageSticker")
        || type == QLatin1String("messageAnimatedEmoji")) {
        return QStringLiteral("sticker");
    }
    if (type == QLatin1String("messageLocation")
        || type == QLatin1String("messageLiveLocation")
        || type == QLatin1String("messageVenue")) {
        return QStringLiteral("location");
    }
    if (type == QLatin1String("messageContact"))
        return QStringLiteral("contact");
    if (type == QLatin1String("messagePoll"))
        return QStringLiteral("poll");
    return QStringLiteral("none");
}

bool mediaNeedsDownload(const QJsonObject& content)
{
    const QJsonObject file = primaryFile(contentOf(content));
    if (fileIdOf(file) == 0)
        return false;
    return !isLocallyComplete(file);
}

} // namespace beygram::telegram
