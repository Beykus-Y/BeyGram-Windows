#pragma once

#include "telegram/GiftCatalogModel.h"
#include "telegram/GiftHistoryModel.h"
#include "telegram/GiftSendController.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// The gift picker as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the
/// catalog lives in GiftCatalogModel, the send form in GiftSendController and
/// the history in GiftHistoryModel. The facade only does what QML cannot: it
/// accepts the string lists a TDLib-shaped payload arrives as, turns them into
/// the models' value types, and formats the rows the picker renders.
///
/// The three models' changed() signals are forwarded as this object's
/// changed(), so a QML binding on any property repaints on a real update and
/// stays quiet on a redundant one (each model already suppresses no-op
/// changes). The selection is the one piece of state the facade owns; moving it
/// updates the send controller, which emits the same change.
///
/// Formatting:
///   - prices() is one row per catalog gift: the star count, a space and the
///     Telegram star glyph (U+2B50), e.g. 50 followed by the glyph.
///   - titles() is the catalog title verbatim. TDLib leaves it empty for plain
///     catalog gifts, so the view chooses its own fallback label.
///   - history() is one row per stored gift, newest first, with the direction,
///     title, price and date separated by a middle dot (U+00B7), e.g.
///     "Received <dot> Bear <dot> 50 <star> <dot> 2026-01-02". Empty title or
///     date segments are omitted rather than left as a dangling separator.
///
/// Ids are strings because TDLib ids are int64 and a JavaScript number cannot
/// hold every one of them. A value that is not a decimal integer reads as 0,
/// which the models treat as "no value". The facade never touches TDLib, the
/// network or storage, and never sends anything itself: send() only reports the
/// request for the caller to execute.
class GiftsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList titles READ titles NOTIFY changed)
    Q_PROPERTY(QStringList prices READ prices NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY changed)
    Q_PROPERTY(bool anonymous READ anonymous WRITE setAnonymous NOTIFY changed)
    Q_PROPERTY(bool canSend READ canSend NOTIFY changed)
    Q_PROPERTY(QStringList history READ history NOTIFY changed)

public:
    explicit GiftsFacade(QObject* parent = nullptr);

    /// The catalog titles in stored order.
    [[nodiscard]] QStringList titles() const;

    /// The catalog prices as the star count followed by the star glyph, in
    /// stored order.
    [[nodiscard]] QStringList prices() const;

    /// Replaces the catalog from the string lists a payload arrives as: ids and
    /// stars are decimal integers, titles are free text. The lists are read
    /// positionally and may be shorter than ids; a missing title reads as empty
    /// and a missing or malformed star count as 0.
    ///
    /// The model's normalization applies: non-positive ids are dropped, a
    /// negative star count is dropped and only the first gift of each id is
    /// kept, so a repeated id cannot appear twice. The caller's order is
    /// otherwise preserved. A selection whose gift is still in the new catalog
    /// follows that gift to its new index; a selection whose gift disappeared
    /// is cleared.
    Q_INVOKABLE void setCatalog(const QStringList& ids, const QStringList& titles,
                                const QStringList& stars);

    /// The selected catalog index, or -1 when nothing is selected.
    [[nodiscard]] int selectedIndex() const;

    /// Selects a catalog gift. -1 clears the selection; any other out-of-range
    /// index is ignored. Selecting the already selected index is a no-op. A
    /// real change moves the send controller to that gift and its price.
    Q_INVOKABLE void select(int index);

    /// The normalised message sent with the gift, or empty when none is set.
    [[nodiscard]] QString message() const;
    void setMessage(const QString& text);

    /// True when the gift is sent anonymously (sendGift.is_private).
    [[nodiscard]] bool anonymous() const;
    void setAnonymous(bool anonymous);

    /// True when the form can be sent: a positive gift id, a positive price and
    /// a positive chat or user id. The controller defines those rules.
    [[nodiscard]] bool canSend() const;

    /// Sets the recipient from decimal strings. chatId is the private chat that
    /// receives the gift; userId is the fallback owner when no positive chat id
    /// is known yet. Ids are stored as given and judged by canSend(), so a
    /// group/channel chat id cannot make a send valid.
    Q_INVOKABLE void setRecipient(const QString& chatId, const QString& userId);

    /// Emits sendRequested() with the current gift, recipient, message and
    /// anonymity when canSend() is true, and returns whether it did. It does
    /// not clear the form, so a failed send leaves the user's choice in place.
    Q_INVOKABLE bool send();

    /// The formatted history rows, newest first.
    [[nodiscard]] QStringList history() const;

    /// Adds one history row. title and date are free text (date is shown as
    /// given), stars is a decimal amount that the model clamps to >= 0, and
    /// incoming selects the direction. The history model needs a positive gift
    /// id and uses it only for de-duplication, so the facade assigns a synthetic
    /// id; direction is carried by the sender/receiver fields. An ISO-style date
    /// is also parsed for ordering; an unparsable date is treated as undated
    /// and such rows keep their insertion order, newest first.
    Q_INVOKABLE void addHistory(const QString& title, const QString& date,
                                const QString& stars, bool incoming);

    /// Clears the send form (selection, message, anonymity and recipient) while
    /// keeping the catalog and the history. Emits changed() only when there was
    /// something to clear.
    Q_INVOKABLE void reset();

signals:
    /// Emitted whenever any bound model or the selection actually changed.
    void changed();

    /// Emitted by send() when canSend() is true. Ids are decimal strings; the
    /// caller resolves chatId/userId the same way GiftSendController does.
    void sendRequested(const QString& giftId, const QString& chatId,
                       const QString& userId, const QString& message,
                       bool anonymous);

private:
    /// The display fields GiftHistoryRow does not carry: the model stores only
    /// ids, price, date and direction.
    struct HistoryEntry {
        QString title;
        QString date;
        bool incoming = false;
    };

    /// Re-resolves the selected gift id against the current catalog and moves
    /// the send controller onto it. Called when the catalog changed; its own
    /// controller update is folded into the catalog's change notification.
    void reconcileSelection();

    telegram::GiftCatalogModel m_catalog;
    telegram::GiftSendController m_send;
    telegram::GiftHistoryModel m_history;

    QHash<qint64, HistoryEntry> m_historyEntries;
    qint64 m_nextHistoryId = 1;
    qint64 m_selectedGiftId = 0;
    int m_selectedIndex = -1;
    bool m_reconciling = false;
};

} // namespace beygram::ui
