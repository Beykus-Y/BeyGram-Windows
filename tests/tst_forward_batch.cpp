#include "telegram/ForwardBatch.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

#include <utility>

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

qint64 chatIdOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("chat_id")).toVariant().toLongLong();
}

ForwardBatchPlan planWith(qint64 fromChatId, QVector<qint64> messageIds,
                          QVector<qint64> targetChatIds)
{
    ForwardBatchPlan plan;
    plan.fromChatId = fromChatId;
    plan.messageIds = std::move(messageIds);
    plan.targetChatIds = std::move(targetChatIds);
    return plan;
}

QVector<qint64> sequentialIds(qint64 first, int count)
{
    QVector<qint64> ids;
    ids.reserve(count);
    for (int index = 0; index < count; ++index)
        ids.append(first + index);
    return ids;
}

} // namespace

class ForwardBatchTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsValidPlan();
    void acceptsDuplicatesWithinLimits();
    void rejectsMissingSource();
    void rejectsMissingMessages();
    void rejectsMissingTargets();
    void rejectsTooManyMessages();
    void rejectsTooManyTargets();
    void validationOrderMatchesContract();
    void buildReturnsFirstTarget();
    void buildReturnsEmptyForInvalidPlan();
    void buildAllBuildsOneRequestPerTarget();
    void buildAllNormalisesIds();
    void buildAllHonoursDropFlags();
    void buildAllReturnsEmptyForInvalidPlan();
    void acceptsSchemaLimits();
};

void ForwardBatchTest::acceptsValidPlan()
{
    QCOMPARE(ForwardBatchPlanner::validate(planWith(-1001, {10, 11}, {42, 43})), QString());
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, {1}, {500})), QString());
}

void ForwardBatchTest::acceptsDuplicatesWithinLimits()
{
    QVector<qint64> messages = sequentialIds(1, 100);
    messages << 1 << 2 << 0 << -5;
    QVector<qint64> targets = sequentialIds(500, 20);
    targets << 500 << 501 << 0 << -1;

    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, messages, targets)), QString());
}

void ForwardBatchTest::rejectsMissingSource()
{
    QCOMPARE(ForwardBatchPlanner::validate(planWith(0, {10}, {42})),
             QStringLiteral("no-source"));
    QCOMPARE(ForwardBatchPlanner::validate(planWith(0, {}, {})),
             QStringLiteral("no-source"));
}

void ForwardBatchTest::rejectsMissingMessages()
{
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, {}, {42})),
             QStringLiteral("no-messages"));
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, {0, -1, -2}, {42})),
             QStringLiteral("no-messages"));
}

void ForwardBatchTest::rejectsMissingTargets()
{
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, {10}, {})),
             QStringLiteral("no-targets"));
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, {10}, {0, -4})),
             QStringLiteral("no-targets"));
}

void ForwardBatchTest::rejectsTooManyMessages()
{
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, sequentialIds(1, 101), {42})),
             QStringLiteral("too-many-messages"));
}

void ForwardBatchTest::rejectsTooManyTargets()
{
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, {10}, sequentialIds(500, 21))),
             QStringLiteral("too-many-targets"));
}

void ForwardBatchTest::validationOrderMatchesContract()
{
    QCOMPARE(ForwardBatchPlanner::validate(planWith(0, {}, {})),
             QStringLiteral("no-source"));
    QCOMPARE(ForwardBatchPlanner::validate(planWith(7, {}, {})),
             QStringLiteral("no-messages"));

    const ForwardBatchPlan bothOverLimit =
        planWith(7, sequentialIds(1, 101), sequentialIds(500, 21));
    QCOMPARE(ForwardBatchPlanner::validate(bothOverLimit),
             QStringLiteral("too-many-messages"));
}

void ForwardBatchTest::buildReturnsFirstTarget()
{
    const ForwardBatchPlan plan = planWith(-1001, {0, 10, 11, 10}, {42, 43});
    const QJsonObject request = serialized(ForwardBatchPlanner::build(plan));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("forwardMessages"));
    QCOMPARE(chatIdOf(request), qint64(42));
    QCOMPARE(request.value(QStringLiteral("from_chat_id")).toVariant().toLongLong(),
             qint64(-1001));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QCOMPARE(idsOf(request), (QVector<qint64>{10, 11}));
    QVERIFY(!request.value(QStringLiteral("send_copy")).toBool());
    QVERIFY(!request.value(QStringLiteral("remove_caption")).toBool());

    const QJsonObject options = request.value(QStringLiteral("options")).toObject();
    QCOMPARE(options.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSendOptions"));
    QVERIFY(!options.value(QStringLiteral("disable_notification")).toBool());
    QVERIFY(!options.value(QStringLiteral("only_preview")).toBool());

    QCOMPARE(serialized(ForwardBatchPlanner::build(plan)),
             serialized(ForwardBatchPlanner::buildAll(plan).first()));
}

void ForwardBatchTest::buildReturnsEmptyForInvalidPlan()
{
    QVERIFY(ForwardBatchPlanner::build(planWith(0, {10}, {42})).isEmpty());
    QVERIFY(ForwardBatchPlanner::build(planWith(7, {}, {42})).isEmpty());
    QVERIFY(ForwardBatchPlanner::build(planWith(7, {10}, {})).isEmpty());
    QVERIFY(ForwardBatchPlanner::build(planWith(7, sequentialIds(1, 101), {42})).isEmpty());
    QVERIFY(ForwardBatchPlanner::build(planWith(7, {10}, sequentialIds(500, 21))).isEmpty());

    const ForwardBatchPlan invalid = planWith(7, {}, {42});
    QVERIFY(ForwardBatchPlanner::build(invalid, true, true).isEmpty());
}

void ForwardBatchTest::buildAllBuildsOneRequestPerTarget()
{
    const ForwardBatchPlan plan = planWith(-1001, {10, 11}, {42, 43, 44});
    const QVector<QJsonObject> requests = ForwardBatchPlanner::buildAll(plan);

    QCOMPARE(requests.size(), qsizetype(3));

    QVector<qint64> targets;
    targets.reserve(requests.size());
    for (const QJsonObject& request : requests) {
        QCOMPARE(request.value(QStringLiteral("@type")).toString(),
                 QStringLiteral("forwardMessages"));
        QCOMPARE(request.value(QStringLiteral("from_chat_id")).toVariant().toLongLong(),
                 qint64(-1001));
        QCOMPARE(idsOf(request), (QVector<qint64>{10, 11}));
        targets.append(chatIdOf(request));
    }
    QCOMPARE(targets, (QVector<qint64>{42, 43, 44}));
}

void ForwardBatchTest::buildAllNormalisesIds()
{
    const ForwardBatchPlan plan = planWith(7, {0, -1, 8, 8, 9}, {0, 500, -2, 500, 501});
    const QVector<QJsonObject> requests = ForwardBatchPlanner::buildAll(plan);

    QCOMPARE(requests.size(), qsizetype(2));
    QCOMPARE(idsOf(requests.at(0)), (QVector<qint64>{8, 9}));
    QCOMPARE(chatIdOf(requests.at(0)), qint64(500));
    QCOMPARE(idsOf(requests.at(1)), (QVector<qint64>{8, 9}));
    QCOMPARE(chatIdOf(requests.at(1)), qint64(501));
}

void ForwardBatchTest::buildAllHonoursDropFlags()
{
    const ForwardBatchPlan plan = planWith(7, {10}, {42, 43});

    const QVector<QJsonObject> plain = ForwardBatchPlanner::buildAll(plan);
    for (const QJsonObject& request : plain) {
        QVERIFY(!request.value(QStringLiteral("send_copy")).toBool());
        QVERIFY(!request.value(QStringLiteral("remove_caption")).toBool());
    }

    const QVector<QJsonObject> droppedAuthor = ForwardBatchPlanner::buildAll(plan, true, false);
    for (const QJsonObject& request : droppedAuthor) {
        QVERIFY(request.value(QStringLiteral("send_copy")).toBool());
        QVERIFY(!request.value(QStringLiteral("remove_caption")).toBool());
    }

    const QVector<QJsonObject> droppedCaptions = ForwardBatchPlanner::buildAll(plan, false, true);
    for (const QJsonObject& request : droppedCaptions) {
        QVERIFY(!request.value(QStringLiteral("send_copy")).toBool());
        QVERIFY(request.value(QStringLiteral("remove_caption")).toBool());
    }

    const QVector<QJsonObject> both = ForwardBatchPlanner::buildAll(plan, true, true);
    for (const QJsonObject& request : both) {
        QVERIFY(request.value(QStringLiteral("send_copy")).toBool());
        QVERIFY(request.value(QStringLiteral("remove_caption")).toBool());
    }

    const QJsonObject single = ForwardBatchPlanner::build(plan, true, true);
    QVERIFY(single.value(QStringLiteral("send_copy")).toBool());
    QVERIFY(single.value(QStringLiteral("remove_caption")).toBool());
}

void ForwardBatchTest::buildAllReturnsEmptyForInvalidPlan()
{
    QVERIFY(ForwardBatchPlanner::buildAll(planWith(0, {10}, {42})).isEmpty());
    QVERIFY(ForwardBatchPlanner::buildAll(planWith(7, {}, {42})).isEmpty());
    QVERIFY(ForwardBatchPlanner::buildAll(planWith(7, {10}, {})).isEmpty());
    QVERIFY(ForwardBatchPlanner::buildAll(planWith(7, sequentialIds(1, 101), {42})).isEmpty());
    QVERIFY(ForwardBatchPlanner::buildAll(planWith(7, {10}, sequentialIds(500, 21))).isEmpty());
}

void ForwardBatchTest::acceptsSchemaLimits()
{
    const ForwardBatchPlan plan = planWith(7, sequentialIds(1, 100), sequentialIds(500, 20));
    QCOMPARE(ForwardBatchPlanner::validate(plan), QString());
    QCOMPARE(ForwardBatchPlanner::buildAll(plan).size(), qsizetype(20));
}

QTEST_GUILESS_MAIN(ForwardBatchTest)
#include "tst_forward_batch.moc"
