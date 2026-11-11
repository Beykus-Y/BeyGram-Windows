#include "TelegramUpdateScheduler.h"

#include "TelegramTypes.h"

#include <QSet>

namespace beygram::telegram {

TelegramUpdateScheduler::TelegramUpdateScheduler(QObject* parent)
    : QObject(parent)
{
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(FlushIntervalMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &TelegramUpdateScheduler::flush);
}

void TelegramUpdateScheduler::enqueue(const QJsonObject& update)
{
    const QString type = update.value(QStringLiteral("@type")).toString();
    if (type.isEmpty() || isIgnored(type))
        return;

    if (isUrgent(type)) {
        // Authorization state changes must never wait behind chat traffic.
        flush();
        emit batchReady(QVector<QJsonObject>{update});
        return;
    }

    const QString key = coalesceKey(type, update);
    if (!key.isEmpty()) {
        const auto it = m_coalesced.constFind(key);
        if (it != m_coalesced.cend()) {
            m_pending[it.value()] = update;
            return;
        }
        m_coalesced.insert(key, m_pending.size());
    }

    m_pending.append(update);
    if (m_pending.size() >= MaxBatchSize) {
        flush();
        return;
    }
    if (!m_flushTimer.isActive())
        m_flushTimer.start();
}

void TelegramUpdateScheduler::flush()
{
    m_flushTimer.stop();
    if (m_pending.isEmpty())
        return;

    QVector<QJsonObject> batch = std::move(m_pending);
    m_pending.clear();
    m_coalesced.clear();
    emit batchReady(batch);
}

bool TelegramUpdateScheduler::isUrgent(const QString& type)
{
    return type == QLatin1String("updateAuthorizationState")
        || type == QLatin1String("updateConnectionState");
}

bool TelegramUpdateScheduler::isIgnored(const QString& type)
{
    // The repository never consumes these, so filtering them here keeps bursty
    // typing/status/file traffic out of the model thread entirely.
    return type == QLatin1String("updateUserStatus")
        || type == QLatin1String("updateChatOnlineMemberCount")
        || type == QLatin1String("updateFile")
        || type == QLatin1String("updateMessageContentOpened");
}

QString TelegramUpdateScheduler::coalesceKey(const QString& type,
                                             const QJsonObject& update)
{
    static const QSet<QString> replaceable{
        QStringLiteral("updateChatPosition"),
        QStringLiteral("updateChatLastMessage"),
        QStringLiteral("updateChatReadInbox"),
        QStringLiteral("updateChatReadOutbox"),
        QStringLiteral("updateChatUnreadMentionCount"),
        QStringLiteral("updateChatNotificationSettings"),
        QStringLiteral("updateChatTitle"),
        QStringLiteral("updateChatPhoto"),
        QStringLiteral("updateChatIsMarkedAsUnread"),
        QStringLiteral("updateChatAddedToList"),
        QStringLiteral("updateChatRemovedFromList"),
    };
    if (!replaceable.contains(type))
        return {};
    const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
    if (chatId == 0)
        return {};
    return type + QLatin1Char(':') + QString::number(chatId);
}

} // namespace beygram::telegram
