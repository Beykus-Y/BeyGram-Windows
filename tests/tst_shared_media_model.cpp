#include "telegram/SharedMediaModel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

SharedMediaItem makeItem(qint64 messageId, qint64 date, const QString& title = {})
{
    SharedMediaItem item;
    item.messageId = messageId;
    item.date = date;
    item.title = title;
    return item;
}

QVector<qint64> messageIdsOf(const QVector<SharedMediaItem>& items)
{
    QVector<qint64> ids;
    ids.reserve(items.size());
    for (const SharedMediaItem& item : items)
        ids.append(item.messageId);
    return ids;
}

} // namespace

class SharedMediaModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void filterJsonMapsEveryTab_data();
    void filterJsonMapsEveryTab();
    void setItemsSortsNewestFirstByDateThenId();
    void setItemsDedupesByMessageIdKeepingFirst();
    void setItemsKeepsEveryPayloadField();
    void itemsReturnsACopy();
    void setItemsInAnotherOrderIsSilent();
    void setItemsWithChangedPayloadEmits();
    void setItemsEmptyOnEmptyModelIsSilent();
    void setItemsEmptyClearsLoadedItems();
    void appendItemsAddsNewIdsOnlyAndResorts();
    void appendItemsWithOnlyKnownIdsIsSilent();
    void appendItemsToEmptyListWorks();
    void appendItemsDedupesWithinThePage();
    void appendMovesNextCursorOlder();
    void setTabClearsItemsAndEmits();
    void setTabOnEmptyModelEmits();
    void setTabToCurrentTabKeepsItems();
    void clearDropsItemsAndKeepsTab();
    void clearOnEmptyModelIsSilent();
    void changedFiresOnlyOnRealChanges();
};

void SharedMediaModelTest::startsEmpty()
{
    SharedMediaModel model;

    QCOMPARE(static_cast<int>(model.tab()), static_cast<int>(SharedMediaTab::Photos));
    QCOMPARE(model.count(), 0);
    QVERIFY(model.items().isEmpty());
    QCOMPARE(model.nextFromMessageId(), qint64(0));
}

void SharedMediaModelTest::filterJsonMapsEveryTab_data()
{
    QTest::addColumn<int>("tab");
    QTest::addColumn<QString>("expectedType");

    QTest::newRow("photos") << static_cast<int>(SharedMediaTab::Photos)
                            << QStringLiteral("searchMessagesFilterPhoto");
    QTest::newRow("videos") << static_cast<int>(SharedMediaTab::Videos)
                            << QStringLiteral("searchMessagesFilterVideo");
    QTest::newRow("files") << static_cast<int>(SharedMediaTab::Files)
                           << QStringLiteral("searchMessagesFilterDocument");
    QTest::newRow("links") << static_cast<int>(SharedMediaTab::Links)
                           << QStringLiteral("searchMessagesFilterUrl");
    QTest::newRow("voice") << static_cast<int>(SharedMediaTab::Voice)
                           << QStringLiteral("searchMessagesFilterVoiceNote");
    QTest::newRow("music") << static_cast<int>(SharedMediaTab::Music)
                           << QStringLiteral("searchMessagesFilterAudio");
    QTest::newRow("gifs") << static_cast<int>(SharedMediaTab::Gifs)
                          << QStringLiteral("searchMessagesFilterAnimation");
}

void SharedMediaModelTest::filterJsonMapsEveryTab()
{
    QFETCH(int, tab);
    QFETCH(QString, expectedType);

    const QString json = sharedMediaFilterJson(static_cast<SharedMediaTab>(tab));

    // Exact text: one object with a single field and no formatting whitespace.
    QCOMPARE(json, QStringLiteral("{\"@type\":\"%1\"}").arg(expectedType));

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    QVERIFY(document.isObject());

    const QJsonObject filter = document.object();
    QCOMPARE(filter.size(), 1);
    QCOMPARE(filter.value(QStringLiteral("@type")).toString(), expectedType);
}

void SharedMediaModelTest::setItemsSortsNewestFirstByDateThenId()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100), makeItem(2, 300), makeItem(3, 300), makeItem(4, 50)});

    QCOMPARE(messageIdsOf(model.items()), (QVector<qint64>{3, 2, 1, 4}));
    QCOMPARE(model.count(), 4);
    // The last item is the oldest one and therefore the cursor for older pages.
    QCOMPARE(model.nextFromMessageId(), qint64(4));
}

void SharedMediaModelTest::setItemsDedupesByMessageIdKeepingFirst()
{
    SharedMediaModel model;
    model.setItems({makeItem(7, 500, QStringLiteral("first")),
                    makeItem(7, 900, QStringLiteral("duplicate")),
                    makeItem(8, 600, QStringLiteral("other")),
                    makeItem(7, 1000, QStringLiteral("third"))});

    const QVector<SharedMediaItem> items = model.items();

    QCOMPARE(model.count(), 2);
    QCOMPARE(messageIdsOf(items), (QVector<qint64>{8, 7}));
    QCOMPARE(items.constLast().date, qint64(500));
    QCOMPARE(items.constLast().title, QStringLiteral("first"));
}

void SharedMediaModelTest::setItemsKeepsEveryPayloadField()
{
    SharedMediaItem item;
    item.messageId = 42;
    item.date = 1700000000;
    item.title = QStringLiteral("report.pdf");
    item.subtitle = QStringLiteral("2.5 MB, You");
    item.fileId = 777;
    item.sizeBytes = 2621440;
    item.durationSeconds = 12;

    SharedMediaModel model;
    model.setItems({item});

    const SharedMediaItem stored = model.items().constFirst();
    QCOMPARE(stored.messageId, qint64(42));
    QCOMPARE(stored.date, qint64(1700000000));
    QCOMPARE(stored.title, QStringLiteral("report.pdf"));
    QCOMPARE(stored.subtitle, QStringLiteral("2.5 MB, You"));
    QCOMPARE(stored.fileId, 777);
    QCOMPARE(stored.sizeBytes, qint64(2621440));
    QCOMPARE(stored.durationSeconds, 12);
}

void SharedMediaModelTest::itemsReturnsACopy()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100, QStringLiteral("original"))});

    QVector<SharedMediaItem> copy = model.items();
    copy[0].title = QStringLiteral("edited");

    QCOMPARE(model.items().constFirst().title, QStringLiteral("original"));
}

void SharedMediaModelTest::setItemsInAnotherOrderIsSilent()
{
    SharedMediaModel model;
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setItems({makeItem(1, 100), makeItem(2, 200)});
    QCOMPARE(spy.count(), 1);

    // The normalized list is the same, so only the input order differs.
    model.setItems({makeItem(2, 200), makeItem(1, 100)});
    QCOMPARE(spy.count(), 1);
}

void SharedMediaModelTest::setItemsWithChangedPayloadEmits()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100, QStringLiteral("before"))});
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setItems({makeItem(1, 100, QStringLiteral("after"))});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.items().constFirst().title, QStringLiteral("after"));
}

void SharedMediaModelTest::setItemsEmptyOnEmptyModelIsSilent()
{
    SharedMediaModel model;
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setItems({});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.nextFromMessageId(), qint64(0));
}

void SharedMediaModelTest::setItemsEmptyClearsLoadedItems()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100), makeItem(2, 200)});
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setItems({});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.items().isEmpty());
    QCOMPARE(model.nextFromMessageId(), qint64(0));
}

void SharedMediaModelTest::appendItemsAddsNewIdsOnlyAndResorts()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100, QStringLiteral("one")),
                    makeItem(2, 300, QStringLiteral("two"))});
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.appendItems({makeItem(3, 200, QStringLiteral("three")),
                       makeItem(2, 999, QStringLiteral("duplicate")),
                       makeItem(4, 400, QStringLiteral("four"))});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(messageIdsOf(model.items()), (QVector<qint64>{4, 2, 3, 1}));
    // The loaded copy of message 2 wins over the one from the page.
    QCOMPARE(model.items().at(1).date, qint64(300));
    QCOMPARE(model.items().at(1).title, QStringLiteral("two"));
    QCOMPARE(model.nextFromMessageId(), qint64(1));
}

void SharedMediaModelTest::appendItemsWithOnlyKnownIdsIsSilent()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100, QStringLiteral("one"))});
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.appendItems({makeItem(1, 500, QStringLiteral("duplicate")),
                       makeItem(1, 700, QStringLiteral("another"))});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.items().constFirst().date, qint64(100));
    QCOMPARE(model.items().constFirst().title, QStringLiteral("one"));
}

void SharedMediaModelTest::appendItemsToEmptyListWorks()
{
    SharedMediaModel model;
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.appendItems({makeItem(5, 50), makeItem(6, 100)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(messageIdsOf(model.items()), (QVector<qint64>{6, 5}));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.nextFromMessageId(), qint64(5));
}

void SharedMediaModelTest::appendItemsDedupesWithinThePage()
{
    SharedMediaModel model;
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.appendItems({makeItem(9, 100, QStringLiteral("first")),
                       makeItem(9, 200, QStringLiteral("duplicate")),
                       makeItem(8, 150, QStringLiteral("other"))});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(messageIdsOf(model.items()), (QVector<qint64>{8, 9}));
    QCOMPARE(model.items().constLast().date, qint64(100));
    QCOMPARE(model.items().constLast().title, QStringLiteral("first"));
}

void SharedMediaModelTest::appendMovesNextCursorOlder()
{
    SharedMediaModel model;
    model.setItems({makeItem(10, 100), makeItem(20, 200)});
    QCOMPARE(model.nextFromMessageId(), qint64(10));

    model.appendItems({makeItem(5, 50)});
    QCOMPARE(model.nextFromMessageId(), qint64(5));

    // A newer page does not move the cursor for older pages.
    model.appendItems({makeItem(30, 300)});
    QCOMPARE(model.nextFromMessageId(), qint64(5));
}

void SharedMediaModelTest::setTabClearsItemsAndEmits()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100), makeItem(2, 200)});
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setTab(SharedMediaTab::Videos);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(static_cast<int>(model.tab()), static_cast<int>(SharedMediaTab::Videos));
    QCOMPARE(model.count(), 0);
    QVERIFY(model.items().isEmpty());
    QCOMPARE(model.nextFromMessageId(), qint64(0));

    // Selecting the tab that is already current changes nothing.
    model.setTab(SharedMediaTab::Videos);
    QCOMPARE(spy.count(), 1);
}

void SharedMediaModelTest::setTabOnEmptyModelEmits()
{
    SharedMediaModel model;
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setTab(SharedMediaTab::Links);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(static_cast<int>(model.tab()), static_cast<int>(SharedMediaTab::Links));
}

void SharedMediaModelTest::setTabToCurrentTabKeepsItems()
{
    SharedMediaModel model;
    model.setItems({makeItem(1, 100), makeItem(2, 200)});
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setTab(SharedMediaTab::Photos);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 2);
    QCOMPARE(messageIdsOf(model.items()), (QVector<qint64>{2, 1}));
}

void SharedMediaModelTest::clearDropsItemsAndKeepsTab()
{
    SharedMediaModel model;
    model.setTab(SharedMediaTab::Voice);
    model.setItems({makeItem(1, 100), makeItem(2, 200)});
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.items().isEmpty());
    QCOMPARE(model.nextFromMessageId(), qint64(0));
    QCOMPARE(static_cast<int>(model.tab()), static_cast<int>(SharedMediaTab::Voice));

    model.clear();
    QCOMPARE(spy.count(), 1);
}

void SharedMediaModelTest::clearOnEmptyModelIsSilent()
{
    SharedMediaModel model;
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
}

void SharedMediaModelTest::changedFiresOnlyOnRealChanges()
{
    SharedMediaModel model;
    QSignalSpy spy(&model, &SharedMediaModel::changed);

    model.setItems({});
    model.clear();
    model.appendItems({});
    model.setTab(SharedMediaTab::Photos);
    QCOMPARE(spy.count(), 0);

    model.setItems({makeItem(1, 100)});
    QCOMPARE(spy.count(), 1);

    model.setItems({makeItem(1, 100)});
    model.appendItems({makeItem(1, 100)});
    QCOMPARE(spy.count(), 1);

    model.appendItems({makeItem(2, 200)});
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);
    model.clear();
    QCOMPARE(spy.count(), 3);
}

QTEST_GUILESS_MAIN(SharedMediaModelTest)
#include "tst_shared_media_model.moc"
