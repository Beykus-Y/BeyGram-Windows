#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One booster of a chat as the boost screen lists it: who boosted, how many
/// boosts that user contributed and when those boosts expire. `expirationDate`
/// stays in whatever unit the caller parsed out of TDLib (a Unix timestamp in
/// seconds); the model only stores it and never reads the clock.
struct BoostRow {
    qint64 userId = 0;
    QString name;
    int boostCount = 0;
    qint64 expirationDate = 0;
};

/// The boost state of one chat: its current level, the boosts it has and the
/// number of boosts the next level needs.
///
/// The model never talks to TDLib, never logs and never reads the clock. The
/// caller sends getChatBoosts (or reads the boost fields of a chat), hands the
/// parsed values to setStatus() and the parsed booster list to setBoosters(),
/// and drives any request itself. Because nothing here performs I/O, the tests
/// stay deterministic.
///
/// Invariants:
///
///  - Every count is non-negative. A negative level, boost count or next-level
///    target (an unparsed answer) is stored as 0, so progress can never become
///    negative and the UI cannot render "-1 boosts".
///  - `userId` identifies a booster. setBoosters() drops rows without a
///    positive user id (nothing could be shown or acted on) and keeps the first
///    row of each id, so a caller merging several answers cannot end up with
///    two rows fighting over one user.
///  - Order is the caller's order. setBoosters() only removes rows and clamps
///    a negative per-row boost count; it never sorts, so the screen shows
///    boosters in the order Telegram returned them.
///  - changed() is emitted only when the stored values really differ.
///    Re-sending the same status, re-sending the same boosters, clearing an
///    empty model and clamping an already-non-negative value are all silent,
///    so a refresh that brings the same data does not repaint the screen.
class BoostModel final : public QObject {
    Q_OBJECT

public:
    explicit BoostModel(QObject* parent = nullptr);

    /// Stores the level, the current boost count and the boost count the next
    /// level needs. Each value is clamped to >= 0 and the model keeps the last
    /// value it was given. changed() is emitted only when at least one stored
    /// value differs from what was there before.
    void setStatus(int level, int boostCount, int nextLevelBoostCount);

    /// Current boost level of the chat. 0 when the chat has no boosts.
    [[nodiscard]] int level() const;

    /// Boosts the chat currently has.
    [[nodiscard]] int boostCount() const;

    /// Boosts needed to reach the next level, 0 when there is no next level.
    [[nodiscard]] int nextLevelBoostCount() const;

    /// Progress from the current boost count towards the next level, in
    /// `[0, 1]`. 0 when no target is known (`nextLevelBoostCount() == 0`) and
    /// 0 while the chat has no boosts at all; 1 once the target is reached or
    /// exceeded, so a stale count never overflows a progress bar.
    [[nodiscard]] double progressToNextLevel() const;

    /// Replaces every booster with `boosters`, in the given order. Rows
    /// without a positive user id and later duplicates of a user id (the first
    /// occurrence wins) are dropped, and a negative boost count is stored as 0.
    /// changed() is emitted only when the resulting list differs from the
    /// current one.
    void setBoosters(QVector<BoostRow> boosters);

    /// A copy of the boosters, in display order.
    [[nodiscard]] QVector<BoostRow> boosters() const;

    /// Number of boosters currently held. Always equal to boosters().size().
    [[nodiscard]] int boosterCount() const;

    /// Forgets the status and every booster, e.g. when the chat is left or the
    /// account is signed out. Emits changed() only if there was something to
    /// forget.
    void clear();

signals:
    /// The stored boost state changed. Not emitted for a no-op call.
    void changed();

private:
    int m_level = 0;
    int m_boosts = 0;
    int m_next = 0;
    QVector<BoostRow> m_boosters;
};

} // namespace beygram::telegram
