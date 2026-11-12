#include "CallRequests.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinCallDuration = 0;
constexpr int kMaxCallDuration = 86400;
constexpr int kMinCallRating = 1;
constexpr int kMaxCallRating = 5;
constexpr int kMaxCallCommentLength = 4096;

// TDLib rejects durations outside 0..86400.
int clampedDuration(int duration)
{
    return std::clamp(duration, kMinCallDuration, kMaxCallDuration);
}

// TDLib rates a call on a 1..5 scale.
int clampedRating(int rating)
{
    return std::clamp(rating, kMinCallRating, kMaxCallRating);
}

/// Cuts text to at most maxLength Unicode code points without splitting a
/// surrogate pair, which would leave half an emoji in the payload.
QString limitedToCodePoints(const QString& text, int maxLength)
{
    int codePoints = 0;
    qsizetype index = 0;
    while (index < text.size()) {
        if (codePoints == maxLength)
            return text.left(index);

        const QChar character = text.at(index);
        const bool surrogatePair = character.isHighSurrogate() && index + 1 < text.size()
            && text.at(index + 1).isLowSurrogate();
        index += surrogatePair ? 2 : 1;
        ++codePoints;
    }
    return text;
}

QString callComment(const QString& comment)
{
    return limitedToCodePoints(comment.trimmed(), kMaxCallCommentLength);
}

} // namespace

QJsonObject getCallRequest(qint64 callId)
{
    if (callId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getCall")},
        {QStringLiteral("call_id"), callId},
    };
}

QJsonObject acceptCallRequest(qint64 callId, const QJsonObject& protocol)
{
    if (callId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("acceptCall")},
        {QStringLiteral("call_id"), callId},
        {QStringLiteral("protocol"), protocol},
    };
}

QJsonObject discardCallRequest(qint64 callId, bool isDisconnected, int duration,
                               bool isVideo)
{
    if (callId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("discardCall")},
        {QStringLiteral("call_id"), callId},
        {QStringLiteral("is_disconnected"), isDisconnected},
        {QStringLiteral("duration"), clampedDuration(duration)},
        {QStringLiteral("is_video"), isVideo},
    };
}

QJsonObject sendCallRatingRequest(qint64 callId, int rating, const QString& comment)
{
    if (callId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendCallRating")},
        {QStringLiteral("call_id"), callId},
        {QStringLiteral("rating"), clampedRating(rating)},
        {QStringLiteral("comment"), callComment(comment)},
    };
}

QJsonObject sendCallDebugInformationRequest(qint64 callId,
                                            const QString& debugInformation)
{
    if (callId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendCallDebugInformation")},
        {QStringLiteral("call_id"), callId},
        {QStringLiteral("debug_information"), debugInformation},
    };
}

QJsonObject sendCallLogRequest(const QString& logFile)
{
    const QString path = logFile.trimmed();
    if (path.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendCallLog")},
        {QStringLiteral("log_file"), path},
    };
}

} // namespace beygram::telegram
