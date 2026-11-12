#include "telegram/MessageSendOptions.h"

#include <QJsonObject>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

const QStringList& requiredOptionFields()
{
    static const QStringList fields{
        QStringLiteral("@type"),
        QStringLiteral("suggested_post_info"),
        QStringLiteral("disable_notification"),
        QStringLiteral("from_background"),
        QStringLiteral("protect_content"),
        QStringLiteral("allow_paid_broadcast"),
        QStringLiteral("paid_message_star_count"),
        QStringLiteral("update_order_of_installed_sticker_sets"),
        QStringLiteral("scheduling_state"),
        QStringLiteral("effect_id"),
        QStringLiteral("sending_id"),
        QStringLiteral("only_preview"),
    };
    return fields;
}

QJsonObject schedulingStateOf(const SendOptions& options)
{
    return messageSendOptionsObject(options)
        .value(QStringLiteral("scheduling_state"))
        .toObject();
}

QString stateTypeOf(const SendOptions& options)
{
    return schedulingStateOf(options).value(QStringLiteral("@type")).toString();
}

} // namespace

class MessageSendOptionsTest final : public QObject {
    Q_OBJECT

private slots:
    void hasCompleteNeutralShape();
    void mapsSwitchesToFields();
    void scheduleDateUsesSendAtDate_data();
    void scheduleDateUsesSendAtDate();
    void onlineUsesSendWhenOnline();
    void scheduleDateWinsOverOnline();
    void noSchedulingWithoutDateOrOnline();
    void withSendOptionsReplacesOptions();
    void withSendOptionsKeepsOtherFields();
    void withSendOptionsCarriesSchedulingState();
    void withSendOptionsLeavesEmptyRequestEmpty();
};

void MessageSendOptionsTest::hasCompleteNeutralShape()
{
    const QJsonObject options = messageSendOptionsObject(SendOptions{});

    QCOMPARE(options.size(), requiredOptionFields().size());
    for (const QString& field : requiredOptionFields())
        QVERIFY2(options.contains(field), qPrintable(field));

    QCOMPARE(options.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSendOptions"));
    QVERIFY(options.value(QStringLiteral("suggested_post_info")).isNull());
    QVERIFY(!options.value(QStringLiteral("disable_notification")).toBool());
    QVERIFY(!options.value(QStringLiteral("from_background")).toBool());
    QVERIFY(!options.value(QStringLiteral("protect_content")).toBool());
    QVERIFY(!options.value(QStringLiteral("allow_paid_broadcast")).toBool());
    QCOMPARE(options.value(QStringLiteral("paid_message_star_count")).toVariant().toLongLong(),
             qint64(0));
    QVERIFY(!options.value(QStringLiteral("update_order_of_installed_sticker_sets")).toBool());
    QVERIFY(options.value(QStringLiteral("scheduling_state")).isNull());
    QCOMPARE(options.value(QStringLiteral("effect_id")).toVariant().toLongLong(),
             qint64(0));
    QCOMPARE(options.value(QStringLiteral("sending_id")).toInt(), 0);
    QVERIFY(!options.value(QStringLiteral("only_preview")).toBool());
}

void MessageSendOptionsTest::mapsSwitchesToFields()
{
    SendOptions options;
    options.silent = true;
    options.background = true;
    options.protectContent = true;

    const QJsonObject object = messageSendOptionsObject(options);
    QVERIFY(object.value(QStringLiteral("disable_notification")).toBool());
    QVERIFY(object.value(QStringLiteral("from_background")).toBool());
    QVERIFY(object.value(QStringLiteral("protect_content")).toBool());

    // Switches this struct does not expose stay neutral.
    QVERIFY(!object.value(QStringLiteral("allow_paid_broadcast")).toBool());
    QVERIFY(!object.value(QStringLiteral("update_order_of_installed_sticker_sets")).toBool());
    QVERIFY(!object.value(QStringLiteral("only_preview")).toBool());
    QVERIFY(object.value(QStringLiteral("suggested_post_info")).isNull());
    QVERIFY(object.value(QStringLiteral("scheduling_state")).isNull());
}

void MessageSendOptionsTest::scheduleDateUsesSendAtDate_data()
{
    QTest::addColumn<qint64>("date");

    QTest::newRow("one second after epoch") << qint64(1);
    QTest::newRow("ordinary date") << qint64(1700000000);
    QTest::newRow("int32 maximum") << qint64(2147483647);
}

void MessageSendOptionsTest::scheduleDateUsesSendAtDate()
{
    QFETCH(qint64, date);

    SendOptions options;
    options.scheduleDate = date;

    const QJsonObject state = schedulingStateOf(options);
    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSchedulingStateSendAtDate"));
    QCOMPARE(state.value(QStringLiteral("send_date")).toVariant().toLongLong(), date);
    QCOMPARE(state.value(QStringLiteral("repeat_period")).toInt(), 0);
    QCOMPARE(state.size(), 3);
}

void MessageSendOptionsTest::onlineUsesSendWhenOnline()
{
    SendOptions options;
    options.sendWhenOnline = true;

    const QJsonObject state = schedulingStateOf(options);
    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSchedulingStateSendWhenOnline"));
    // This state has no fields beyond @type.
    QCOMPARE(state.size(), 1);

    // A non-positive date is "no date" and must not hide the online choice.
    options.scheduleDate = 0;
    QCOMPARE(stateTypeOf(options), QStringLiteral("messageSchedulingStateSendWhenOnline"));
    options.scheduleDate = -10;
    QCOMPARE(stateTypeOf(options), QStringLiteral("messageSchedulingStateSendWhenOnline"));
}

void MessageSendOptionsTest::scheduleDateWinsOverOnline()
{
    SendOptions options;
    options.scheduleDate = 1700000000;
    options.sendWhenOnline = true;

    const QJsonObject state = schedulingStateOf(options);
    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSchedulingStateSendAtDate"));
    QCOMPARE(state.value(QStringLiteral("send_date")).toVariant().toLongLong(),
             qint64(1700000000));
}

void MessageSendOptionsTest::noSchedulingWithoutDateOrOnline()
{
    QVERIFY(messageSendOptionsObject(SendOptions{})
                .value(QStringLiteral("scheduling_state"))
                .isNull());

    SendOptions zeroDate;
    zeroDate.scheduleDate = 0;
    QVERIFY(messageSendOptionsObject(zeroDate)
                .value(QStringLiteral("scheduling_state"))
                .isNull());

    SendOptions negativeDate;
    negativeDate.scheduleDate = -1;
    QVERIFY(messageSendOptionsObject(negativeDate)
                .value(QStringLiteral("scheduling_state"))
                .isNull());
}

void MessageSendOptionsTest::withSendOptionsReplacesOptions()
{
    const QJsonObject request{
        {QStringLiteral("@type"), QStringLiteral("sendMessage")},
        {QStringLiteral("chat_id"), 42},
        {QStringLiteral("options"), QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("messageSendOptions")},
             {QStringLiteral("disable_notification"), false},
         }},
    };

    SendOptions options;
    options.silent = true;
    options.protectContent = true;

    const QJsonObject result = withSendOptions(request, options);
    QCOMPARE(result.value(QStringLiteral("options")).toObject(),
             messageSendOptionsObject(options));
    QVERIFY(result.value(QStringLiteral("options"))
                .toObject()
                .value(QStringLiteral("disable_notification"))
                .toBool());

    // The input is a value copy and must not be modified.
    QVERIFY(!request.value(QStringLiteral("options"))
                 .toObject()
                 .value(QStringLiteral("disable_notification"))
                 .toBool());
}

void MessageSendOptionsTest::withSendOptionsKeepsOtherFields()
{
    const QJsonObject request{
        {QStringLiteral("@type"), QStringLiteral("sendMessageAlbum")},
        {QStringLiteral("chat_id"), 99},
        {QStringLiteral("topic_id"), 7},
    };

    const QJsonObject result = withSendOptions(request, SendOptions{});
    QCOMPARE(result.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessageAlbum"));
    QCOMPARE(result.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(99));
    QCOMPARE(result.value(QStringLiteral("topic_id")).toVariant().toLongLong(), qint64(7));
    QVERIFY(result.value(QStringLiteral("options")).isObject());
}

void MessageSendOptionsTest::withSendOptionsCarriesSchedulingState()
{
    QJsonObject request{
        {QStringLiteral("@type"), QStringLiteral("forwardMessages")},
        {QStringLiteral("chat_id"), 5},
    };

    SendOptions options;
    options.scheduleDate = 1700000000;

    const QJsonObject result = withSendOptions(request, options);
    const QJsonObject state = result.value(QStringLiteral("options"))
                                  .toObject()
                                  .value(QStringLiteral("scheduling_state"))
                                  .toObject();
    QCOMPARE(state.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSchedulingStateSendAtDate"));
    QCOMPARE(state.value(QStringLiteral("send_date")).toVariant().toLongLong(),
             qint64(1700000000));
    QCOMPARE(state.value(QStringLiteral("repeat_period")).toInt(), 0);
}

void MessageSendOptionsTest::withSendOptionsLeavesEmptyRequestEmpty()
{
    QVERIFY(withSendOptions(QJsonObject{}, SendOptions{}).isEmpty());

    SendOptions scheduled;
    scheduled.scheduleDate = 1700000000;
    QVERIFY(withSendOptions(QJsonObject{}, scheduled).isEmpty());
}

QTEST_GUILESS_MAIN(MessageSendOptionsTest)
#include "tst_message_send_options.moc"
