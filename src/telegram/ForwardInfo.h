#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Everything needed to render "Forwarded from ..." for a message. Parsed from
/// TDLib without touching the UI, so the model layer can hold it directly.
///
/// Shape follows td_api.tl:
///   messageForwardInfo origin:MessageForwardOrigin date:int32
///                      source:MessageSource public_service_announcement_type:string;
///   messageForwardOriginUser         sender_user_id:int53;
///   messageForwardOriginHiddenUser   sender_name:string;
///   messageForwardOriginChat         sender_chat_id:int53 author_signature:string;
///   messageForwardOriginChannel      chat_id:int53 message_id:int53
///                                    author_signature:string;
///   messageForwardOriginAnonymous;
///
/// Names are never synthesized here: a field stays empty when TDLib does not
/// carry it, and the caller resolves display names from the chat/user registry.
struct ForwardInfo {
    enum class Origin { None, User, HiddenUser, Chat, Channel, Anonymous };

    Origin origin = Origin::None;
    qint64 sourceChatId = 0;
    qint64 sourceMessageId = 0;
    qint64 senderUserId = 0;
    QString senderName;      // only hidden-user forwards carry a name
    QString chatTitle;       // TDLib sends no title; resolved by the caller
    QString authorSignature; // channel posts and chat forwards
    qint64 date = 0;
};

/// Parses message["forward_info"] from a TDLib message object. Returns a
/// default-constructed (Origin::None) value when the message is not a forward.
[[nodiscard]] ForwardInfo forwardInfoFromMessage(const QJsonObject& message);

} // namespace beygram::telegram
