#include "telegram/StickerSendController.h"

#include <QJsonObject>
#include <QSet>
#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject contentOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_message_content")).toObject();
}

QJsonObject stickerFileOf(const QJsonObject& request)
{
    return contentOf(request)
        .value(QStringLiteral("sticker"))
        .toObject()
        .value(QStringLiteral("sticker"))
        .toObject();
}

qint64 chatIdOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("chat_id")).toVariant().toLongLong();
}

qint64 replyIdOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("reply_to"))
        .toObject()
        .value(QStringLiteral("message_id"))
        .toVariant()
        .toLongLong();
}

} // namespace

class StickerSendControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithoutTarget();
    void setTargetAcceptsChatAndReply();
    void setTargetRejectsInvalidChat();
    void setTargetNormalizesReply();
    void setTargetEmitsOnlyOnChange();
    void canSendRequiresTarget();
    void canSendValidatesStickerAndFile();
    void requestBuildsStickerForTarget();
    void requestFallsBackToRemoteIdentifier();
    void requestIsEmptyWithoutTarget();
    void addRecentKeepsNewestFirst();
    void addRecentMovesExistingToFront();
    void addRecentCapsAtThirtyByDefault();
    void addRecentHonorsCustomLimit();
    void addRecentIgnoresInvalidInput();
    void addRecentEmitsOnlyOnRealChange();
    void recentIsASnapshot();
    void emitsChangedForTargetAndRecent();
};

void StickerSendControllerTest::startsWithoutTarget()
{
    StickerSendController controller;

    QVERIFY(!controller.canSend(123, 0));
    QVERIFY(!controller.canSend(123, 7));
    QVERIFY(controller.request(123, 7).isEmpty());
    QVERIFY(controller.recent().isEmpty());
}

void StickerSendControllerTest::setTargetAcceptsChatAndReply()
{
    StickerSendController controller;

    controller.setTarget(42, 99);
    QVERIFY(controller.canSend(1, 0));

    const QJsonObject direct = controller.request(1, 0);
    QCOMPARE(chatIdOf(direct), qint64(42));
    QCOMPARE(replyIdOf(direct), qint64(99));

    // Group and channel ids are negative, so they are valid targets too.
    controller.setTarget(-1001234567890LL);
    const QJsonObject group = controller.request(2, 0);
    QCOMPARE(chatIdOf(group), qint64(-1001234567890LL));
    QVERIFY(group.value(QStringLiteral("reply_to")).isNull());
}

void StickerSendControllerTest::setTargetRejectsInvalidChat()
{
    StickerSendController controller;
    controller.setTarget(42, 7);

    QSignalSpy spy(&controller, &StickerSendController::changed);
    controller.setTarget(0);
    controller.setTarget(0, 5);
    controller.setTarget(0, -5);

    // The old target must survive a rejected update.
    QCOMPARE(spy.count(), 0);
    const QJsonObject request = controller.request(1, 0);
    QCOMPARE(chatIdOf(request), qint64(42));
    QCOMPARE(replyIdOf(request), qint64(7));
}

void StickerSendControllerTest::setTargetNormalizesReply()
{
    StickerSendController controller;

    controller.setTarget(42, -5);
    QVERIFY(controller.request(1, 0).value(QStringLiteral("reply_to")).isNull());

    controller.setTarget(42, 0);
    QVERIFY(controller.request(1, 0).value(QStringLiteral("reply_to")).isNull());

    controller.setTarget(42, 1);
    QCOMPARE(replyIdOf(controller.request(1, 0)), qint64(1));
}

void StickerSendControllerTest::setTargetEmitsOnlyOnChange()
{
    StickerSendController controller;
    QSignalSpy spy(&controller, &StickerSendController::changed);

    controller.setTarget(42);          // 1
    controller.setTarget(42);          // no-op
    controller.setTarget(42, 0);       // no-op
    controller.setTarget(42, 5);       // 2
    controller.setTarget(42, 5);       // no-op
    controller.setTarget(42, -1);      // normalises to 0 -> 3
    controller.setTarget(-7, 0);       // 4
    controller.setTarget(-7, -3);      // normalises to the same target -> no-op

    QCOMPARE(spy.count(), 4);
}

void StickerSendControllerTest::canSendRequiresTarget()
{
    StickerSendController controller;

    QVERIFY(!controller.canSend(1, 0));
    QVERIFY(!controller.canSend(9876543210LL, 55));

    controller.setTarget(1);
    QVERIFY(controller.canSend(1, 0));
    QVERIFY(controller.canSend(9876543210LL, 55));
}

void StickerSendControllerTest::canSendValidatesStickerAndFile()
{
    StickerSendController controller;
    controller.setTarget(42);

    QVERIFY(controller.canSend(1, 0));
    QVERIFY(controller.canSend(1, 55));
    QVERIFY(controller.canSend(9876543210LL, 1234));

    QVERIFY(!controller.canSend(0, 0));
    QVERIFY(!controller.canSend(-1, 0));
    QVERIFY(!controller.canSend(1, -1));
}

void StickerSendControllerTest::requestBuildsStickerForTarget()
{
    StickerSendController controller;
    controller.setTarget(-1001234567890LL, 777);

    const QJsonObject request = controller.request(9876543210LL, 1234);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessage"));
    QCOMPARE(chatIdOf(request), qint64(-1001234567890LL));
    QCOMPARE(replyIdOf(request), qint64(777));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("options")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_markup")).isNull());
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QJsonObject content = contentOf(request);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageSticker"));
    QCOMPARE(content.value(QStringLiteral("emoji")).toString(), QString());
    QVERIFY(content.value(QStringLiteral("emoji")).isString());

    const QJsonObject file = stickerFileOf(request);
    QCOMPARE(file.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileId"));
    QCOMPARE(file.value(QStringLiteral("id")).toInt(), 1234);
}

void StickerSendControllerTest::requestFallsBackToRemoteIdentifier()
{
    StickerSendController controller;
    controller.setTarget(42);

    const QJsonObject file = stickerFileOf(controller.request(9876543210LL, 0));
    QCOMPARE(file.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileRemote"));
    QCOMPARE(file.value(QStringLiteral("id")).toString(),
             QStringLiteral("9876543210"));
}

void StickerSendControllerTest::requestIsEmptyWithoutTarget()
{
    StickerSendController controller;

    QVERIFY(controller.request(9876543210LL, 0).isEmpty());
    QVERIFY(controller.request(9876543210LL, 55).isEmpty());

    // An invalid sticker or file id also yields no request.
    controller.setTarget(42);
    QVERIFY(controller.request(0, 55).isEmpty());
    QVERIFY(controller.request(-3, 55).isEmpty());
    QVERIFY(controller.request(9876543210LL, -1).isEmpty());
}

void StickerSendControllerTest::addRecentKeepsNewestFirst()
{
    StickerSendController controller;

    controller.addRecent(10);
    controller.addRecent(20);
    controller.addRecent(30);

    QCOMPARE(controller.recent(), (QVector<qint64>{30, 20, 10}));

    controller.addRecent(9876543210LL);
    QCOMPARE(controller.recent(),
             (QVector<qint64>{9876543210LL, 30, 20, 10}));
}

void StickerSendControllerTest::addRecentMovesExistingToFront()
{
    StickerSendController controller;

    controller.addRecent(5);
    controller.addRecent(7);
    controller.addRecent(9);
    controller.addRecent(7);

    QCOMPARE(controller.recent(), (QVector<qint64>{7, 9, 5}));

    // Repeating the newest sticker is not a duplicate and keeps the size.
    controller.addRecent(7);
    QCOMPARE(controller.recent(), (QVector<qint64>{7, 9, 5}));
}

void StickerSendControllerTest::addRecentCapsAtThirtyByDefault()
{
    StickerSendController controller;

    for (qint64 id = 1; id <= 35; ++id)
        controller.addRecent(id);

    const QVector<qint64> recent = controller.recent();
    QCOMPARE(recent.size(), 30);
    QCOMPARE(recent.first(), qint64(35));
    QCOMPARE(recent.last(), qint64(6));

    QSet<qint64> unique;
    for (const qint64 id : recent)
        unique.insert(id);
    QCOMPARE(unique.size(), 30);
}

void StickerSendControllerTest::addRecentHonorsCustomLimit()
{
    StickerSendController controller;

    controller.addRecent(1, 3);
    controller.addRecent(2, 3);
    controller.addRecent(3, 3);
    controller.addRecent(4, 3);
    QCOMPARE(controller.recent(), (QVector<qint64>{4, 3, 2}));

    // Adding a sticker that is already newest still applies a smaller cap.
    controller.addRecent(4, 2);
    QCOMPARE(controller.recent(), (QVector<qint64>{4, 3}));

    controller.addRecent(9, 1);
    QCOMPARE(controller.recent(), (QVector<qint64>{9}));
}

void StickerSendControllerTest::addRecentIgnoresInvalidInput()
{
    StickerSendController controller;
    QSignalSpy spy(&controller, &StickerSendController::changed);

    controller.addRecent(0);
    controller.addRecent(-3);
    controller.addRecent(5, 0);
    controller.addRecent(5, -2);

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.recent().isEmpty());

    controller.addRecent(5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.recent(), (QVector<qint64>{5}));
}

void StickerSendControllerTest::addRecentEmitsOnlyOnRealChange()
{
    StickerSendController controller;
    QSignalSpy spy(&controller, &StickerSendController::changed);

    controller.addRecent(1);          // 1
    controller.addRecent(1);          // no-op
    controller.addRecent(2);          // 2 -> {2, 1}
    controller.addRecent(1);          // 3 -> {1, 2}
    controller.addRecent(0);          // no-op
    controller.addRecent(3, 0);       // no-op
    controller.addRecent(1, 2);       // already {1, 2} -> no-op
    controller.addRecent(4, 2);       // 4 -> {4, 1}

    QCOMPARE(spy.count(), 4);
    QCOMPARE(controller.recent(), (QVector<qint64>{4, 1}));
}

void StickerSendControllerTest::recentIsASnapshot()
{
    StickerSendController controller;
    controller.addRecent(10);
    controller.addRecent(20);

    QVector<qint64> snapshot = controller.recent();
    QCOMPARE(snapshot, (QVector<qint64>{20, 10}));

    controller.addRecent(30);
    QCOMPARE(snapshot, (QVector<qint64>{20, 10}));
    QCOMPARE(controller.recent(), (QVector<qint64>{30, 20, 10}));

    // Mutating the snapshot must not touch the stored list.
    snapshot.append(99);
    QCOMPARE(controller.recent(), (QVector<qint64>{30, 20, 10}));
}

void StickerSendControllerTest::emitsChangedForTargetAndRecent()
{
    StickerSendController controller;
    QSignalSpy spy(&controller, &StickerSendController::changed);

    controller.setTarget(42);          // 1
    controller.setTarget(42);          // no-op
    controller.addRecent(1);           // 2
    controller.addRecent(1);           // no-op
    controller.addRecent(2);           // 3
    controller.setTarget(42, 7);       // 4
    controller.addRecent(1);           // 5 -> {1, 2}
    controller.setTarget(0);           // no-op

    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(StickerSendControllerTest)
#include "tst_sticker_send_controller.moc"
