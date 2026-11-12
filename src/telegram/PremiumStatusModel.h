#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::telegram {

/// The Premium status of the signed-in account: whether it is Premium, when
/// that expires and which Premium features the server listed.
///
/// Reuse note: `PremiumRequests.h` was read first and it does **not** declare a
/// `PremiumState` type — it declares only `PremiumFeature`, plus the request
/// builders and `featuresFromResponse()`, which parses a `PremiumFeatures`
/// object into a `QVector<PremiumFeature>`. There is therefore nothing to
/// reuse or embed here, and this model stores the three fields itself, exactly
/// as its declared private members require. Feeding it from a
/// `getPremiumFeatures` answer means mapping the payload to keys yourself:
/// `PremiumFeature::title` is a display label (the server title, or the spaced
/// `@type` name), not a stable key, so a caller that wants hasFeature() to
/// answer for `premiumFeatureDisabledAds` should pass the `@type` strings, not
/// the titles. The two types stay independent so this model needs no TDLib
/// payload to be tested.
///
/// The model never talks to TDLib, never logs and never reads the clock: the
/// caller passes a timestamp to setPremium() and an explicit `nowUnix` to
/// daysLeft(), so the tests are deterministic.
///
/// Invariants:
///
///  - An expiry only exists while the account is Premium. setPremium(false)
///    stores an expiry of 0 regardless of what was passed, so "not Premium"
///    can never be paired with a leftover date that a caller might trust.
///  - A negative expiry is stored as 0. 0 means "no known expiry" (a lifetime
///    subscription, or a status TDLib reported without one); it is never
///    treated as the Unix epoch.
///  - Feature keys are trimmed, empty keys are dropped, and duplicates are
///    removed keeping the first occurrence, so the list keeps the caller's
///    order and cannot render a blank or doubled row. Duplicates are detected
///    case-insensitively because hasFeature() is case-insensitive: two keys
///    that differ only in case would be indistinguishable to it.
///  - hasFeature() compares case-insensitively and ignores surrounding
///    whitespace, and an empty query always answers false.
///  - changed() is emitted only when the stored values really differ.
///    Re-sending the same status, re-sending the same features, and clearing an
///    already-clear model are all silent, so a refresh that brings the same
///    answer does not repaint the screen.
class PremiumStatusModel final : public QObject {
    Q_OBJECT

public:
    explicit PremiumStatusModel(QObject* parent = nullptr);

    /// Stores whether the account is Premium and, when it is, when that ends
    /// (`expiresAt`, a Unix timestamp in seconds). The expiry is kept only for
    /// a Premium account: `premium == false` forces it to 0, and a negative
    /// value becomes 0. changed() is emitted only when the resulting pair
    /// differs from the stored one.
    void setPremium(bool premium, qint64 expiresAt = 0);

    /// Whether the account is Premium. Reflects the stored flag only; it is not
    /// re-evaluated against the clock, so an elapsed expiry still reports true
    /// until the caller stores the new state.
    [[nodiscard]] bool premium() const;

    /// When Premium ends, as a Unix timestamp in seconds. 0 when the account is
    /// not Premium, when no expiry is known, or when the caller passed a
    /// negative value.
    [[nodiscard]] qint64 expiresAt() const;

    /// Replaces the feature keys with `features`, in the given order. Each key
    /// is trimmed, empty keys are dropped and later duplicates of a key (the
    /// first occurrence wins, compared case-insensitively) are removed.
    /// changed() is emitted only when the resulting list differs from the
    /// current one.
    void setFeatures(QStringList features);

    /// A copy of the feature keys, in display order.
    [[nodiscard]] QStringList features() const;

    /// Whether `feature` is in the list, compared case-insensitively and
    /// ignoring surrounding whitespace. False for an empty query.
    ///
    /// Only the stored keys are consulted: setPremium() never touches them, so
    /// features survive a status change until clear() or a new setFeatures()
    /// call. A caller for which a feature only counts on a paying account must
    /// check premium() as well.
    [[nodiscard]] bool hasFeature(const QString& feature) const;

    /// Whole days until `expiresAt`, rounded up, so a subscription with one
    /// hour left reports 1 rather than 0. 0 when the account is not Premium,
    /// when no expiry is known (`expiresAt() == 0`) or when the expiry has
    /// already passed, so the result is never negative and a UI can always
    /// render "N days left".
    [[nodiscard]] int daysLeft(qint64 nowUnix) const;

    /// The label to show next to the account name: "Premium" while the stored
    /// flag is set, otherwise an empty string. The clock is not consulted (this
    /// method takes no `now`), so an elapsed expiry keeps the badge until the
    /// caller stores the new state; use daysLeft() for the remaining time.
    [[nodiscard]] QString badgeText() const;

    /// Forgets the status, the expiry and every feature key, e.g. when the
    /// account is signed out. Emits changed() only if there was something to
    /// forget.
    void clear();

signals:
    /// The stored Premium status changed. Not emitted for a no-op call.
    void changed();

private:
    bool m_premium = false;
    qint64 m_expires = 0;
    QStringList m_features;
};

} // namespace beygram::telegram
