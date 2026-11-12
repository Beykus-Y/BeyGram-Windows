#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of the Telegram Premium feature list.
///
/// The pinned td_api.tl (TDLib 1.8.69) models PremiumFeature as an abstract
/// class whose subtypes carry no payload at all:
///   premiumFeatureIncreasedLimits = PremiumFeature;
///   premiumFeatureDisabledAds = PremiumFeature;
///   ...
/// so a real response only names the feature (@type) and every title is a
/// client string. title therefore keeps a server-provided title when a payload
/// carries one and otherwise falls back to the @type name, with the class
/// prefix dropped and the camel-case words spaced
/// ("premiumFeatureDisabledAds" -> "Disabled Ads"). description stays empty
/// unless the payload carries one, and available stays true unless the payload
/// denies it explicitly: the schema has no availability flag, and a feature
/// TDLib lists is available.
struct PremiumFeature {
    QString title;
    QString description;
    bool available = true;
};

/// Builds getPremiumPromo = PremiumPromo.
///
/// The pinned td_api.tl (TDLib 1.8.69, commit 4c6377a) no longer declares this
/// method: it was dropped after 1.7.x, and its work is split between
/// getPremiumState (status text, payment options, promotion animations) and
/// getPremiumFeatures. The request is still built verbatim so a caller that
/// targets an older TDLib, or that probes for support, sends the exact legacy
/// call; a pinned 1.8.69 server answers it with 400 "Method not found", which
/// the caller must treat as "this build has no promo endpoint", not as a
/// network failure.
[[nodiscard]] QJsonObject getPremiumPromoRequest();

/// Builds getPremiumFeatures source:PremiumSource = PremiumFeatures.
///
/// source is a required member in the pinned schema, so this builder always
/// sends premiumSourceSettings — the source TDLib reports when the feature list
/// is opened from Settings, which is what a plain "show me Premium" screen is.
/// A screen opened from a link, an exceeded limit or a single feature needs the
/// matching source and therefore cannot use this parameter-less builder.
[[nodiscard]] QJsonObject getPremiumFeaturesRequest();

/// Builds canPurchasePremium purpose:PremiumSource = Ok, Telegram's answer to
/// "may this account subscribe at all".
///
/// The pinned td_api.tl (TDLib 1.8.69) no longer declares this method either:
/// the remaining store check is canPurchaseFromStore
/// purpose:StorePaymentPurpose = Ok, and purchase eligibility is implied by
/// getPremiumState's payment_options. As with getPremiumPromoRequest(), the
/// legacy request is built verbatim and a pinned server rejects it with
/// 400 "Method not found".
///
/// TDLib rejects a bare string where a PremiumSource object is expected, so the
/// purpose token is mapped to the matching source:
///   "" or "settings"      -> premiumSourceSettings
///   "link[:<referrer>]"   -> premiumSourceLink
///   "feature:<type>"      -> premiumSourceFeature with that premiumFeature*
///   "limit:<type>"        -> premiumSourceLimitExceeded with that premiumLimitType*
///   "story:<type>"        -> premiumSourceStoryFeature with that premiumStoryFeature*
///   "business[:<type>]"   -> premiumSourceBusinessFeature, feature null without a type
///   "premiumFeature<...>" -> premiumSourceFeature (a bare type names a feature)
///   anything else         -> premiumSourceLink with the token as referrer
///
/// Keywords are matched case-insensitively and the value keeps its case. A
/// "feature:", "limit:" or "story:" prefix without a type cannot be expressed
/// as a source, so it degrades to premiumSourceSettings instead of sending an
/// empty @type. The catch-all keeps an unrecognised token as a link referrer,
/// which TDLib accepts verbatim, rather than discarding what the caller passed.
[[nodiscard]] QJsonObject canPurchasePremiumRequest(const QString& purpose);

/// Parses the PremiumFeatures object returned by getPremiumFeatures:
///   premiumFeatures features:vector<PremiumFeature>
///                  limits:vector<premiumLimit>
///                  payment_link:InternalLinkType = PremiumFeatures;
///
/// Only features is read: limits are a separate PremiumLimit list with their
/// own default and premium values, and payment_link is an InternalLinkType for
/// the caller to resolve, so neither belongs in this list.
///
/// Entries keep TDLib's order and are not deduplicated, because TDLib already
/// sends each feature once and the caller may rely on the server order. A
/// non-object, non-string entry, and an object that names neither a @type nor a
/// title, is skipped: it would render as a blank row. See PremiumFeature for
/// how title, description and available are filled in when the payload omits
/// them.
[[nodiscard]] QVector<PremiumFeature> featuresFromResponse(const QJsonObject& response);

} // namespace beygram::telegram
