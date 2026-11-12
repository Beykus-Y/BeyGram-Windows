#include "ChatExport.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int kMinExportLimit = 1;
constexpr int kMaxExportLimit = 1000;

qint64 nonNegative(qint64 value)
{
    return value > 0 ? value : 0;
}

/// Returns the first present, non-null member among the accepted spellings.
QJsonValue memberOf(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (!value.isUndefined() && !value.isNull())
            return value;
    }
    return QJsonValue(QJsonValue::Undefined);
}

/// Export updates may carry their counters directly or in a nested progress
/// object; both layouts describe the same snapshot.
QJsonValue progressMember(const QJsonObject& update, std::initializer_list<const char*> keys)
{
    const QJsonValue direct = memberOf(update, keys);
    if (!direct.isUndefined())
        return direct;

    const QJsonObject progress = update.value(QStringLiteral("progress")).toObject();
    if (!progress.isEmpty())
        return memberOf(progress, keys);
    return QJsonValue(QJsonValue::Undefined);
}

bool boolMember(const QJsonObject& update, std::initializer_list<const char*> keys)
{
    const QJsonValue value = progressMember(update, keys);
    return value.isBool() && value.toBool();
}

/// TDLib counters are int32; a negative or oversized value is clamped instead
/// of wrapping the public int fields.
int countMember(const QJsonObject& update, std::initializer_list<const char*> keys)
{
    const qint64 count = jsonInt64(progressMember(update, keys));
    return static_cast<int>(
        std::clamp<qint64>(count, 0, std::numeric_limits<int>::max()));
}

/// TDLib reports errors as {"code": ..., "message": ...}; a bare string is
/// accepted too. The result is always trimmed.
QString errorText(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().trimmed();
    if (!value.isObject())
        return {};

    const QJsonObject object = value.toObject();
    const QString message = object.value(QStringLiteral("message")).toString().trimmed();
    if (!message.isEmpty())
        return message;

    const QJsonValue nested = object.value(QStringLiteral("error"));
    if (!nested.isUndefined() && !nested.isNull())
        return errorText(nested);
    return {};
}

/// A completed file transfer is a terminal progress event. The file object is
/// normally under "file"; an updateFile payload that was handed over directly
/// is accepted as well.
bool fileTransferCompleted(const QJsonObject& update)
{
    QJsonObject file = update.value(QStringLiteral("file")).toObject();
    if (file.isEmpty())
        file = update;

    if (file.value(QStringLiteral("is_downloading_completed")).toBool())
        return true;
    if (file.value(QStringLiteral("local"))
            .toObject()
            .value(QStringLiteral("is_downloading_completed"))
            .toBool()) {
        return true;
    }
    return file.value(QStringLiteral("remote"))
        .toObject()
        .value(QStringLiteral("is_uploading_completed"))
        .toBool();
}

bool terminalStatus(const QJsonObject& update)
{
    const QString status =
        progressMember(update, { "state", "status" }).toString().trimmed().toLower();
    return status == QLatin1String("done") || status == QLatin1String("complete")
        || status == QLatin1String("completed") || status == QLatin1String("finished")
        || status == QLatin1String("success") || status == QLatin1String("succeeded");
}

} // namespace

QJsonObject exportChatHistoryRequest(qint64 chatId, const ChatExportOptions& options,
                                     qint64 fromMessageId, int limit)
{
    if (chatId <= 0)
        return {};

    // Normalize negative bounds first, then order the range.
    qint64 fromDate = nonNegative(options.fromDate);
    qint64 toDate = nonNegative(options.toDate);
    if (fromDate > toDate)
        std::swap(fromDate, toDate);

    return QJsonObject{
        { QStringLiteral("@type"), QStringLiteral("exportChatHistory") },
        { QStringLiteral("chat_id"), chatId },
        { QStringLiteral("from_message_id"), nonNegative(fromMessageId) },
        { QStringLiteral("limit"), std::clamp(limit, kMinExportLimit, kMaxExportLimit) },
        { QStringLiteral("only_my_messages"), options.onlyMyMessages },
        { QStringLiteral("from_date"), fromDate },
        { QStringLiteral("to_date"), toDate },
        { QStringLiteral("include_media"), options.includeMedia },
        { QStringLiteral("include_photos"), options.includePhotos },
        { QStringLiteral("include_videos"), options.includeVideos },
        { QStringLiteral("include_files"), options.includeFiles },
        { QStringLiteral("include_voice"), options.includeVoice },
        { QStringLiteral("include_links"), options.includeLinks },
    };
}

ChatExportProgress exportProgressFromUpdate(const QJsonObject& update)
{
    ChatExportProgress progress;
    if (update.isEmpty())
        return progress;

    progress.messagesExported = countMember(
        update, { "messages_exported", "messagesExported", "exported_messages",
                  "messages_written", "messagesWritten" });
    progress.messagesTotal = countMember(
        update, { "messages_total", "messagesTotal", "total_messages", "totalMessages" });
    progress.filesExported = countMember(
        update, { "files_exported", "filesExported", "exported_files", "media_copied",
                  "mediaCopied", "files_copied" });

    progress.done = boolMember(
        update, { "done", "is_done", "finished", "completed", "is_finished" });
    if (!progress.done)
        progress.done = terminalStatus(update);
    if (!progress.done)
        progress.done = fileTransferCompleted(update);

    progress.error = errorText(
        progressMember(update, { "error", "error_message", "errorMessage" }));
    if (progress.error.isEmpty()
        && update.value(QStringLiteral("@type")).toString() == QLatin1String("error")) {
        progress.error = update.value(QStringLiteral("message")).toString().trimmed();
    }
    if (progress.error.isEmpty()) {
        const QJsonObject file = update.value(QStringLiteral("file")).toObject();
        progress.error = errorText(file.value(QStringLiteral("error")));
        if (progress.error.isEmpty()) {
            progress.error = errorText(
                file.value(QStringLiteral("local")).toObject().value(QStringLiteral("error")));
        }
    }

    return progress;
}

} // namespace beygram::telegram
