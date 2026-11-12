#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One reason offered for reporting a message or a chat photo.
///
/// `id` is kept exactly as Telegram supplied it so it can be sent back as an
/// opaque option id; it is never guessed from `title`. For a TDLib reportOption
/// the id is a base64-encoded byte string. `title` is the localised label.
/// `description` and `requiresComment` are optional extras: TDLib's reportOption
/// carries only id and text, while a richer server response may also describe a
/// reason and say that a comment is mandatory.
struct ReportReason {
    QString id;
    QString title;
    QString description;
    bool requiresComment = false;
};

/// Projects the report options of a TDLib reportChatResultOptionRequired answer
/// (or a response that lists the same options under `reasons`) onto ReportReason.
///
/// The option list is read from `options`, falling back to `reasons`, and a
/// nested `result` object is unwrapped first. Each option is read as:
///
///   id              from "id", then "reason_id"
///   title           from "title", then "text", then "label"
///   description     from "description", then "subtitle"
///   requiresComment from "requires_comment", then "requiresComment"
///
/// Strings are trimmed. An option whose id or title is empty after trimming is
/// skipped, as is any entry that is not an object. The input order of the
/// surviving reasons is preserved; duplicate ids are not removed. A response
/// without a usable option list yields an empty vector. Never logs and never
/// throws.
[[nodiscard]] QVector<ReportReason> reportReasonsFromOptions(const QJsonObject& response);

/// Builds the first step of the two-step report flow: a reportChat request with
/// an empty option_id and empty text, which makes Telegram answer with
/// reportChatResultOptionRequired (the report options) instead of performing the
/// report. TDLib has no separate getReportChatOptions function, so this is the
/// actual request that asks for the options.
///
/// chatId must be positive, otherwise the builder returns an empty object.
/// messageIds are normalized: non-positive ids are dropped, duplicates keep
/// their first position, and at most 100 ids are kept. An empty list is valid
/// and asks for the options of reporting the chat as a whole.
[[nodiscard]] QJsonObject getReportChatOptionsRequest(qint64 chatId,
                                                     const QVector<qint64>& messageIds);

/// Builds the second step of the message report: reportChat with the option
/// chosen from reportReasonsFromOptions(). reasonId is trimmed and then passed
/// through unchanged (TDLib's JSON interface carries bytes base64-encoded, so a
/// value taken from ReportReason::id must not be re-encoded). comment is the
/// optional report details text, trimmed and clamped to the schema's
/// 1024-character limit; an empty comment is sent as an empty string.
///
/// chatId must be positive and reasonId must not be empty after trimming,
/// otherwise the builder returns an empty object. messageIds follow the same
/// positive/deduped/capped rules as getReportChatOptionsRequest(); an empty list
/// reports the whole chat.
[[nodiscard]] QJsonObject reportChatRequest(qint64 chatId,
                                            const QVector<qint64>& messageIds,
                                            const QString& reasonId,
                                            const QString& comment);

/// Builds reportChatPhoto. reasonId is the ReportReason @type that names the
/// reason, for example "reportReasonSpam"; it is sent as the @type of the
/// nested `reason` object, which is the shape TDLib defines for this request
/// (reportChatPhoto has no option_id field). comment is trimmed and clamped to
/// 1024 characters.
///
/// chatId and fileId must be positive and reasonId must not be empty after
/// trimming, otherwise the builder returns an empty object.
[[nodiscard]] QJsonObject reportChatPhotoRequest(qint64 chatId, qint64 fileId,
                                                 const QString& reasonId,
                                                 const QString& comment);

/// Builds reportSupergroupSpam, which reports messages to the supergroup's
/// administrators. supergroupId is the positive supergroup identifier, not the
/// negative chat id. messageIds are normalized like the other builders; because
/// this request reports concrete messages, an empty normalized list (or a
/// non-positive supergroupId) makes the builder return an empty object.
[[nodiscard]] QJsonObject reportSupergroupSpamRequest(qint64 supergroupId,
                                                      const QVector<qint64>& messageIds);

/// Builds reportSupergroupAntiSpamFalsePositive, which tells the supergroup's
/// administrators that a message was deleted by anti-spam by mistake.
/// supergroupId and messageId must both be positive, otherwise the builder
/// returns an empty object.
[[nodiscard]] QJsonObject reportSupergroupAntiSpamFalsePositiveRequest(qint64 supergroupId,
                                                                       qint64 messageId);

} // namespace beygram::telegram
