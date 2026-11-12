#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// What the current account may do with one message, as reported by TDLib.
/// The defaults describe a message whose properties are unknown: nothing is
/// granted except local clipboard copying, which needs no server permission.
struct MessageProperties {
    bool canBeForwarded = false;
    bool canBeSaved = false;
    bool canBeCopied = true;
    bool canBeDeletedOnlyForSelf = false;
    bool canBeDeletedForAllUsers = false;
    bool canGetAddedReactions = false;
};

/// Builds the TDLib request that returns the properties of one message. It is
/// an offline method: TDLib answers from its local database and the request
/// needs no network round-trip. No logging, no secrets and no @extra; the
/// transport adds correlation.
///
/// Shape follows td_api.tl:
///   getMessageProperties chat_id:int53 message_id:int53 = MessageProperties;
///
/// The response is parsed with propertiesFromResponse.
[[nodiscard]] QJsonObject getMessagePropertiesRequest(qint64 chatId, qint64 messageId);

/// Parses a getMessageProperties answer.
///
///   messageProperties can_be_forwarded:Bool can_be_saved:Bool
///                     can_be_copied:Bool
///                     can_be_deleted_only_for_self:Bool
///                     can_be_deleted_for_all_users:Bool
///                     can_get_added_reactions:Bool;
///
/// Fields are read only when TDLib actually sends them, so a partial object
/// keeps the defaults above. can_get_added_reactions is absent from newer
/// revisions of the schema and then stays false. The newer
/// has_protected_content_by_current_user/has_protected_content_by_other_user
/// flags also withdraw canBeSaved, because protected content must never be
/// written to disk.
///
/// A TDLib error answer (@type "error") yields the default value: no action is
/// granted beyond the local copy, so a failed lookup can never enable a
/// destructive or forwarding action.
[[nodiscard]] MessageProperties propertiesFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
