#pragma once

#include "telegram/PremiumStatusModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// The Premium card as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the
/// Premium flag, its expiry and the feature keys live in the
/// `PremiumStatusModel` this class owns, and premium(), badge(), expiryText()
/// and the key fallback of features() read straight from it. The facade only
/// does what QML cannot: it formats the countdown, and it turns the
/// title/description pairs a `getPremiumFeatures` answer is rendered from into
/// the single display string a row shows.
///
/// Formatting follows the model so the screen has one voice:
///   - badge() is `PremiumStatusModel::badgeText()`, i.e. "Premium" while the
///     account is Premium and empty otherwise.
///   - expiryText() is empty when the account is not Premium, "No expiry" when
///     it is Premium without a known expiry, "Expires today" when
///     `daysLeft()` is 0, and "<n> days left" otherwise, e.g. "30 days left".
///     The pattern is always the same, including n == 1, so the line never
///     changes shape.
///   - features() is one "<title> — <description>" row per catalog pair when a
///     catalog was set, and the raw keys from the model otherwise.
///
/// Feature keys and the feature catalog are deliberately separate:
/// `setFeatures()` stores the `@type`-style keys the model's hasFeature() is
/// asked about, while `setFeatureCatalog()` stores the human-readable rows the
/// card paints. Both setters replace the display list, and calling
/// `setFeatures()` drops a previously set catalog, so a new server list is
/// never shadowed by the display text built for the old one. Set the catalog
/// after the keys, i.e. keys first, then titles.
///
/// changed() is emitted only when the visible state really differs. The facade
/// compares the flag, the expiry timestamp, the display list and the loading
/// flag, so a refresh that brings the same answer does not repaint the card.
/// expiryText() reads the clock, so the countdown can move on between two
/// emissions; a caller that wants the card to tick must refresh the status or
/// re-read the property, exactly like the model itself.
///
/// The facade never talks to TDLib, never logs, never reads storage and never
/// starts a purchase. upgrade() only announces the intent through
/// upgradeRequested() for the page to act on.
class PremiumFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool premium READ premium NOTIFY changed)
    Q_PROPERTY(QString badge READ badge NOTIFY changed)
    Q_PROPERTY(QString expiryText READ expiryText NOTIFY changed)
    Q_PROPERTY(QStringList features READ features NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)

public:
    explicit PremiumFacade(QObject* parent = nullptr);

    /// Whether the account is Premium. Reflects the stored flag only; an
    /// elapsed expiry still reports true until setStatus() stores the new
    /// state, as PremiumStatusModel::premium() specifies.
    [[nodiscard]] bool premium() const;

    /// "Premium" while premium() is true, otherwise empty.
    [[nodiscard]] QString badge() const;

    /// The countdown line for the card: empty when not Premium, "No expiry"
    /// when no expiry is known, "Expires today" when `daysLeft()` is 0, and
    /// "<n> days left" otherwise. An expiry that has already passed also reads
    /// "Expires today", because the model never reports a negative count.
    [[nodiscard]] QString expiryText() const;

    /// The rows to paint: the catalog when one is set, the model's feature
    /// keys otherwise. Never empty just because a catalog is missing.
    [[nodiscard]] QStringList features() const;

    /// True while the caller is waiting for a Premium answer. The card uses it
    /// to show its progress state and to keep the Upgrade button disabled.
    [[nodiscard]] bool loading() const;

    /// Stores whether the account is Premium and, when it is, when that ends
    /// (`expiresAt`, seconds since the Unix epoch). The model keeps an expiry
    /// only for a Premium account and stores a negative one as 0.
    Q_INVOKABLE void setStatus(bool premium, qint64 expiresAt);

    /// Replaces the feature keys, in the given order, and drops a previously
    /// set catalog so its display text cannot outlive the list it described.
    /// The model trims keys, drops empty ones and removes later duplicates.
    Q_INVOKABLE void setFeatures(const QStringList& features);

    /// Sets the progress state. Re-sending the current value is silent.
    Q_INVOKABLE void setLoading(bool loading);

    /// Replaces the display rows from parallel title and description lists,
    /// read positionally: "<title> — <description>", or just "<title>" when
    /// the description is empty or missing. Both sides are trimmed, and a pair
    /// without a title is skipped because a description alone has no row to
    /// label. The lists may be shorter than titles; an extra title simply has
    /// no description. The feature keys are not touched.
    Q_INVOKABLE void setFeatureCatalog(const QStringList& titles,
                                       const QStringList& descriptions);

    /// Forgets the status, the expiry, the feature keys, the catalog and the
    /// progress state, e.g. when the account is signed out. Emits changed()
    /// once if there was anything to forget.
    Q_INVOKABLE void clear();

    /// Announces that the user asked to upgrade. Emits upgradeRequested() and
    /// changes nothing else, so a failed purchase leaves the card as it was.
    Q_INVOKABLE void upgrade();

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted by upgrade() for the page to open the Premium purchase flow.
    void upgradeRequested();

private:
    /// The state changed() is diffed against. The raw expiry is kept rather
    /// than expiryText(), so a clock tick alone cannot fire a spurious signal.
    struct VisibleState {
        bool premium = false;
        qint64 expires = 0;
        QStringList features;
        bool loading = false;

        bool operator==(const VisibleState&) const = default;
    };

    [[nodiscard]] VisibleState visibleState() const;

    /// Emits changed() only when the visible state differs from the last one
    /// announced.
    void syncChanged();

    telegram::PremiumStatusModel m_status;
    QStringList m_catalog;
    bool m_loading = false;
    VisibleState m_visible;
};

} // namespace beygram::ui
