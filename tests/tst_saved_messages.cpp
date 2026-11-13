#include "telegram/SavedMessages.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QPair>
#include <QSignalSpy>
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

SavedMessage item(qint64 chatId, qint64 messageId, qint64 savedAt, const QString& preview)
{
    SavedMessage entry;
    entry.chatId = chatId;
    entry.messageId = messageId;
    entry.savedAt = savedAt;
    entry.preview = preview;
    return entry;
}

QVector<QPair<qint64, qint64>> pairsOf(const QVector<SavedMessage>& items)
{
    QVector<QPair<qint64, qint64>> pairs;
    pairs.reserve(items.size());
    for (const SavedMessage& entry : items)
        pairs.append({entry.chatId, entry.messageId});
    return pairs;
}

} // namespace

class SavedMessagesTest final : public QObject {
    Q_OBJECT

private slots:
    void forwardRequestHasForwardMessagesShape();
    void forwardRequestDropsUnusableIds();
    void savedChatRequestResolvesMyId();
    void setItemsDropsInvalidAndDeduplicates();
    void setItemsSortsNewestFirst();
    void setItemsKeepsArrivalOrderOnFullTies();
    void setItemsIsSilentWhenNothingChanged();
    void appendAddsOnlyUnseenPairs();
    void appendIsSilentWithoutNewPairs();
    void removeDeletesOnlyTheExactPair();
    void clearIsSilentWhenAlreadyEmpty();
    void startsEmpty();
};

void SavedMessagesTest::forwardRequestHasForwardMessagesShape()
{
    const QVector<qint64> input{100, 101, 102};
    const QJsonObject request = serialized(forwardToSavedMessagesRequest(7, input));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("forwardMessages"));
    // Saved Messages is addressed by its chat id, which this signature does not
    // carry: the placeholder must be filled in before the request is sent.
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(0));
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
    QVERIFY(!options.value(QStringLiteral("disable_notification")).toBool());
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

void SavedMessagesTest::forwardRequestDropsUnusableIds()
{
    const QVector<qint64> input{0, -3, 8, 8, 0, 9, -1};
    const QJsonObject request = serialized(forwardToSavedMessagesRequest(7, input));

    const QVector<qint64> expected{8, 9};
    QCOMPARE(idsOf(request), expected);

    const QJsonObject empty = serialized(forwardToSavedMessagesRequest(7, {}));
    QCOMPARE(empty.value(QStringLiteral("@type")).toString(),
             QStringLiteral("forwardMessages"));
    QVERIFY(empty.value(QStringLiteral("message_ids")).isArray());
    QCOMPARE(empty.value(QStringLiteral("message_ids")).toArray().size(), 0);
    QCOMPARE(idsOf(empty), QVector<qint64>{});
}

void SavedMessagesTest::savedChatRequestResolvesMyId()
{
    // td_api.tl has no getSavedMessagesChat: the id is the "my_id" option.
    const QJsonObject request = serialized(getSavedMessagesChatRequest());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("getOption"));
    QCOMPARE(request.value(QStringLiteral("name")).toString(), QStringLiteral("my_id"));
}

void SavedMessagesTest::setItemsDropsInvalidAndDeduplicates()
{
    SavedMessagesModel model;
    QSignalSpy spy(&model, &SavedMessagesModel::changed);

    const QVector<SavedMessage> input{
        item(5, 1, 100, QStringLiteral("first")),
        item(5, 1, 300, QStringLiteral("duplicate-later")),
        item(0, 4, 400, QStringLiteral("no-chat")),
        item(7, -1, 500, QStringLiteral("no-message")),
        item(-2, 6, 600, QStringLiteral("negative-chat")),
        item(5, 3, 200, QStringLiteral("other")),
    };

    model.setItems(input);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 2);

    const QVector<SavedMessage> stored = model.items();
    const QVector<QPair<qint64, qint64>> expectedPairs{{5, 3}, {5, 1}};
    QCOMPARE(pairsOf(stored), expectedPairs);
    // The first copy of (5, 1) wins, so its older savedAt and preview survive.
    QCOMPARE(stored.at(1).savedAt, qint64(100));
    QCOMPARE(stored.at(1).preview, QStringLiteral("first"));

    // Replacing with an input that holds no usable entry empties the list.
    model.setItems({item(0, 1, 100, QStringLiteral("no-chat"))});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.count(), 0);
}

void SavedMessagesTest::setItemsSortsNewestFirst()
{
    SavedMessagesModel model;
    model.setItems({
        item(5, 1, 100, QStringLiteral("oldest")),
        item(5, 3, 200, QStringLiteral("same-date-lower-id")),
        item(6, 9, 200, QStringLiteral("same-date-higher-id")),
        item(6, 2, 900, QStringLiteral("newest")),
    });

    const QVector<QPair<qint64, qint64>> expectedPairs{{6, 2}, {6, 9}, {5, 3}, {5, 1}};
    QCOMPARE(pairsOf(model.items()), expectedPairs);
}

void SavedMessagesTest::setItemsKeepsArrivalOrderOnFullTies()
{
    SavedMessagesModel model;
    model.setItems({
        item(9, 4, 200, QStringLiteral("first-arrival")),
        item(8, 4, 200, QStringLiteral("second-arrival")),
    });

    const QVector<QPair<qint64, qint64>> expectedPairs{{9, 4}, {8, 4}};
    QCOMPARE(pairsOf(model.items()), expectedPairs);
}

void SavedMessagesTest::setItemsIsSilentWhenNothingChanged()
{
    SavedMessagesModel model;
    QSignalSpy spy(&model, &SavedMessagesModel::changed);

    model.setItems({});
    QCOMPARE(spy.count(), 0);

    const QVector<SavedMessage> first{
        item(5, 1, 100, QStringLiteral("one")),
        item(5, 2, 200, QStringLiteral("two")),
    };
    model.setItems(first);
    QCOMPARE(spy.count(), 1);

    // Same entries, reversed input order: normalisation sorts them back.
    model.setItems({first.at(1), first.at(0)});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(pairsOf(model.items()), (QVector<QPair<qint64, qint64>>{{5, 2}, {5, 1}}));

    // A changed preview is observable through items(), so it is a real change.
    model.setItems({item(5, 1, 100, QStringLiteral("one-edited")), first.at(1)});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.items().at(1).preview, QStringLiteral("one-edited"));

    // A changed savedAt reorders the list and is a real change as well.
    model.setItems({item(5, 1, 300, QStringLiteral("one-edited")), first.at(1)});
    QCOMPARE(spy.count(), 3);
    QCOMPARE(pairsOf(model.items()), (QVector<QPair<qint64, qint64>>{{5, 1}, {5, 2}}));
}

void SavedMessagesTest::appendAddsOnlyUnseenPairs()
{
    SavedMessagesModel model;
    model.setItems({item(5, 1, 100, QStringLiteral("stored"))});

    QSignalSpy spy(&model, &SavedMessagesModel::changed);
    model.append({
        item(5, 1, 999, QStringLiteral("must-not-replace")),
        item(5, 2, 200, QStringLiteral("second")),
        item(5, 2, 300, QStringLiteral("duplicate-in-batch")),
        item(0, 7, 400, QStringLiteral("invalid")),
    });

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 2);

    const QVector<SavedMessage> stored = model.items();
    const QVector<QPair<qint64, qint64>> expectedPairs{{5, 2}, {5, 1}};
    QCOMPARE(pairsOf(stored), expectedPairs);
    // The stored pair keeps the copy it already had.
    QCOMPARE(stored.at(1).savedAt, qint64(100));
    QCOMPARE(stored.at(1).preview, QStringLiteral("stored"));
    QCOMPARE(stored.at(0).savedAt, qint64(200));
    QCOMPARE(stored.at(0).preview, QStringLiteral("second"));
}

void SavedMessagesTest::appendIsSilentWithoutNewPairs()
{
    SavedMessagesModel model;
    model.setItems({item(5, 1, 100, QStringLiteral("stored"))});

    QSignalSpy spy(&model, &SavedMessagesModel::changed);

    model.append({});
    QCOMPARE(spy.count(), 0);

    model.append({item(0, 1, 100, QStringLiteral("invalid"))});
    QCOMPARE(spy.count(), 0);

    model.append({item(5, 1, 500, QStringLiteral("seen"))});
    QCOMPARE(spy.count(), 0);

    model.append({item(5, 1, 100, QStringLiteral("stored"))});
    QCOMPARE(spy.count(), 0);

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.items().at(0).savedAt, qint64(100));
}

void SavedMessagesTest::removeDeletesOnlyTheExactPair()
{
    SavedMessagesModel model;
    model.setItems({
        item(5, 1, 100, QStringLiteral("one")),
        item(5, 2, 200, QStringLiteral("two")),
    });

    QSignalSpy spy(&model, &SavedMessagesModel::changed);

    // Same message id in another chat is a different entry.
    QVERIFY(!model.remove(6, 1));
    QCOMPARE(spy.count(), 0);

    QVERIFY(!model.remove(5, 99));
    QCOMPARE(spy.count(), 0);

    QVERIFY(model.remove(5, 1));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 1);
    QCOMPARE(pairsOf(model.items()), (QVector<QPair<qint64, qint64>>{{5, 2}}));

    // Removing the same pair twice is a no-op.
    QVERIFY(!model.remove(5, 1));
    QCOMPARE(spy.count(), 1);

    QVERIFY(model.remove(5, 2));
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.items().isEmpty());
}

void SavedMessagesTest::clearIsSilentWhenAlreadyEmpty()
{
    SavedMessagesModel model;
    QSignalSpy spy(&model, &SavedMessagesModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setItems({item(5, 1, 100, QStringLiteral("one"))});
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.items().isEmpty());

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void SavedMessagesTest::startsEmpty()
{
    SavedMessagesModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.items().isEmpty());
}

QTEST_GUILESS_MAIN(SavedMessagesTest)
#include "tst_saved_messages.moc"
