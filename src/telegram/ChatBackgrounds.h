#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of a backgrounds catalog, reduced to what the background picker
/// needs.
///
/// id is the unique background identifier and is always positive for a stored
/// entry. name is the background's unique name, trimmed of surrounding
/// whitespace. isDefault marks the entries Telegram ships as defaults, and
/// isDark marks a background that is meant for the dark theme. fileId is the
/// file id of background.document.file (0 when the background has no document,
/// as filled backgrounds do, or when the id is not a usable int32).
struct BackgroundInfo {
    qint64 id = 0;
    QString name;
    bool isDefault = false;
    bool isDark = false;
    int fileId = 0;
};

/// Builds the request for the list of installed backgrounds.
///
/// forDarkTheme is sent as for_dark_theme and asks the server to order the
/// answer for the dark theme; the same list is returned either way.
[[nodiscard]] QJsonObject getBackgroundsRequest(bool forDarkTheme);

/// Builds the request for a persistent URL of one named background.
///
/// name is trimmed before it is sent; an empty name is not addressable and
/// yields an empty object. type is one of the four background kinds the app
/// paints: "wallpaper", "pattern", "solid" or "gradient". The spelling is
/// case-insensitive and surrounding whitespace is ignored; the canonical
/// lower-case name is sent. Any other type yields an empty object, so a caller
/// never sends a kind the renderer cannot draw.
[[nodiscard]] QJsonObject getBackgroundUrlRequest(const QString& name,
                                                  const QString& type);

/// Builds the request that applies a background.
///
/// backgroundId must be positive; 0 and negative ids are not addressable and
/// yield an empty object. chatId 0 means the global/default background used by
/// chats without an override; negative ids are real chats (groups and
/// channels) and are sent unchanged, as are positive private-chat ids.
[[nodiscard]] QJsonObject setBackgroundRequest(qint64 backgroundId,
                                               qint64 chatId);

/// Builds the request that chooses how backgrounds of a theme are painted.
///
/// forDarkTheme selects the light or the dark theme. type accepts the same four
/// canonical names as getBackgroundUrlRequest; the spelling is normalized and
/// an unknown or blank type yields an empty object.
[[nodiscard]] QJsonObject setBackgroundTypeRequest(bool forDarkTheme,
                                                   const QString& type);

/// Reads one background object from a backgrounds response.
///
/// A background needs a positive id; every other field is optional metadata, so
/// an object without one, a non-object, or an object with a zero, negative or
/// non-numeric id reports a default-constructed BackgroundInfo whose id is 0.
/// The caller skips such an entry. name is trimmed, is_default and is_dark are
/// read as booleans, and fileId comes from document.file.id; a missing document
/// or a file id outside the int32 range reports 0 instead of a truncated id.
[[nodiscard]] BackgroundInfo backgroundFromJson(const QJsonObject& background);

} // namespace beygram::telegram
