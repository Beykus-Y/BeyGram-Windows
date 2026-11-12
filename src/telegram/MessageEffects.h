#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of the message-effect catalog that a composer offers.
///
/// id is the TDLib effect id that is sent back when the effect is applied to a
/// message; it is always positive, because the parser drops entries that carry
/// no usable id. emoji is the glyph the effect plays and title its display
/// name; both are trimmed and either may be empty when TDLib omits it.
/// premiumOnly mirrors is_premium: the effect can only be sent from an account
/// with Telegram Premium, so a caller may show it locked instead of hiding it.
struct MessageEffect {
    qint64 id = 0;
    QString emoji;
    QString title;
    bool premiumOnly = false;
};

/// Pure builders and parsers for TDLib message effects. They contain no
/// logging, no secrets and no @extra; the transport adds correlation.

/// Builds the request that returns the catalog of effects available to the
/// current account. It takes no arguments, so the object carries only its
/// @type:
///
///   getMessageAvailableEffects
///
/// The answer lists the effects under an "effects" array; it is parsed with
/// messageEffectsFromResponse.
[[nodiscard]] QJsonObject getMessageAvailableEffectsRequest();

/// Builds getMessageEffect for one catalog entry. The answer describes the
/// stickers that draw the effect and is fetched only when an effect is about to
/// play, so browsing the catalog costs no request:
///
///   getMessageEffect effect_id:int64 = MessageEffect;
///
/// An effectId <= 0 is not an effect (0 is TDLib's "none" value), so the
/// builder returns an empty object rather than a request TDLib would reject.
[[nodiscard]] QJsonObject getMessageEffectRequest(qint64 effectId);

/// Parses a getMessageAvailableEffects answer into catalog entries.
///
/// Only the fields MessageEffect keeps are read from each entry:
///   id:int64 emoji:string title:string is_premium:Bool
/// A response without an "effects" array yields an empty vector, and an entry
/// that is not an object or has no positive id (a null entry, a number, a
/// trimmed wrapper, a negative or zero id) is skipped without discarding its
/// neighbours. The catalog order is preserved. Duplicate ids keep their first
/// occurrence, so a later copy can never replace a title the user already saw.
[[nodiscard]] QVector<MessageEffect> messageEffectsFromResponse(
    const QJsonObject& response);

/// Builds the messageEffect object that a send option carries:
///
///   messageEffect id:int64 = MessageEffect;
///
/// effectId <= 0 means "no effect", so the builder returns an empty object that
/// the caller omits or sends as null instead of naming effect id 0.
[[nodiscard]] QJsonObject messageEffectObject(qint64 effectId);

} // namespace beygram::telegram
