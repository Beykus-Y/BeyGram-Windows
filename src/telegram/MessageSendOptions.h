#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// User-facing switches for an outgoing message. They are translated into a
/// TDLib messageSendOptions object by messageSendOptionsObject.
struct SendOptions {
    bool silent = false;          ///< disable_notification
    bool background = false;      ///< from_background
    bool protectContent = false;  ///< protect_content
    qint64 scheduleDate = 0;      ///< Unix seconds; <= 0 means "send now"
    bool sendWhenOnline = false;  ///< schedule for the peer's next online moment
};

/// Builds a schema-complete TDLib messageSendOptions object.
///
/// Shape follows td_api.tl:
///   messageSendOptions suggested_post_info:inputSuggestedPostInfo
///       disable_notification:Bool from_background:Bool protect_content:Bool
///       allow_paid_broadcast:Bool paid_message_star_count:int53
///       update_order_of_installed_sticker_sets:Bool
///       scheduling_state:MessageSchedulingState effect_id:int64
///       sending_id:int32 only_preview:Bool = MessageSendOptions;
///
/// Fields this application does not expose are still emitted with their
/// neutral values, because TDLib expects every field of the constructor:
/// suggested_post_info is null, allow_paid_broadcast and
/// update_order_of_installed_sticker_sets are false, paid_message_star_count
/// and effect_id are 0, sending_id is 0 and only_preview is false.
///
/// scheduling_state is messageSchedulingStateSendAtDate when scheduleDate is
/// positive, otherwise messageSchedulingStateSendWhenOnline when
/// sendWhenOnline is set, otherwise null (send immediately). A positive
/// scheduleDate wins over sendWhenOnline because the two states are mutually
/// exclusive. The builder performs no logging and adds no @extra; the
/// transport adds correlation.
[[nodiscard]] QJsonObject messageSendOptionsObject(const SendOptions& options);

/// Returns a copy of a TDLib request with its "options" field replaced by
/// messageSendOptionsObject(options). Every other field is left untouched.
/// The caller passes a request whose schema has an options:messageSendOptions
/// field, such as sendMessage, sendMessageAlbum or forwardMessages; the
/// function does not inspect @type. An empty request is returned unchanged
/// instead of growing an options object on its own.
[[nodiscard]] QJsonObject withSendOptions(QJsonObject request,
                                          const SendOptions& options);

} // namespace beygram::telegram
