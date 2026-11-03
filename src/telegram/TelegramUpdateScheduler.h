#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>

namespace beygram::telegram {

/// Coalesces TDLib updates so a burst of hundreds of messages cannot stall the
/// GUI thread in one event-loop turn.
///
/// The scheduler keeps order, drops update types this client does not consume,
/// merges repeated "replaceable" updates for the same chat (position, last
/// message, read markers) and flushes a bounded batch on a short timer.
class TelegramUpdateScheduler final : public QObject {
    Q_OBJECT

public:
    explicit TelegramUpdateScheduler(QObject* parent = nullptr);

    void enqueue(const QJsonObject& update);

    /// Delivers everything that is currently buffered. Public so tests and
    /// shutdown paths can drain the queue without waiting for the timer.
    void flush();

    [[nodiscard]] int pendingCount() const { return m_pending.size(); }

signals:
    void batchReady(const QVector<QJsonObject>& updates);

private:
    [[nodiscard]] static bool isUrgent(const QString& type);
    [[nodiscard]] static bool isIgnored(const QString& type);
    [[nodiscard]] static QString coalesceKey(const QString& type,
                                             const QJsonObject& update);

    static constexpr int FlushIntervalMs = 16;
    static constexpr int MaxBatchSize = 64;

    QVector<QJsonObject> m_pending;
    QHash<QString, int> m_coalesced;
    QTimer m_flushTimer;
};

} // namespace beygram::telegram

Q_DECLARE_METATYPE(QVector<QJsonObject>)
