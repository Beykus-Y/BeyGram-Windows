#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::telegram {

/// Pure builders for blocking senders and reporting messages. They contain no
/// logging and no secrets; the transport adds correlation.
///
/// Shapes follow the TDLib schema pinned by this client (1.8.69, commit
/// 4c6377aa, see docs/WINDOWS_TDLIB.md):
///
///   messageSenderUser user_id:int53 = MessageSender;
///   messageSenderChat chat_id:int53 = MessageSender;
///   blockListMain = BlockList;
///   blockListStories = BlockList;
///   setMessageSenderBlockList sender_id:MessageSender block_list:BlockList = Ok;
///   reportChat chat_id:int53 option_id:bytes message_ids:vector<int53>
///              text:string = ReportChatResult;
///   getBlockedMessageSenders block_list:BlockList offset:int32 limit:int32
///                           = MessageSenders;
///   blockMessageSenderFromReplies message_id:int53 delete_message:Bool
///              delete_all_messages:Bool report_spam:Bool = Ok;
///
/// This revision has no toggleMessageSenderIsBlocked and no reportMessage.
/// Blocking is setMessageSenderBlockList, whose nullable block_list unblocks the
/// sender (BlockList only has the Main and Stories variants, so there is no
/// "none" object to send), and a single message is reported with reportChat
/// carrying message_ids.

/// Builds the messageSenderUser variant of MessageSender. user_id is an int53
/// user identifier and must be positive; the builder does not guess the variant
/// from the sign of the id, so a chat id can never leak into user_id.
[[nodiscard]] QJsonObject messageSenderUser(qint64 userId);

/// Builds the messageSenderChat variant of MessageSender. chat_id identifies a
/// supergroup or channel and is negative in TDLib; only zero is invalid.
[[nodiscard]] QJsonObject messageSenderChat(qint64 chatId);

/// Builds setMessageSenderBlockList for a user sender. blocked=true sends
/// blockListMain, blocked=false sends the null block_list that unblocks.
[[nodiscard]] QJsonObject blockSenderRequest(qint64 senderUserId, bool blocked);

/// Builds setMessageSenderBlockList for a chat sender (supergroup or channel).
/// TDLib only blocks users and supergroup chats; a basic group chat id is
/// rejected by the server, not by this builder.
[[nodiscard]] QJsonObject blockSenderRequestChat(qint64 senderChatId, bool blocked);

/// Every ReportReason @type this schema defines, in schema order. Telegram has
/// moved message reports to server-supplied options (reportOption), so these
/// names are matched against the option list rather than sent as a field.
[[nodiscard]] QStringList reportReasonTypes();

/// Maps a reason string to its ReportReason @type. Accepts the schema name
/// ("reportReasonSpam") as well as a loose spelling of it ("spam", "Spam",
/// "personal_details", "personalDetails"), case-insensitively. Returns an empty
/// string when the reason is unknown, so a typo is never sent as a reason.
[[nodiscard]] QString reportReasonType(const QString& reason);

/// Builds the first request of the two-step message report. This schema has no
/// reportMessage and reportChat has no ReportReason field: Telegram answers the
/// first request with reportChatResultOptionRequired and the caller resends the
/// chosen option through reportMessageOptionRequest.
///
/// A known reason (see reportReasonType) is therefore selected from Telegram's
/// own option list afterwards and is not written into the request. A free-form
/// reason is sent as the optional report details text, clamped to the schema's
/// 1024-character limit. option_id stays empty and message_ids carries the one
/// reported message, which is what tells TDLib this is a message report and not
/// a chat report.
[[nodiscard]] QJsonObject reportMessageRequest(qint64 chatId, qint64 messageId,
                                               const QString& reason);

/// Builds the follow-up reportChat for the option Telegram offered. optionId is
/// the reportOption.id taken verbatim from the reportChatResultOptionRequired
/// answer: the JSON interface carries bytes base64-encoded, so the value is
/// passed through unchanged and an empty optionId repeats the initial request.
/// text carries the details asked for by reportChatResultTextRequired and stays
/// empty when the caller only confirms an optional text step.
[[nodiscard]] QJsonObject reportMessageOptionRequest(qint64 chatId, qint64 messageId,
                                                     const QString& optionId,
                                                     const QString& text = QString());

/// Picks the reportOption.id matching a reason from a
/// reportChatResultOptionRequired answer. Telegram localises option texts, so
/// both the decoded option id and the text are matched on keywords for the
/// reason; when the option list has no matching entry the result is empty and
/// the caller must not substitute an unrelated reason.
[[nodiscard]] QString reportOptionIdForReason(const QJsonArray& options,
                                              const QString& reason);

/// Builds getBlockedMessageSenders for the main block list. The schema requires
/// a concrete block_list, and this client has no stories block list yet, so
/// blockListMain is always sent. offset is clamped to a non-negative value and
/// limit to 1..100, the range TDLib accepts.
[[nodiscard]] QJsonObject getBlockedSendersRequest(int offset, int limit);

/// True when the response is a TDLib messageSenders object, i.e. the answer to
/// getBlockedMessageSenders.
[[nodiscard]] bool responseIsMessageSenders(const QJsonObject& response);

/// Builds blockMessageSenderFromReplies, which blocks the original sender of an
/// incoming message in the Replies chat. messageId is a message id in that chat,
/// not in the original conversation.
[[nodiscard]] QJsonObject blockSenderFromRepliesRequest(qint64 messageId,
                                                        bool deleteMessage,
                                                        bool deleteAllMessages,
                                                        bool reportSpam);

} // namespace beygram::telegram
