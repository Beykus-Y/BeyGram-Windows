#include "telegram/TelegramUpdateScheduler.h"

#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject update(const char* type)
{
    return QJsonObject{{QStringLiteral("@type"), QString::fromLatin1(type)}};
}

QJsonObject chatUpdate(const char* type, qint64 chatId, const QString& value)
{
    return QJsonObject{{QStringLiteral("@type"), QString::fromLatin1(type)},
                       {QStringLiteral("chat_id"), chatId},
                       {QStringLiteral("value"), value}};
}

QVector<QJsonObject> takeBatch(QSignalSpy& spy)
{
    if (spy.isEmpty())
        return {};
    return spy.takeFirst().at(0).value<QVector<QJsonObject>>();
}

QString typeOf(const QVector<QJsonObject>& batch)
{
    return batch.isEmpty() ? QString()
                           : batch.first().value(QStringLiteral("@type")).toString();
}

} // namespace

class UpdateSchedulerTest final : public QObject {
    Q_OBJECT

private slots:
    void flushesBufferedUpdates();
    void mergesReplaceableChatUpdates();
    void dropsIgnoredTraffic();
    void deliversUrgentUpdatesImmediately();
    void drainsLargeBatchesInChunks();
};

void UpdateSchedulerTest::flushesBufferedUpdates()
{
    TelegramUpdateScheduler scheduler;
    QSignalSpy spy(&scheduler, &TelegramUpdateScheduler::batchReady);

    scheduler.enqueue(update("updateNewMessage"));
    scheduler.enqueue(update("updateDeleteMessages"));
    QCOMPARE(scheduler.pendingCount(), 2);
    QCOMPARE(spy.count(), 0);

    scheduler.flush();
    QCOMPARE(scheduler.pendingCount(), 0);
    QCOMPARE(spy.count(), 1);

    const QVector<QJsonObject> batch = takeBatch(spy);
    QCOMPARE(batch.size(), 2);
    QCOMPARE(batch.at(0).value(QStringLiteral("@type")).toString(),
             QStringLiteral("updateNewMessage"));
}

void UpdateSchedulerTest::mergesReplaceableChatUpdates()
{
    TelegramUpdateScheduler scheduler;
    QSignalSpy spy(&scheduler, &TelegramUpdateScheduler::batchReady);

    scheduler.enqueue(chatUpdate("updateChatPosition", 7, QStringLiteral("old")));
    scheduler.enqueue(chatUpdate("updateChatPosition", 8, QStringLiteral("other")));
    scheduler.enqueue(chatUpdate("updateChatPosition", 7, QStringLiteral("new")));
    scheduler.flush();

    const QVector<QJsonObject> batch = takeBatch(spy);
    QCOMPARE(batch.size(), 2);
    QCOMPARE(batch.at(0).value(QStringLiteral("value")).toString(),
             QStringLiteral("new"));
    QCOMPARE(batch.at(1).value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(8));
}

void UpdateSchedulerTest::dropsIgnoredTraffic()
{
    TelegramUpdateScheduler scheduler;
    QSignalSpy spy(&scheduler, &TelegramUpdateScheduler::batchReady);

    scheduler.enqueue(update("updateChatAction"));
    scheduler.enqueue(update("updateUserStatus"));
    scheduler.enqueue(update("updateFile"));
    QCOMPARE(scheduler.pendingCount(), 0);
    scheduler.flush();
    QCOMPARE(spy.count(), 0);
}

void UpdateSchedulerTest::deliversUrgentUpdatesImmediately()
{
    TelegramUpdateScheduler scheduler;
    QSignalSpy spy(&scheduler, &TelegramUpdateScheduler::batchReady);

    scheduler.enqueue(update("updateNewMessage"));
    scheduler.enqueue(update("updateAuthorizationState"));

    QCOMPARE(spy.count(), 2);
    const QVector<QJsonObject> first = takeBatch(spy);
    const QVector<QJsonObject> second = takeBatch(spy);
    QCOMPARE(first.size(), 1);
    QCOMPARE(typeOf(first), QStringLiteral("updateNewMessage"));
    QCOMPARE(second.size(), 1);
    QCOMPARE(typeOf(second), QStringLiteral("updateAuthorizationState"));
}

void UpdateSchedulerTest::drainsLargeBatchesInChunks()
{
    TelegramUpdateScheduler scheduler;
    QSignalSpy spy(&scheduler, &TelegramUpdateScheduler::batchReady);

    for (int index = 0; index < 130; ++index)
        scheduler.enqueue(update("updateNewMessage"));

    QCOMPARE(spy.count(), 2);
    QCOMPARE(takeBatch(spy).size(), 64);
    QCOMPARE(takeBatch(spy).size(), 64);
    QCOMPARE(scheduler.pendingCount(), 2);

    scheduler.flush();
    QCOMPARE(takeBatch(spy).size(), 2);
}

QTEST_GUILESS_MAIN(UpdateSchedulerTest)
#include "tst_update_scheduler.moc"
