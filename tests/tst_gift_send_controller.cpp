#include "telegram/GiftSendController.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QString typeOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("@type")).toString();
}

qint64 giftIdOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("gift_id")).toVariant().toLongLong();
}

QJsonObject ownerOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("owner_id")).toObject();
}

qint64 ownerUserIdOf(const QJsonObject& request)
{
    return ownerOf(request).value(QStringLiteral("user_id")).toVariant().toLongLong();
}

QJsonObject textOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("text")).toObject();
}

QString textStringOf(const QJsonObject& request)
{
    return textOf(request).value(QStringLiteral("text")).toString();
}

QJsonArray entitiesOf(const QJsonObject& request)
{
    return textOf(request).value(QStringLiteral("entities")).toArray();
}

bool isPrivateOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("is_private")).toBool();
}

bool payForUpgradeOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("pay_for_upgrade")).toBool();
}

// Fills in a gift that is ready to send: recipient, catalog gift and its
// price. It takes the controller by reference because a QObject cannot be
// returned by value.
void makeReady(GiftSendController& controller, qint64 chatId = 42,
               qint64 userId = 0)
{
    controller.setRecipient(chatId, userId);
    controller.setGift(123456, 50);
}

} // namespace

class GiftSendControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setRecipientStoresIds();
    void setRecipientEmitsOnlyOnChange();
    void setGiftStoresGift();
    void setGiftClampsNegativeStars();
    void setGiftEmitsOnlyOnChange();
    void setMessageTrimsAndAcceptsEmpty();
    void setMessageCapsAt255();
    void setMessageKeepsSurrogatePairsWhole();
    void setMessageEmitsOnlyOnChange();
    void setAnonymousStoresFlag();
    void validRequiresGiftStarsAndRecipient();
    void validRejectsGroupAndChannelIds();
    void resetClearsEverything();
    void resetIsNoOpWhenEmpty();
    void requestBuildsSendGift();
    void requestUsesUserIdWithoutChatId();
    void requestPrefersChatIdOverUserId();
    void requestMarksAnonymousAsPrivate();
    void requestIsEmptyWhenInvalid();
    void emitsChangedOnlyOnRealChanges();
};

void GiftSendControllerTest::startsEmpty()
{
    GiftSendController controller;

    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.userId(), qint64(0));
    QCOMPARE(controller.giftId(), qint64(0));
    QCOMPARE(controller.starCount(), qint64(0));
    QCOMPARE(controller.message(), QString());
    QVERIFY(!controller.anonymous());
    QVERIFY(!controller.valid());
    QVERIFY(controller.request().isEmpty());
}

void GiftSendControllerTest::setRecipientStoresIds()
{
    GiftSendController controller;

    controller.setRecipient(42, 7);
    QCOMPARE(controller.chatId(), qint64(42));
    QCOMPARE(controller.userId(), qint64(7));

    controller.setRecipient(9876543210LL, 0);
    QCOMPARE(controller.chatId(), qint64(9876543210LL));
    QCOMPARE(controller.userId(), qint64(0));

    // A group id is stored as given even though it cannot be a gift target.
    controller.setRecipient(-1001234567890LL, 0);
    QCOMPARE(controller.chatId(), qint64(-1001234567890LL));
    QCOMPARE(controller.userId(), qint64(0));
}

void GiftSendControllerTest::setRecipientEmitsOnlyOnChange()
{
    GiftSendController controller;
    QSignalSpy spy(&controller, &GiftSendController::changed);

    controller.setRecipient(42, 0);       // 1
    controller.setRecipient(42, 0);       // no-op
    controller.setRecipient(42, 7);       // 2
    controller.setRecipient(42, 7);       // no-op
    controller.setRecipient(0, 7);        // 3
    controller.setRecipient(0, 0);        // 4

    QCOMPARE(spy.count(), 4);
}

void GiftSendControllerTest::setGiftStoresGift()
{
    GiftSendController controller;

    controller.setGift(123456, 50);
    QCOMPARE(controller.giftId(), qint64(123456));
    QCOMPARE(controller.starCount(), qint64(50));

    controller.setGift(9876543210LL, 1);
    QCOMPARE(controller.giftId(), qint64(9876543210LL));
    QCOMPARE(controller.starCount(), qint64(1));
}

void GiftSendControllerTest::setGiftClampsNegativeStars()
{
    GiftSendController controller;

    controller.setGift(5, -1);
    QCOMPARE(controller.giftId(), qint64(5));
    QCOMPARE(controller.starCount(), qint64(0));

    controller.setGift(5, -9876543210LL);
    QCOMPARE(controller.starCount(), qint64(0));

    // A non-positive gift id is stored as given; valid() is what rejects it.
    controller.setGift(-5, 10);
    QCOMPARE(controller.giftId(), qint64(-5));
    QCOMPARE(controller.starCount(), qint64(10));
}

void GiftSendControllerTest::setGiftEmitsOnlyOnChange()
{
    GiftSendController controller;
    QSignalSpy spy(&controller, &GiftSendController::changed);

    controller.setGift(5, 10);            // 1
    controller.setGift(5, 10);            // no-op
    controller.setGift(5, -3);            // clamps to 0 -> 2
    controller.setGift(5, -3);            // no-op
    controller.setGift(6, 0);             // 3
    controller.setGift(6, 0);             // no-op

    QCOMPARE(spy.count(), 3);
}

void GiftSendControllerTest::setMessageTrimsAndAcceptsEmpty()
{
    GiftSendController controller;

    controller.setMessage(QStringLiteral("  hello  "));
    QCOMPARE(controller.message(), QStringLiteral("hello"));

    controller.setMessage(QStringLiteral("   \n\t "));
    QCOMPARE(controller.message(), QString());

    // The stored message is already trimmed, so setting it back is a no-op.
    QSignalSpy spy(&controller, &GiftSendController::changed);
    controller.setMessage(QStringLiteral("   "));
    controller.setMessage(QString());
    QCOMPARE(spy.count(), 0);
}

void GiftSendControllerTest::setMessageCapsAt255()
{
    GiftSendController controller;

    const QString exact(255, QLatin1Char('a'));
    controller.setMessage(exact);
    QCOMPARE(controller.message(), exact);
    QCOMPARE(controller.message().size(), 255);

    controller.setMessage(QString(300, QLatin1Char('b')));
    QCOMPARE(controller.message().size(), 255);
    QCOMPARE(controller.message(), QString(255, QLatin1Char('b')));

    // Trimming happens before the cap, so the spaces never eat the budget.
    controller.setMessage(QStringLiteral("   ") + QString(300, QLatin1Char('c'))
                          + QStringLiteral("   "));
    QCOMPARE(controller.message(), QString(255, QLatin1Char('c')));
}

void GiftSendControllerTest::setMessageKeepsSurrogatePairsWhole()
{
    GiftSendController controller;
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    QCOMPARE(emoji.size(), 2);

    // 300 emoji are 300 characters, so the cap keeps 255 whole ones (510
    // UTF-16 units) instead of cutting a pair in half.
    QString many;
    for (int i = 0; i < 300; ++i)
        many += emoji;
    controller.setMessage(many);
    QCOMPARE(controller.message().size(), 255 * 2);
    QCOMPARE(controller.message().count(emoji), 255);

    // 254 ASCII characters, one emoji and one more character: the 256th
    // character does not fit, and the emoji stays complete even though it ends
    // on an odd UTF-16 index.
    controller.setMessage(QString(254, QLatin1Char('a')) + emoji
                          + QStringLiteral("b"));
    QCOMPARE(controller.message().size(), 256);
    QVERIFY(controller.message().endsWith(emoji));
    QVERIFY(controller.message().at(254).isHighSurrogate());
    QVERIFY(controller.message().at(255).isLowSurrogate());
}

void GiftSendControllerTest::setMessageEmitsOnlyOnChange()
{
    GiftSendController controller;
    QSignalSpy spy(&controller, &GiftSendController::changed);

    controller.setMessage(QStringLiteral("hi"));       // 1
    controller.setMessage(QStringLiteral(" hi "));     // normalises -> no-op
    controller.setMessage(QStringLiteral("bye"));      // 2
    controller.setMessage(QString());                  // 3
    controller.setMessage(QString());                  // no-op

    QCOMPARE(spy.count(), 3);
    QCOMPARE(controller.message(), QString());
}

void GiftSendControllerTest::setAnonymousStoresFlag()
{
    GiftSendController controller;

    QVERIFY(!controller.anonymous());
    controller.setAnonymous(true);
    QVERIFY(controller.anonymous());
    controller.setAnonymous(false);
    QVERIFY(!controller.anonymous());

    QSignalSpy spy(&controller, &GiftSendController::changed);
    controller.setAnonymous(false);
    QCOMPARE(spy.count(), 0);
}

void GiftSendControllerTest::validRequiresGiftStarsAndRecipient()
{
    GiftSendController controller;
    QVERIFY(!controller.valid());

    // Gift and price alone are not enough: there is no recipient.
    controller.setGift(123456, 50);
    QVERIFY(!controller.valid());

    controller.setRecipient(42, 0);
    QVERIFY(controller.valid());

    controller.setGift(0, 50);
    QVERIFY(!controller.valid());

    controller.setGift(123456, 0);
    QVERIFY(!controller.valid());

    controller.setGift(-1, 50);
    QVERIFY(!controller.valid());

    // A user id alone is a valid recipient too.
    controller.setGift(123456, 50);
    controller.setRecipient(0, 7);
    QVERIFY(controller.valid());

    controller.setRecipient(0, 0);
    QVERIFY(!controller.valid());
}

void GiftSendControllerTest::validRejectsGroupAndChannelIds()
{
    GiftSendController controller;
    controller.setGift(123456, 50);

    controller.setRecipient(-1001234567890LL, 0);
    QVERIFY(!controller.valid());

    controller.setRecipient(-7, -9);
    QVERIFY(!controller.valid());

    // A group chat id next to a positive user id still resolves to the user.
    controller.setRecipient(-1001234567890LL, 7);
    QVERIFY(controller.valid());
}

void GiftSendControllerTest::resetClearsEverything()
{
    GiftSendController controller;
    controller.setRecipient(42, 7);
    controller.setGift(123456, 50);
    controller.setMessage(QStringLiteral("for you"));
    controller.setAnonymous(true);

    QSignalSpy spy(&controller, &GiftSendController::changed);
    controller.reset();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.userId(), qint64(0));
    QCOMPARE(controller.giftId(), qint64(0));
    QCOMPARE(controller.starCount(), qint64(0));
    QCOMPARE(controller.message(), QString());
    QVERIFY(!controller.anonymous());
    QVERIFY(!controller.valid());
    QVERIFY(controller.request().isEmpty());
}

void GiftSendControllerTest::resetIsNoOpWhenEmpty()
{
    GiftSendController controller;
    QSignalSpy spy(&controller, &GiftSendController::changed);

    controller.reset();
    controller.reset();

    QCOMPARE(spy.count(), 0);

    // The form is empty again after reset, so a second reset stays silent.
    controller.setRecipient(42, 0);
    controller.reset();
    QCOMPARE(spy.count(), 2);
    controller.reset();
    QCOMPARE(spy.count(), 2);
}

void GiftSendControllerTest::requestBuildsSendGift()
{
    GiftSendController controller;
    makeReady(controller, 42, 7);
    controller.setMessage(QStringLiteral("  enjoy  "));

    const QJsonObject request = controller.request();

    QCOMPARE(typeOf(request), QStringLiteral("sendGift"));
    QCOMPARE(giftIdOf(request), qint64(123456));
    QCOMPARE(ownerOf(request).value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(ownerUserIdOf(request), qint64(42));

    QCOMPARE(textStringOf(request), QStringLiteral("enjoy"));
    QVERIFY(textOf(request).value(QStringLiteral("entities")).isArray());
    QVERIFY(entitiesOf(request).isEmpty());

    QVERIFY(!isPrivateOf(request));
    QVERIFY(!payForUpgradeOf(request));

    // The star count is display state: sendGift prices the gift by id.
    QVERIFY(!request.contains(QStringLiteral("star_count")));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void GiftSendControllerTest::requestUsesUserIdWithoutChatId()
{
    GiftSendController controller;
    makeReady(controller, 0, 7);

    QCOMPARE(ownerUserIdOf(controller.request()), qint64(7));

    // A negative chat id cannot own a gift, so the user id is the recipient.
    controller.setRecipient(-1001234567890LL, 7);
    QCOMPARE(ownerUserIdOf(controller.request()), qint64(7));
}

void GiftSendControllerTest::requestPrefersChatIdOverUserId()
{
    GiftSendController controller;
    makeReady(controller, 42, 7);

    QCOMPARE(ownerUserIdOf(controller.request()), qint64(42));

    controller.setRecipient(9876543210LL, 7);
    QCOMPARE(ownerUserIdOf(controller.request()), qint64(9876543210LL));
}

void GiftSendControllerTest::requestMarksAnonymousAsPrivate()
{
    GiftSendController controller;
    makeReady(controller);
    QVERIFY(!isPrivateOf(controller.request()));

    controller.setAnonymous(true);
    QVERIFY(isPrivateOf(controller.request()));

    // Anonymity is the only field the controller overrides.
    QVERIFY(!payForUpgradeOf(controller.request()));
    QCOMPARE(typeOf(controller.request()), QStringLiteral("sendGift"));
    QCOMPARE(giftIdOf(controller.request()), qint64(123456));
}

void GiftSendControllerTest::requestIsEmptyWhenInvalid()
{
    GiftSendController controller;
    QVERIFY(controller.request().isEmpty());

    controller.setRecipient(42, 7);
    QVERIFY(controller.request().isEmpty());

    controller.setGift(123456, 0);
    QVERIFY(controller.request().isEmpty());

    controller.setGift(0, 50);
    QVERIFY(controller.request().isEmpty());

    controller.setGift(-3, 50);
    QVERIFY(controller.request().isEmpty());

    // Only a complete form builds a request.
    controller.setGift(123456, 50);
    QVERIFY(!controller.request().isEmpty());

    controller.setRecipient(0, 0);
    QVERIFY(controller.request().isEmpty());
}

void GiftSendControllerTest::emitsChangedOnlyOnRealChanges()
{
    GiftSendController controller;
    QSignalSpy spy(&controller, &GiftSendController::changed);

    controller.setRecipient(42, 0);       // 1
    controller.setRecipient(42, 0);       // no-op
    controller.setGift(5, 10);            // 2
    controller.setGift(5, 10);            // no-op
    controller.setGift(5, -3);            // clamps to 0 -> 3
    controller.setMessage(QStringLiteral(" hi "));   // 4
    controller.setMessage(QStringLiteral("hi"));     // no-op
    controller.setAnonymous(true);        // 5
    controller.setAnonymous(true);        // no-op
    controller.setMessage(QString());     // 6
    controller.setRecipient(42, 7);       // 7
    controller.setGift(-5, 10);           // 8
    controller.setAnonymous(false);       // 9
    controller.reset();                   // 10
    controller.reset();                   // no-op

    QCOMPARE(spy.count(), 10);
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.giftId(), qint64(0));
    QCOMPARE(controller.message(), QString());
    QVERIFY(!controller.anonymous());
}

QTEST_GUILESS_MAIN(GiftSendControllerTest)
#include "tst_gift_send_controller.moc"
