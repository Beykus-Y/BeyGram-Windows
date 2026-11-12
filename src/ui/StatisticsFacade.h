#pragma once

#include "telegram/BoostModel.h"
#include "telegram/ChatStatisticsModel.h"
#include "telegram/MessageStatisticsModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The statistics screen as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the chat
/// overview and its chart live in ChatStatisticsModel, the per-message counters
/// in MessageStatisticsModel and the boost state in BoostModel. Every property
/// reads straight from those models, and their changed() signals are forwarded
/// as this object's changed(), so a binding repaints on a real update and stays
/// quiet on a redundant one (each model already suppresses no-op changes).
///
/// The facade only does what QML cannot:
///   - it accepts the string lists TDLib-shaped payloads arrive as and turns
///     them into the models' value types;
///   - it formats every number the panel prints.
///
/// Formatting has one voice across the screen:
///   - Overview values are "<number>[<unit>]", e.g. "1 234", "12.5%",
///     "1 234 MB". Digits are grouped by spaces, three at a time from the
///     right, and at most two decimals are kept with trailing zeros dropped.
///     A unit that starts with a symbol ("%") hugs the number; a unit that
///     starts with a letter or digit ("MB") is separated by one space.
///   - Chart values are grouped the same way. A panel that needs the number
///     strips the grouping spaces before it draws a bar.
///   - The per-message counters stay ints, because the panel can lay them out
///     however it likes; the facade does not pick a font size or a grouping.
///
/// Requests are announced, never performed: the facade holds no transport and
/// performs no I/O. requestChatStats() emits chatStatsRequested(), and
/// openMessageStats() selects the message and emits messageStatsRequested()
/// with the normalized ids. The caller answers with setChatStats() and
/// setMessageCounts(); a late answer that arrives after clear() is dropped by
/// MessageStatisticsModel, so a stale panel cannot be revived.
///
/// Invariants:
///  - chatStatsAvailable() is true exactly when the chat model holds an
///    overview entry or a chart point.
///  - messageStatsActive() is true exactly when a message is selected; the
///    counters only change while it is true.
///  - boostProgress() is always in [0, 1].
class StatisticsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool chatStatsAvailable READ chatStatsAvailable NOTIFY changed)
    Q_PROPERTY(QStringList overviewLabels READ overviewLabels NOTIFY changed)
    Q_PROPERTY(QStringList overviewValues READ overviewValues NOTIFY changed)
    Q_PROPERTY(QStringList chartLabels READ chartLabels NOTIFY changed)
    Q_PROPERTY(QStringList chartValues READ chartValues NOTIFY changed)
    Q_PROPERTY(bool messageStatsActive READ messageStatsActive NOTIFY changed)
    Q_PROPERTY(int views READ views NOTIFY changed)
    Q_PROPERTY(int forwards READ forwards NOTIFY changed)
    Q_PROPERTY(int reactions READ reactions NOTIFY changed)
    Q_PROPERTY(int boostLevel READ boostLevel NOTIFY changed)
    Q_PROPERTY(int boostCount READ boostCount NOTIFY changed)
    Q_PROPERTY(double boostProgress READ boostProgress NOTIFY changed)

public:
    explicit StatisticsFacade(QObject* parent = nullptr);

    /// Whether the chat model holds anything to render.
    [[nodiscard]] bool chatStatsAvailable() const;

    /// The overview captions, in display order.
    [[nodiscard]] QStringList overviewLabels() const;

    /// The formatted overview numbers, aligned with overviewLabels().
    [[nodiscard]] QStringList overviewValues() const;

    /// The chart captions, stored verbatim.
    [[nodiscard]] QStringList chartLabels() const;

    /// The formatted chart numbers, aligned with chartLabels(): the series is
    /// truncated or padded with "0" by the model, so both lists can be indexed
    /// with the same position.
    [[nodiscard]] QStringList chartValues() const;

    /// Replaces the chat statistics from the string lists a payload arrives as.
    ///
    /// `labels` and `values` are read positionally; a missing value reads as 0,
    /// and extra values without a label are ignored because there would be no
    /// row to caption them. Each value may carry its own unit suffix ("12.5%",
    /// "1 234 MB"); grouping spaces are dropped and the rest of the string
    /// after the number becomes the unit. Text with no leading number reads as
    /// 0 rather than being guessed at.
    ///
    /// `chartLabelList` is stored verbatim and `chartValueList` is parsed
    /// positionally, with a missing point read as 0. The model keeps the chart
    /// aligned with its labels, so a short or long value list is normalized
    /// there instead of here. The model also drops overview entries with an
    /// empty label and later duplicates of an earlier label.
    Q_INVOKABLE void setChatStats(const QStringList& labels, const QStringList& values,
                                  const QStringList& chartLabelList,
                                  const QStringList& chartValueList);

    /// Announces that the user asked for chat statistics: emits
    /// chatStatsRequested() and changes nothing else, so a refresh leaves the
    /// last answer on screen until a new one arrives.
    Q_INVOKABLE void requestChatStats();

    /// Selects the message the counters belong to and announces that its
    /// statistics are needed. Both ids must be positive decimal integers;
    /// anything else (empty, "0", negative, non-numeric) is ignored and emits
    /// nothing. A valid pair resets the counters, because they described the
    /// previous message, and emits messageStatsRequested() with the ids as
    /// normalized decimal strings.
    Q_INVOKABLE void openMessageStats(const QString& chatId, const QString& messageId);

    /// Whether a message is selected.
    [[nodiscard]] bool messageStatsActive() const;

    /// Replaces the three counters. Negative values are stored as 0, and the
    /// call is ignored while no message is selected, so a late answer cannot
    /// revive a closed panel.
    Q_INVOKABLE void setMessageCounts(int views, int forwards, int reactions);

    [[nodiscard]] int views() const;
    [[nodiscard]] int forwards() const;
    [[nodiscard]] int reactions() const;

    /// Stores the boost level, the boosts the chat has and the boost count the
    /// next level needs. Each value is clamped to >= 0 by the model.
    Q_INVOKABLE void setBoosts(int level, int count, int nextLevelCount);

    [[nodiscard]] int boostLevel() const;
    [[nodiscard]] int boostCount() const;

    /// Progress towards the next boost level, in [0, 1]: 0 when no target is
    /// known or the chat has no boosts, 1 once the target is reached.
    [[nodiscard]] double boostProgress() const;

    /// Forgets the chat statistics, the selected message and the boost state,
    /// e.g. when the chat is left or the account is signed out. Emits changed()
    /// only for the models that had something to forget.
    Q_INVOKABLE void clear();

signals:
    /// Emitted whenever any of the three bound models actually changed.
    void changed();

    /// Emitted by requestChatStats() for the caller to fetch chat statistics.
    void chatStatsRequested();

    /// Emitted by openMessageStats() with the normalized ids of the message
    /// whose statistics the caller should fetch.
    void messageStatsRequested(const QString& chatId, const QString& messageId);

private:
    telegram::ChatStatisticsModel m_chat;
    telegram::MessageStatisticsModel m_message;
    telegram::BoostModel m_boosts;
};

} // namespace beygram::ui
