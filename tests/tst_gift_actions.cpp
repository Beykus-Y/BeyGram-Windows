#include "telegram/GiftActions.h"
#include "telegram/GiftRequests.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class GiftActionsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsUpgradePreviewRequest();
    void buildsUpgradeGiftRequest();
    void buildsTransferGiftRequest();
    void buildsConvertGiftToStarsRequest();
    void buildsReceivedGiftsRequest();
    void buildsAllReceivedGiftsRequest();
    void clampsReceivedGiftsLimit();
    void buildsSentGiftsRequest();
    void clampsSentGiftsLimit();
    void buildsToggleGiftIsSavedRequest();
    void rejectsNonPositiveIds();
    void preservesLargeIdsThroughJson();
};

void GiftActionsTest::buildsUpgradePreviewRequest()
{
    const QJsonObject request = getGiftUpgradePreviewRequest(555);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getGiftUpgradePreview"));
    QCOMPARE(numberAt(request, QStringLiteral("gift_id")), qint64(555));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QJsonObject encoded = serialized(request);
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getGiftUpgradePreview"));
    QCOMPARE(numberAt(encoded, QStringLiteral("gift_id")), qint64(555));
}

void GiftActionsTest::buildsUpgradeGiftRequest()
{
    const QJsonObject keep = upgradeGiftRequest(42, true);
    QCOMPARE(keep.value(QStringLiteral("@type")).toString(),
             QStringLiteral("upgradeGift"));
    QCOMPARE(numberAt(keep, QStringLiteral("gift_id")), qint64(42));
    QCOMPARE(keep.value(QStringLiteral("keep_original_details")).toBool(false),
             true);
    QCOMPARE(keep.size(), 3);

    const QJsonObject drop = upgradeGiftRequest(42, false);
    QCOMPARE(drop.value(QStringLiteral("keep_original_details")).toBool(true),
             false);
    QCOMPARE(numberAt(drop, QStringLiteral("gift_id")), qint64(42));
}

void GiftActionsTest::buildsTransferGiftRequest()
{
    const QJsonObject request = transferGiftRequest(555, 777);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("transferGift"));
    QCOMPARE(numberAt(request, QStringLiteral("gift_id")), qint64(555));
    QCOMPARE(request.size(), 3);

    // The recipient is a MessageSender, never a bare chat_id.
    QVERIFY(!request.contains(QStringLiteral("chat_id")));
    const QJsonObject owner =
        request.value(QStringLiteral("new_owner_id")).toObject();
    QCOMPARE(owner.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(numberAt(owner, QStringLiteral("user_id")), qint64(777));
    QCOMPARE(owner.size(), 2);
}

void GiftActionsTest::buildsConvertGiftToStarsRequest()
{
    const QJsonObject request = convertGiftToStarsRequest(555);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("convertGiftToStars"));
    QCOMPARE(numberAt(request, QStringLiteral("gift_id")), qint64(555));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void GiftActionsTest::buildsReceivedGiftsRequest()
{
    const QJsonObject request = getReceivedGiftsRequest(777, 30);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getReceivedGifts"));
    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(30));
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
    QCOMPARE(request.size(), 15);

    const QJsonObject owner =
        request.value(QStringLiteral("owner_id")).toObject();
    QCOMPARE(owner.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(numberAt(owner, QStringLiteral("user_id")), qint64(777));

    // A positive chat id is exactly the existing GiftRequests builder.
    const QJsonObject legacy =
        beygram::telegram::getReceivedGiftsRequest(777, 0, 30);
    QCOMPARE(request.value(QStringLiteral("offset")).toString(),
             legacy.value(QStringLiteral("offset")).toString());
    QCOMPARE(numberAt(request, QStringLiteral("limit")),
             numberAt(legacy, QStringLiteral("limit")));
    QCOMPARE(numberAt(request.value(QStringLiteral("owner_id")).toObject(),
                      QStringLiteral("user_id")),
             numberAt(legacy.value(QStringLiteral("owner_id")).toObject(),
                      QStringLiteral("user_id")));
    QCOMPARE(request.value(QStringLiteral("exclude_saved")).toBool(true),
             legacy.value(QStringLiteral("exclude_saved")).toBool(false));
    QCOMPARE(request.value(QStringLiteral("sort_by_price")).toBool(true),
             legacy.value(QStringLiteral("sort_by_price")).toBool(false));
}

void GiftActionsTest::buildsAllReceivedGiftsRequest()
{
    const QJsonObject request = getReceivedGiftsRequest(0, 30);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getReceivedGifts"));
    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(30));
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
    QVERIFY(!request.contains(QStringLiteral("owner_id")));
    QCOMPARE(request.size(), 14);

    // Zero is the account-wide form; a negative chat id names nothing.
    QVERIFY(getReceivedGiftsRequest(-1, 30).isEmpty());
    QVERIFY(getReceivedGiftsRequest(-999, 30).isEmpty());
}

void GiftActionsTest::clampsReceivedGiftsLimit()
{
    const QVector<QPair<int, int>> cases{
        {0, 1},
        {-7, 1},
        {1, 1},
        {30, 30},
        {100, 100},
        {101, 100},
        {5000, 100},
    };

    for (const auto& testCase : cases) {
        const QJsonObject named = getReceivedGiftsRequest(42, testCase.first);
        QCOMPARE(numberAt(named, QStringLiteral("limit")),
                 static_cast<qint64>(testCase.second));

        const QJsonObject all = getReceivedGiftsRequest(0, testCase.first);
        QCOMPARE(numberAt(all, QStringLiteral("limit")),
                 static_cast<qint64>(testCase.second));
    }
}

void GiftActionsTest::buildsSentGiftsRequest()
{
    const QJsonObject request = getSentGiftsRequest(30);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getSentGifts"));
    QVERIFY(request.value(QStringLiteral("offset")).isString());
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());
    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(30));
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("owner_id")));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void GiftActionsTest::clampsSentGiftsLimit()
{
    const QVector<QPair<int, int>> cases{
        {0, 1},
        {-3, 1},
        {1, 1},
        {50, 50},
        {100, 100},
        {101, 100},
        {9999, 100},
    };

    for (const auto& testCase : cases) {
        const QJsonObject request = getSentGiftsRequest(testCase.first);
        QCOMPARE(numberAt(request, QStringLiteral("limit")),
                 static_cast<qint64>(testCase.second));
    }
}

void GiftActionsTest::buildsToggleGiftIsSavedRequest()
{
    const QJsonObject saved = toggleGiftIsSavedRequest(555, true);
    QCOMPARE(saved.value(QStringLiteral("@type")).toString(),
             QStringLiteral("toggleGiftIsSaved"));
    QCOMPARE(numberAt(saved, QStringLiteral("gift_id")), qint64(555));
    QCOMPARE(saved.value(QStringLiteral("is_saved")).toBool(false), true);
    QCOMPARE(saved.size(), 3);

    const QJsonObject hidden = toggleGiftIsSavedRequest(555, false);
    QCOMPARE(hidden.value(QStringLiteral("is_saved")).toBool(true), false);
    QCOMPARE(numberAt(hidden, QStringLiteral("gift_id")), qint64(555));
}

void GiftActionsTest::rejectsNonPositiveIds()
{
    QVERIFY(getGiftUpgradePreviewRequest(0).isEmpty());
    QVERIFY(getGiftUpgradePreviewRequest(-1).isEmpty());

    QVERIFY(upgradeGiftRequest(0, true).isEmpty());
    QVERIFY(upgradeGiftRequest(-5, false).isEmpty());

    QVERIFY(transferGiftRequest(0, 1).isEmpty());
    QVERIFY(transferGiftRequest(-1, 1).isEmpty());
    QVERIFY(transferGiftRequest(1, 0).isEmpty());
    QVERIFY(transferGiftRequest(1, -2).isEmpty());

    QVERIFY(convertGiftToStarsRequest(0).isEmpty());
    QVERIFY(convertGiftToStarsRequest(-3).isEmpty());

    QVERIFY(toggleGiftIsSavedRequest(0, true).isEmpty());
    QVERIFY(toggleGiftIsSavedRequest(-4, false).isEmpty());
}

void GiftActionsTest::preservesLargeIdsThroughJson()
{
    const qint64 giftId = 1234567890123LL;
    const qint64 ownerId = 987654321098LL;

    const QJsonObject preview =
        serialized(getGiftUpgradePreviewRequest(giftId));
    QCOMPARE(numberAt(preview, QStringLiteral("gift_id")), giftId);

    const QJsonObject upgrade =
        serialized(upgradeGiftRequest(giftId, true));
    QCOMPARE(numberAt(upgrade, QStringLiteral("gift_id")), giftId);

    const QJsonObject transfer =
        serialized(transferGiftRequest(giftId, ownerId));
    QCOMPARE(numberAt(transfer, QStringLiteral("gift_id")), giftId);
    QCOMPARE(numberAt(transfer.value(QStringLiteral("new_owner_id")).toObject(),
                      QStringLiteral("user_id")),
             ownerId);

    const QJsonObject toggle =
        serialized(toggleGiftIsSavedRequest(giftId, true));
    QCOMPARE(numberAt(toggle, QStringLiteral("gift_id")), giftId);
}

QTEST_GUILESS_MAIN(GiftActionsTest)
#include "tst_gift_actions.moc"
