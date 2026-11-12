#include "ProfileMedia.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <algorithm>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kMinProfilePhotosLimit = 1;
constexpr int kMaxProfilePhotosLimit = 100;

QJsonObject inputFileId(qint64 fileId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputFileId")},
        {QStringLiteral("id"), fileId},
    };
}

// TDLib puts the profile track in userFullInfo.first_profile_audio. The alias
// and the bare-audio cases keep the parser useful for callers that cache the
// field under another name or pass the audio object directly.
QJsonObject audioObject(const QJsonObject& user)
{
    const QJsonObject first =
        user.value(QStringLiteral("first_profile_audio")).toObject();
    if (!first.isEmpty())
        return first;

    const QJsonObject alias =
        user.value(QStringLiteral("profile_audio")).toObject();
    if (!alias.isEmpty())
        return alias;

    if (user.value(QStringLiteral("@type")).toString() == QLatin1String("audio"))
        return user;

    // A fixture may omit @type; the nested audio file still identifies it.
    if (user.value(QStringLiteral("audio")).isObject())
        return user;

    return {};
}

qint64 audioFileId(const QJsonObject& audio)
{
    qint64 id = jsonInt64(
        audio.value(QStringLiteral("audio")).toObject().value(QStringLiteral("id")));
    if (id <= 0) {
        id = jsonInt64(
            audio.value(QStringLiteral("file")).toObject().value(QStringLiteral("id")));
    }
    if (id <= 0)
        id = jsonInt64(audio.value(QStringLiteral("file_id")));
    return id > 0 ? id : 0;
}

// A negative duration means "unknown" on the wire; show it as zero. Values
// beyond int range are clamped rather than truncated.
int clampedDuration(const QJsonValue& value)
{
    const qint64 duration = jsonInt64(value);
    if (duration <= 0)
        return 0;
    return static_cast<int>(
        std::min<qint64>(duration, std::numeric_limits<int>::max()));
}

} // namespace

ProfileAudioInfo profileAudioFromUser(const QJsonObject& user)
{
    const QJsonObject audio = audioObject(user);
    if (audio.isEmpty())
        return {};

    ProfileAudioInfo info;
    info.fileId = audioFileId(audio);
    if (info.fileId <= 0)
        return {};

    // Tags are copied as TDLib reported them; unlike names and bios, track
    // metadata is not trimmed or otherwise rewritten here.
    info.title = audio.value(QStringLiteral("title")).toString();
    info.performer = audio.value(QStringLiteral("performer")).toString();
    info.durationSeconds = clampedDuration(audio.value(QStringLiteral("duration")));
    return info;
}

QJsonObject setProfileAudioRequest(qint64 fileId)
{
    if (fileId <= 0)
        return {};

    // TDLib's addProfileAudio takes a complete inputAudio even when the file is
    // already known; the metadata fields are required, so send neutral values.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("addProfileAudio")},
        {QStringLiteral("audio"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("inputAudio")},
             {QStringLiteral("audio"), inputFileId(fileId)},
             {QStringLiteral("album_cover_thumbnail"), QJsonValue::Null},
             {QStringLiteral("duration"), 0},
             {QStringLiteral("title"), QString()},
             {QStringLiteral("performer"), QString()},
         }},
    };
}

QJsonObject removeProfileAudioRequest()
{
    // TDLib also requires file_id; the requested no-argument interface cannot
    // supply it, so this is only the envelope. See the header for the contract.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("removeProfileAudio")},
    };
}

QJsonObject getProfilePhotosRequest(qint64 userId, int offset, int limit)
{
    if (userId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getUserProfilePhotos")},
        {QStringLiteral("user_id"), userId},
        {QStringLiteral("offset"), std::max(offset, 0)},
        {QStringLiteral("limit"),
         std::clamp(limit, kMinProfilePhotosLimit, kMaxProfilePhotosLimit)},
    };
}

QJsonObject setProfilePhotoRequest(const QString& localPath, bool isPublic)
{
    // A filesystem path is not trimmed: leading or trailing spaces can be part
    // of a real file name. Only a path with no visible characters is rejected.
    if (localPath.trimmed().isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setProfilePhoto")},
        {QStringLiteral("photo"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("inputChatPhotoStatic")},
             {QStringLiteral("photo"),
              QJsonObject{
                  {QStringLiteral("@type"), QStringLiteral("inputFileLocal")},
                  {QStringLiteral("path"), localPath},
              }},
         }},
        {QStringLiteral("is_public"), isPublic},
    };
}


} // namespace beygram::telegram
