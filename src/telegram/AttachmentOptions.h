#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The attachment types the composer's attachment sheet offers.
///
/// The order of the enumerators is the display order of
/// defaultAttachmentOptions(), so the sheet does not need its own ordering
/// table. The values are persisted by name (attachmentKindName), never by
/// number, so new kinds may be inserted without rewriting stored settings.
enum class AttachmentKind { Photo, Video, Document, Audio, Location, Contact, Poll, Sticker };

/// One tile of the attachment sheet.
///
/// kind is the attachment type the tile opens. title is the user-facing label
/// and glyph a short decorative symbol; both come from the defaults and may be
/// replaced by the UI (for example with a translated title or an icon name).
/// enabled is false when the current chat or account does not allow the kind,
/// so the tile can be shown greyed out instead of disappearing.
struct AttachmentOption {
    AttachmentKind kind = AttachmentKind::Photo;
    QString title;
    QString glyph;
    bool enabled = true;
};

/// The attachment sheet in display order: one enabled tile for every kind,
/// with English titles. The returned vector is a copy, so the caller may
/// translate or disable entries without changing later calls.
[[nodiscard]] QVector<AttachmentOption> defaultAttachmentOptions();

/// The stable name of a kind, used for settings and logs: "photo", "video",
/// "document", "audio", "location", "contact", "poll" or "sticker".
///
/// A value outside the enum has no name and yields an empty string. The names
/// are lowercase ASCII, so they survive a settings file without escaping.
[[nodiscard]] QString attachmentKindName(AttachmentKind kind);

/// Parses a name produced by attachmentKindName().
///
/// Matching ignores case and surrounding whitespace: a name that went through
/// a settings file or a user-editable config should still resolve. An unknown
/// name returns AttachmentKind::Photo and stores false in *ok (when ok is not
/// null), which lets the caller keep a safe default while noticing the bad
/// value. A null ok is accepted when the caller does not care.
[[nodiscard]] AttachmentKind attachmentKindFromName(const QString& name, bool* ok = nullptr);

/// The file-dialog filter for a kind, in the space-separated "*.jpg *.png"
/// form QFileDialog expects.
///
/// Document accepts any file ("*"). Location, Contact, Poll and Sticker are not
/// chosen from a file, so they return an empty string; a caller must not pass
/// that to a file dialog. A value outside the enum also returns an empty
/// string.
[[nodiscard]] QString attachmentAcceptFilter(AttachmentKind kind);

/// Builds the input_message_content for a location message:
///
///   inputMessageLocation location:location = InputMessageContent;
///   location latitude:double longitude:double horizontal_accuracy:double
///            = Location;
///
/// The result is a content fragment, not a sendMessage request: the caller
/// adds chat_id and the other sendMessage fields. Coordinates are sent as
/// given and horizontal_accuracy stays 0 ("unknown"), so Telegram treats the
/// message as a static location with its defaults. Live locations use a
/// different content type (inputMessageLiveLocation), not this builder.
///
/// A latitude outside -90..90, a longitude outside -180..180 or a non-finite
/// value (NaN, infinity) is not a location TDLib could accept, so the builder
/// returns an empty object instead. The bounds themselves are inclusive.
[[nodiscard]] QJsonObject locationMessageRequest(double latitude, double longitude);

/// Builds the input_message_content for a contact message:
///
///   inputMessageContact contact:contact = InputMessageContent;
///   contact phone_number:string first_name:string last_name:string
///           vcard:string user_id:int53 = Contact;
///
/// The result is a content fragment, not a sendMessage request: the caller
/// adds chat_id and the other sendMessage fields. Only the user id is known
/// here, so phone_number, first_name, last_name and vcard are empty; a caller
/// that has the user cached should fill them in before sending, because the
/// server receives the text fields while user_id only keeps the card linked
/// to that user. userId must be a real user id, so a non-positive value
/// returns an empty object.
[[nodiscard]] QJsonObject contactMessageRequest(qint64 userId);

} // namespace beygram::telegram
