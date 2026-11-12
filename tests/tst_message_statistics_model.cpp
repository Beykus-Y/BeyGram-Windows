#include "telegram/MessageStatisticsModel.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::MessageStatisticsModel;

class MessageStatisticsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void setMessageActivatesAndStoresIds();
    void setMessageRejectsNonPositiveChatId();
    void setMessageRejectsNonPositiveMessageId();
    void setMessageResetsCountsAndLoading();
    void setMessageSameIdsResetsCounts();
    void setMessageSameIdsIsSilentWhenAlreadyEmpty();
    void setMessageSwitchesMessage();
    void setCountsStoresValues();
    void setCountsClampsNegatives();
    void setCountsClampsEachCounterSeparately();
    void setCountsIgnoredWhenInactive();
    void setCountsSilentWhenUnchanged();
    void setCountsSilentWhenClampedValuesMatch();
    void setLoadingTogglesAndSignals();
    void setLoadingIgnoredWhenInactive();
    void setLoadingSilentWhenUnchanged();
    void clearResetsEverything();
    void clearOnInactiveIsNoOp();
    void clearThenLateCountsAreIgnored();
    void acceptsNewMessageAfterClear();
    void changedCountsAcrossOperations();
};

void MessageStatisticsModelTest::startsInactive()
{
    MessageStatisticsModel model;

    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
    QVERIFY(!model.loading());
}

void MessageStatisticsModelTest::setMessageActivatesAndStoresIds()
{
    MessageStatisticsModel model;
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setMessage(42, 777);

    QVERIFY(model.active());
    QCOMPARE(model.chatId(), 42);
    QCOMPARE(model.messageId(), 777);
    QCOMPARE(spy.count(), 1);
}

void MessageStatisticsModelTest::setMessageRejectsNonPositiveChatId()
{
    MessageStatisticsModel model;
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setMessage(0, 777);
    model.setMessage(-1, 777);

    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageStatisticsModelTest::setMessageRejectsNonPositiveMessageId()
{
    MessageStatisticsModel model;
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setMessage(42, 0);
    model.setMessage(42, -5);

    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageStatisticsModelTest::setMessageResetsCountsAndLoading()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.setCounts(10, 20, 30);
    model.setLoading(true);

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setMessage(43, 778);

    QCOMPARE(model.chatId(), 43);
    QCOMPARE(model.messageId(), 778);
    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
    QVERIFY(!model.loading());
    QCOMPARE(spy.count(), 1);
}

void MessageStatisticsModelTest::setMessageSameIdsResetsCounts()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.setCounts(10, 20, 30);
    model.setLoading(true);

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    // Re-selecting the same message is a refresh: the old numbers are dropped.
    model.setMessage(42, 777);

    QCOMPARE(model.chatId(), 42);
    QCOMPARE(model.messageId(), 777);
    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
    QVERIFY(!model.loading());
    QCOMPARE(spy.count(), 1);
}

void MessageStatisticsModelTest::setMessageSameIdsIsSilentWhenAlreadyEmpty()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setMessage(42, 777);

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.active());
}

void MessageStatisticsModelTest::setMessageSwitchesMessage()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.setMessage(43, 778);

    QCOMPARE(model.chatId(), 43);
    QCOMPARE(model.messageId(), 778);
    QVERIFY(model.active());
}

void MessageStatisticsModelTest::setCountsStoresValues()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setCounts(1200, 34, 5);

    QCOMPARE(model.views(), 1200);
    QCOMPARE(model.forwards(), 34);
    QCOMPARE(model.reactions(), 5);
    QCOMPARE(spy.count(), 1);
}

void MessageStatisticsModelTest::setCountsClampsNegatives()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);

    model.setCounts(-1, -20, -300);

    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
}

void MessageStatisticsModelTest::setCountsClampsEachCounterSeparately()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);

    model.setCounts(-5, 7, -9);

    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 7);
    QCOMPARE(model.reactions(), 0);
}

void MessageStatisticsModelTest::setCountsIgnoredWhenInactive()
{
    MessageStatisticsModel model;
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setCounts(10, 20, 30);

    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
    QCOMPARE(spy.count(), 0);
}

void MessageStatisticsModelTest::setCountsSilentWhenUnchanged()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.setCounts(10, 20, 30);

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setCounts(10, 20, 30);

    QCOMPARE(spy.count(), 0);
}

void MessageStatisticsModelTest::setCountsSilentWhenClampedValuesMatch()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.setCounts(0, 0, 0);

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    // The negatives clamp to the zeros already stored, so nothing changed.
    model.setCounts(-4, -5, -6);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
}

void MessageStatisticsModelTest::setLoadingTogglesAndSignals()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setLoading(true);
    QVERIFY(model.loading());
    QCOMPARE(spy.count(), 1);

    model.setLoading(false);
    QVERIFY(!model.loading());
    QCOMPARE(spy.count(), 2);
}

void MessageStatisticsModelTest::setLoadingIgnoredWhenInactive()
{
    MessageStatisticsModel model;
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setLoading(true);

    QVERIFY(!model.loading());
    QCOMPARE(spy.count(), 0);
}

void MessageStatisticsModelTest::setLoadingSilentWhenUnchanged()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setLoading(false);

    QCOMPARE(spy.count(), 0);

    model.setLoading(true);
    QCOMPARE(spy.count(), 1);

    model.setLoading(true);
    QCOMPARE(spy.count(), 1);
}

void MessageStatisticsModelTest::clearResetsEverything()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.setCounts(10, 20, 30);
    model.setLoading(true);

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.clear();

    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), 0);
    QCOMPARE(model.messageId(), 0);
    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
    QVERIFY(!model.loading());
    QCOMPARE(spy.count(), 1);
}

void MessageStatisticsModelTest::clearOnInactiveIsNoOp()
{
    MessageStatisticsModel model;
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
}

void MessageStatisticsModelTest::clearThenLateCountsAreIgnored()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.clear();

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setCounts(10, 20, 30);
    model.setLoading(true);

    QVERIFY(!model.active());
    QCOMPARE(model.views(), 0);
    QCOMPARE(model.forwards(), 0);
    QCOMPARE(model.reactions(), 0);
    QVERIFY(!model.loading());
    QCOMPARE(spy.count(), 0);
}

void MessageStatisticsModelTest::acceptsNewMessageAfterClear()
{
    MessageStatisticsModel model;
    model.setMessage(42, 777);
    model.clear();

    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setMessage(43, 778);

    QVERIFY(model.active());
    QCOMPARE(model.chatId(), 43);
    QCOMPARE(model.messageId(), 778);
    QCOMPARE(spy.count(), 1);
}

void MessageStatisticsModelTest::changedCountsAcrossOperations()
{
    MessageStatisticsModel model;
    QSignalSpy spy(&model, &MessageStatisticsModel::changed);

    model.setMessage(42, 777);
    QCOMPARE(spy.count(), 1);

    model.setCounts(1, 2, 3);
    QCOMPARE(spy.count(), 2);

    model.setLoading(true);
    QCOMPARE(spy.count(), 3);

    // Nothing actually changes here.
    model.setCounts(1, 2, 3);
    model.setLoading(true);
    model.setMessage(0, 777);
    model.setMessage(42, -1);
    QCOMPARE(spy.count(), 3);

    model.clear();
    QCOMPARE(spy.count(), 4);

    model.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(MessageStatisticsModelTest)
#include "tst_message_statistics_model.moc"
