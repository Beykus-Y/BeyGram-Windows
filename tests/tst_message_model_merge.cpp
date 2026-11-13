#include "telegram/MessageHistoryStore.h"
#include "telegram/MessageListModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject photoSize(int width, int height, int fileId)
{
    return QJsonObject{
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height},
        {QStringLiteral("photo"), QJsonObject{{QStringLiteral("id"), fileId}}},
    };
}

QJsonObject photoContent(const QJsonArray& sizes, const QString& caption)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messagePhoto")},
        {QStringLiteral("photo"), QJsonObject{{QStringLiteral("sizes"), sizes}}},
        {QStringLiteral("caption"), QJsonObject{{QStringLiteral("text"), caption}}},
    };
}

QJsonObject textContent(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageText")},
        {QStringLiteral("text"), QJsonObject{{QStringLiteral("text"), text}}},
    };
}

MessageData makeMessage(qint64 id, qint64 date, const QString& text)
{
    MessageData message;
    message.id = id;
    message.chatId = 1;
    message.date = date;
    message.text = text;
    return message;
}

QVector<qint64> idsOf(const MessageHistoryStore& store)
{
    QVector<qint64> ids;
    const QVector<MessageData> messages = store.messages();
    ids.reserve(messages.size());
    for (const MessageData& message : messages)
        ids.append(message.id);
    return ids;
}

QString textOf(const MessageHistoryStore& store, qint64 id)
{
    const QVector<MessageData> messages = store.messages();
    for (const MessageData& message : messages) {
        if (message.id == id)
            return message.text;
    }
    return {};
}

} // namespace

class MessageModelMergeTest final : public QObject {
    Q_OBJECT

private slots:
    void updateContentRetargetsMediaFields();
    void updateContentPreservesEditedFlag();
    void mergeOlderKeepsLiveEditedCopy();
    void mergeOlderIgnoresTombstonedId();
    void mediaFailureIsExposedAndClearedByReady();
};

void MessageModelMergeTest::updateContentRetargetsMediaFields()
{
    MessageListModel model;
    MessageData message;
    message.id = 42;
    message.chatId = 7;
    message.text = QStringLiteral("hello");
    message.contentType = QStringLiteral("text");
    message.mediaReady = true;
    model.addOrUpdate(message);

    // Thumbnail picker targets 320 px wide, the full file is the largest size.
    const QJsonArray sizes{
        photoSize(90, 90, 900),
        photoSize(320, 240, 901),
        photoSize(800, 600, 902),
    };
    QVERIFY(model.updateContent(42, photoContent(sizes, QStringLiteral("caption")), false));

    const QModelIndex row = model.index(0);
    QCOMPARE(model.data(row, MessageListModel::ContentTypeRole).toString(),
             QStringLiteral("photo"));
    QCOMPARE(model.data(row, MessageListModel::MediaFileIdRole).toInt(), 901);
    QCOMPARE(model.data(row, MessageListModel::MediaFullFileIdRole).toInt(), 902);
    QCOMPARE(model.data(row, MessageListModel::MediaWidthRole).toInt(), 320);
    QCOMPARE(model.data(row, MessageListModel::MediaHeightRole).toInt(), 240);
    QCOMPARE(model.data(row, MessageListModel::MediaRevisionRole).toInt(), 1);
    QCOMPARE(model.data(row, MessageListModel::TextRole).toString(),
             QStringLiteral("caption"));
    // New content is not decoded yet, so the ready flag is cleared.
    QVERIFY(!model.data(row, MessageListModel::MediaReadyRole).toBool());

    // A second content change must recompute the attachment fields again and
    // bump the revision rather than reusing the previous photo's ids.
    const QJsonArray resized{
        photoSize(200, 150, 910),
        photoSize(1600, 1200, 911),
    };
    QVERIFY(model.updateContent(42, photoContent(resized, QStringLiteral("caption")), false));
    QCOMPARE(model.data(row, MessageListModel::MediaFileIdRole).toInt(), 910);
    QCOMPARE(model.data(row, MessageListModel::MediaFullFileIdRole).toInt(), 911);
    QCOMPARE(model.data(row, MessageListModel::MediaWidthRole).toInt(), 200);
    QCOMPARE(model.data(row, MessageListModel::MediaHeightRole).toInt(), 150);
    QCOMPARE(model.data(row, MessageListModel::MediaRevisionRole).toInt(), 2);
}

void MessageModelMergeTest::updateContentPreservesEditedFlag()
{
    MessageListModel model;
    MessageData first;
    first.id = 1;
    first.chatId = 1;
    first.text = QStringLiteral("original");
    model.addOrUpdate(first);

    QVERIFY(model.updateContent(1, textContent(QStringLiteral("edited")), true));
    QVERIFY(model.data(model.index(0), MessageListModel::EditedRole).toBool());

    // A later non-edit update must not clear the edited flag.
    QVERIFY(model.updateContent(1, textContent(QStringLiteral("later")), false));
    QVERIFY(model.data(model.index(0), MessageListModel::EditedRole).toBool());

    // Without an edit flag the message stays unedited.
    MessageData second;
    second.id = 2;
    second.chatId = 1;
    second.text = QStringLiteral("plain");
    model.addOrUpdate(second);

    QVERIFY(model.updateContent(2, textContent(QStringLiteral("plain again")), false));
    QVERIFY(!model.data(model.index(1), MessageListModel::EditedRole).toBool());
}

void MessageModelMergeTest::mergeOlderKeepsLiveEditedCopy()
{
    MessageHistoryStore store;
    store.addOrUpdate(makeMessage(100, 1000, QStringLiteral("before edit")));

    store.addOrUpdate(makeMessage(100, 1000, QStringLiteral("after edit")));
    QCOMPARE(textOf(store, 100), QStringLiteral("after edit"));

    // The in-flight older page still carries the pre-edit text; the live copy
    // in the store must win.
    store.mergeOlder({
        makeMessage(50, 500, QStringLiteral("older page")),
        makeMessage(100, 1000, QStringLiteral("before edit")),
    });

    QCOMPARE(store.count(), 2);
    QCOMPARE(idsOf(store), (QVector<qint64>{50, 100}));
    QCOMPARE(textOf(store, 100), QStringLiteral("after edit"));
}

void MessageModelMergeTest::mergeOlderIgnoresTombstonedId()
{
    MessageHistoryStore store;
    store.setMessages({
        makeMessage(100, 1000, QStringLiteral("deleted")),
        makeMessage(200, 2000, QStringLiteral("live")),
    });
    store.removeIds({100});
    QVERIFY(!store.contains(100));

    // A late page cannot resurrect the removed message, but its other rows
    // are still merged.
    store.mergeOlder({
        makeMessage(50, 500, QStringLiteral("older page")),
        makeMessage(100, 1000, QStringLiteral("deleted")),
    });

    QCOMPARE(store.count(), 2);
    QCOMPARE(idsOf(store), (QVector<qint64>{50, 200}));
    QVERIFY(!store.contains(100));
}

void MessageModelMergeTest::mediaFailureIsExposedAndClearedByReady()
{
    MessageListModel model;
    MessageData message;
    message.id = 7;
    message.chatId = 1;
    message.contentType = QStringLiteral("photo");
    message.mediaFileId = 500;
    model.addOrUpdate(message);

    QCOMPARE(model.index(0).data(MessageListModel::MediaFailedRole).toBool(), false);
    model.markMediaFailed(500);
    QCOMPARE(model.index(0).data(MessageListModel::MediaFailedRole).toBool(), true);
    QCOMPARE(model.index(0).data(MessageListModel::MediaReadyRole).toBool(), false);

    // A late success wins over an earlier failure and clears the flag.
    model.markMediaReady(500);
    QCOMPARE(model.index(0).data(MessageListModel::MediaReadyRole).toBool(), true);
    QCOMPARE(model.index(0).data(MessageListModel::MediaFailedRole).toBool(), false);
}

QTEST_GUILESS_MAIN(MessageModelMergeTest)
#include "tst_message_model_merge.moc"
