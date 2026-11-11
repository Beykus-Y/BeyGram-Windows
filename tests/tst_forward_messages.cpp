#include "telegram/ForwardMessages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact)).object();
}

QVector<qint64> idsOf(const QJsonObject& request)
{
    QVector<qint64> ids;
    const QJsonArray array = request.value(QStringLiteral("message_ids")).toArray();
    ids.reserve(array.size());
    for (const QJsonValue& value : array)
        ids.append(value.toVariant().toLongLong());
    return ids;
}

} // namespace

class ForwardMessagesTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsForwardRequestShape();
    void honoursSilentlyFlag();
    void filtersDuplicateAndNonPositiveIds();
    void handlesEmptyIds();
};

void ForwardMessagesTest::buildsForwardRequestShape()
{
    const QVector<qint64> input{100, 101, 102};
    const QJsonObject request = serialized(forwardMessagesRequest(42, 7, input, false));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("forwardMessages"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("from_chat_id")).toVariant().toLongLong(), qint64(7));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(!request.value(QStringLiteral("send_copy")).toBool());
    QVERIFY(!request.value(QStringLiteral("remove_caption")).toBool());

    const QVector<qint64> expected{100, 101, 102};
    QCOMPARE(idsOf(request), expected);

    const QJsonObject options = request.value(QStringLiteral("options")).toObject();
    QCOMPARE(options.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSendOptions"));
    QVERIFY(options.value(QStringLiteral("suggested_post_info")).isNull());
    QVERIFY(!options.value(QStringLiteral("from_background")).toBool());
    QVERIFY(!options.value(QStringLiteral("protect_content")).toBool());
    QVERIFY(!options.value(QStringLiteral("allow_paid_broadcast")).toBool());
    QCOMPARE(options.value(QStringLiteral("paid_message_star_count")).toVariant().toLongLong(),
             qint64(0));
    QVERIFY(!options.value(QStringLiteral("update_order_of_installed_sticker_sets")).toBool());
    QVERIFY(options.value(QStringLiteral("scheduling_state")).isNull());
    QCOMPARE(options.value(QStringLiteral("effect_id")).toVariant().toLongLong(), qint64(0));
    QCOMPARE(options.value(QStringLiteral("sending_id")).toInt(), 0);
    QVERIFY(!options.value(QStringLiteral("only_preview")).toBool());
}

void ForwardMessagesTest::honoursSilentlyFlag()
{
    const QVector<qint64> input{5};

    const QJsonObject loud = serialized(forwardMessagesRequest(42, 7, input));
    QVERIFY(!loud.value(QStringLiteral("options"))
                 .toObject()
                 .value(QStringLiteral("disable_notification"))
                 .toBool());

    const QJsonObject silent = serialized(forwardMessagesRequest(42, 7, input, true));
    QVERIFY(silent.value(QStringLiteral("options"))
                .toObject()
                .value(QStringLiteral("disable_notification"))
                .toBool());
}

void ForwardMessagesTest::filtersDuplicateAndNonPositiveIds()
{
    const QVector<qint64> input{0, -3, 8, 8, 0, 9, -1};
    QVERIFY(canForward(input));

    const QJsonObject request = serialized(forwardMessagesRequest(42, 7, input, false));
    const QVector<qint64> expected{8, 9};
    QCOMPARE(idsOf(request), expected);

    QVERIFY(!canForward(QVector<qint64>{0, -3, 0}));
    QVERIFY(!canForward(QVector<qint64>{}));
    QVERIFY(canForward(QVector<qint64>{8, 8}));
}

void ForwardMessagesTest::handlesEmptyIds()
{
    const QVector<qint64> input;
    QVERIFY(!canForward(input));

    const QJsonObject request = serialized(forwardMessagesRequest(42, 7, input, true));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("forwardMessages"));
    QVERIFY(request.value(QStringLiteral("message_ids")).isArray());
    QCOMPARE(request.value(QStringLiteral("message_ids")).toArray().size(), 0);
    QCOMPARE(idsOf(request), QVector<qint64>{});
}

QTEST_GUILESS_MAIN(ForwardMessagesTest)
#include "tst_forward_messages.moc"
