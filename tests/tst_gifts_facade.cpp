#include "ui/GiftsFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::ui::GiftsFacade;

namespace {

/// The star glyph and middle dot are built from code points so the test source
/// stays ASCII and matches GiftsFacade.cpp exactly.
QString star(qint64 value)
{
    return QString::number(value) + QLatin1Char(' ') + QChar(0x2B50);
}

QString separator()
{
    return QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ");
}

QString historyRow(const QString& direction, const QString& title,
                   qint64 stars, const QString& date)
{
    QStringList parts;
    parts.append(direction);
    if (!title.isEmpty())
        parts.append(title);
    parts.append(star(stars));
    if (!date.isEmpty())
        parts.append(date);
    return parts.join(separator());
}

void loadCatalog(GiftsFacade& facade)
{
    facade.setCatalog({QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("30")},
                      {QStringLiteral("Bear"), QStringLiteral("Panda"),
                       QStringLiteral("Rocket")},
                      {QStringLiteral("50"), QStringLiteral("100"),
                       QStringLiteral("500")});
}

void makeReady(GiftsFacade& facade)
{
    loadCatalog(facade);
    facade.select(0);
    facade.setRecipient(QStringLiteral("42"), QStringLiteral("7"));
}

} // namespace

class GiftsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    // Catalog
    void startsEmpty();
    void setCatalogFormatsTitlesAndPrices();
    void setCatalogKeepsModelOrder();
    void setCatalogDropsInvalidRows();
    void setCatalogReadsShortLists();
    void setCatalogReplacesPreviousCatalog();

    // Selection
    void selectStoresIndexAndGift();
    void selectOutOfRangeIsIgnored();
    void selectMinusOneClearsSelection();
    void selectKeepsSelectionAcrossIdenticalRefresh();
    void setCatalogClearsSelectionWhenGiftDisappears();
    void setCatalogReindexesSelectionByGiftId();
    void selectionEmitsChangedOnlyOnRealChange();

    // canSend
    void canSendRequiresSelectionAndRecipient();
    void canSendAcceptsUserIdWithoutChatId();
    void canSendRejectsGroupChatIdWithoutUserId();
    void canSendRequiresPositivePrice();
    void setRecipientParsesDecimalStrings();

    // send payload
    void sendEmitsPayloadWhenReady();
    void sendPrefersChatIdInPayload();
    void sendDoesNothingWhenNotReady();
    void sendKeepsFormAfterSending();

    // history
    void addHistoryFormatsRows();
    void historyIsNewestFirst();
    void historySortsUndatedRowsNewestFirst();
    void addHistoryOmitsEmptyTitleAndDate();
    void addHistoryNormalizesStars();
    void historyIsIndependentOfCatalog();

    // reset
    void resetClearsSendFormButKeepsCatalogAndHistory();
    void resetEmitsChangedOnlyWhenSomethingWasSet();

    // change notification
    void changedFiresForEachRealUpdate();
    void redundantUpdatesStaySilent();
};

void GiftsFacadeTest::startsEmpty()
{
    GiftsFacade facade;

    QVERIFY(facade.titles().isEmpty());
    QVERIFY(facade.prices().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
    QCOMPARE(facade.message(), QString());
    QVERIFY(!facade.anonymous());
    QVERIFY(!facade.canSend());
    QVERIFY(facade.history().isEmpty());
    QVERIFY(!facade.send());
}

void GiftsFacadeTest::setCatalogFormatsTitlesAndPrices()
{
    GiftsFacade facade;
    loadCatalog(facade);

    QCOMPARE(facade.titles(), QStringList({QStringLiteral("Bear"),
                                           QStringLiteral("Panda"),
                                           QStringLiteral("Rocket")}));
    QCOMPARE(facade.prices(), QStringList({star(50), star(100), star(500)}));
    QCOMPARE(facade.selectedIndex(), -1);
}

void GiftsFacadeTest::setCatalogKeepsModelOrder()
{
    GiftsFacade facade;
    facade.setCatalog({QStringLiteral("30"), QStringLiteral("10"), QStringLiteral("20")},
                      {QStringLiteral("Third"), QStringLiteral("First"),
                       QStringLiteral("Second")},
                      {QStringLiteral("300"), QStringLiteral("100"),
                       QStringLiteral("200")});

    QCOMPARE(facade.titles(), QStringList({QStringLiteral("Third"),
                                           QStringLiteral("First"),
                                           QStringLiteral("Second")}));
    QCOMPARE(facade.prices(), QStringList({star(300), star(100), star(200)}));
}

void GiftsFacadeTest::setCatalogDropsInvalidRows()
{
    GiftsFacade facade;
    facade.setCatalog({QStringLiteral("0"), QStringLiteral("5"), QStringLiteral("5"),
                       QStringLiteral("6")},
                      {QStringLiteral("Zero"), QStringLiteral("First"),
                       QStringLiteral("Duplicate"), QStringLiteral("Negative price")},
                      {QStringLiteral("10"), QStringLiteral("20"),
                       QStringLiteral("30"), QStringLiteral("-1")});

    // Non-positive ids and negative prices are dropped; a repeated id keeps its
    // first row. Nothing survives here except gift 5.
    QCOMPARE(facade.titles(), QStringList({QStringLiteral("First")}));
    QCOMPARE(facade.prices(), QStringList({star(20)}));
}

void GiftsFacadeTest::setCatalogReadsShortLists()
{
    GiftsFacade facade;
    facade.setCatalog({QStringLiteral("1"), QStringLiteral("2")},
                      {QStringLiteral("Only title")}, {QStringLiteral("10")});

    QCOMPARE(facade.titles(), QStringList({QStringLiteral("Only title"), QString()}));
    QCOMPARE(facade.prices(), QStringList({star(10), star(0)}));
}

void GiftsFacadeTest::setCatalogReplacesPreviousCatalog()
{
    GiftsFacade facade;
    loadCatalog(facade);
    facade.setCatalog({QStringLiteral("99")}, {QStringLiteral("Star")},
                      {QStringLiteral("900")});

    QCOMPARE(facade.titles(), QStringList({QStringLiteral("Star")}));
    QCOMPARE(facade.prices(), QStringList({star(900)}));
}

void GiftsFacadeTest::selectStoresIndexAndGift()
{
    GiftsFacade facade;
    loadCatalog(facade);

    facade.select(1);
    QCOMPARE(facade.selectedIndex(), 1);
    QVERIFY(!facade.canSend());

    facade.select(2);
    QCOMPARE(facade.selectedIndex(), 2);
}

void GiftsFacadeTest::selectOutOfRangeIsIgnored()
{
    GiftsFacade facade;
    loadCatalog(facade);
    facade.select(1);

    facade.select(99);
    QCOMPARE(facade.selectedIndex(), 1);

    facade.select(-2);
    QCOMPARE(facade.selectedIndex(), 1);
}

void GiftsFacadeTest::selectMinusOneClearsSelection()
{
    GiftsFacade facade;
    makeReady(facade);
    QVERIFY(facade.canSend());

    facade.select(-1);

    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.canSend());
}

void GiftsFacadeTest::selectKeepsSelectionAcrossIdenticalRefresh()
{
    GiftsFacade facade;
    makeReady(facade);
    QVERIFY(facade.canSend());

    loadCatalog(facade);

    QCOMPARE(facade.selectedIndex(), 0);
    QVERIFY(facade.canSend());
}

void GiftsFacadeTest::setCatalogClearsSelectionWhenGiftDisappears()
{
    GiftsFacade facade;
    makeReady(facade);
    QCOMPARE(facade.selectedIndex(), 0);

    facade.setCatalog({QStringLiteral("20"), QStringLiteral("30")},
                      {QStringLiteral("Panda"), QStringLiteral("Rocket")},
                      {QStringLiteral("100"), QStringLiteral("500")});

    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.canSend());
}

void GiftsFacadeTest::setCatalogReindexesSelectionByGiftId()
{
    GiftsFacade facade;
    makeReady(facade);
    facade.select(1); // gift 20
    QCOMPARE(facade.selectedIndex(), 1);

    facade.setCatalog({QStringLiteral("20"), QStringLiteral("10"), QStringLiteral("30")},
                      {QStringLiteral("Panda"), QStringLiteral("Bear"),
                       QStringLiteral("Rocket")},
                      {QStringLiteral("100"), QStringLiteral("50"),
                       QStringLiteral("500")});

    // The selection follows gift 20 to its new index and stays sendable.
    QCOMPARE(facade.selectedIndex(), 0);
    QVERIFY(facade.canSend());
}

void GiftsFacadeTest::selectionEmitsChangedOnlyOnRealChange()
{
    GiftsFacade facade;
    loadCatalog(facade);

    QSignalSpy spy(&facade, &GiftsFacade::changed);

    facade.select(1);   // 1
    facade.select(1);   // no-op
    facade.select(2);   // 2
    facade.select(99);  // ignored
    facade.select(-1);  // 3
    facade.select(-1);  // no-op

    QCOMPARE(spy.count(), 3);
}

void GiftsFacadeTest::canSendRequiresSelectionAndRecipient()
{
    GiftsFacade facade;
    loadCatalog(facade);
    facade.select(0);
    QVERIFY(!facade.canSend());

    facade.setRecipient(QStringLiteral("42"), QStringLiteral("0"));
    QVERIFY(facade.canSend());

    facade.select(-1);
    QVERIFY(!facade.canSend());

    facade.select(0);
    facade.setRecipient(QStringLiteral("0"), QStringLiteral("0"));
    QVERIFY(!facade.canSend());
}

void GiftsFacadeTest::canSendAcceptsUserIdWithoutChatId()
{
    GiftsFacade facade;
    loadCatalog(facade);
    facade.select(0);

    facade.setRecipient(QStringLiteral("0"), QStringLiteral("7"));
    QVERIFY(facade.canSend());
}

void GiftsFacadeTest::canSendRejectsGroupChatIdWithoutUserId()
{
    GiftsFacade facade;
    loadCatalog(facade);
    facade.select(0);

    facade.setRecipient(QStringLiteral("-1001234567890"), QStringLiteral("0"));
    QVERIFY(!facade.canSend());

    // A positive user id next to the unusable chat id still resolves.
    facade.setRecipient(QStringLiteral("-1001234567890"), QStringLiteral("7"));
    QVERIFY(facade.canSend());
}

void GiftsFacadeTest::canSendRequiresPositivePrice()
{
    GiftsFacade facade;
    facade.setCatalog({QStringLiteral("1")}, {QStringLiteral("Free")},
                      {QStringLiteral("0")});
    facade.select(0);
    facade.setRecipient(QStringLiteral("42"), QStringLiteral("0"));

    QCOMPARE(facade.prices(), QStringList({star(0)}));
    QVERIFY(!facade.canSend());
    QVERIFY(!facade.send());
}

void GiftsFacadeTest::setRecipientParsesDecimalStrings()
{
    GiftsFacade facade;
    loadCatalog(facade);
    facade.select(0);

    // An id larger than 2^53 survives because it travels as a string.
    facade.setRecipient(QStringLiteral("9876543210"), QStringLiteral("0"));
    QVERIFY(facade.canSend());

    QSignalSpy spy(&facade, &GiftsFacade::sendRequested);
    QVERIFY(facade.send());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("9876543210"));

    // A malformed id is no recipient at all.
    facade.setRecipient(QStringLiteral("not-a-number"), QStringLiteral("0"));
    QVERIFY(!facade.canSend());
}

void GiftsFacadeTest::sendEmitsPayloadWhenReady()
{
    GiftsFacade facade;
    makeReady(facade);
    facade.select(1);
    facade.setMessage(QStringLiteral("  for you  "));
    facade.setAnonymous(true);

    QSignalSpy spy(&facade, &GiftsFacade::sendRequested);
    QVERIFY(facade.send());

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> args = spy.at(0);
    QCOMPARE(args.at(0).toString(), QStringLiteral("20"));
    QCOMPARE(args.at(1).toString(), QStringLiteral("42"));
    QCOMPARE(args.at(2).toString(), QStringLiteral("7"));
    // The controller normalises the message before the facade reports it.
    QCOMPARE(args.at(3).toString(), QStringLiteral("for you"));
    QCOMPARE(args.at(4).toBool(), true);
}

void GiftsFacadeTest::sendPrefersChatIdInPayload()
{
    GiftsFacade facade;
    loadCatalog(facade);
    facade.select(0);
    facade.setRecipient(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &GiftsFacade::sendRequested);
    QVERIFY(facade.send());

    // Both ids are reported as stored; GiftSendController gives chatId
    // precedence when it builds owner_id.
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("42"));
    QCOMPARE(spy.at(0).at(2).toString(), QStringLiteral("7"));
    QCOMPARE(spy.at(0).at(4).toBool(), false);
}

void GiftsFacadeTest::sendDoesNothingWhenNotReady()
{
    GiftsFacade facade;
    QSignalSpy spy(&facade, &GiftsFacade::sendRequested);

    QVERIFY(!facade.send());
    QCOMPARE(spy.count(), 0);

    loadCatalog(facade);
    facade.select(0);
    QVERIFY(!facade.send());
    QCOMPARE(spy.count(), 0);
}

void GiftsFacadeTest::sendKeepsFormAfterSending()
{
    GiftsFacade facade;
    makeReady(facade);
    facade.setMessage(QStringLiteral("hello"));

    QSignalSpy spy(&facade, &GiftsFacade::sendRequested);
    QVERIFY(facade.send());
    QVERIFY(facade.send());

    QCOMPARE(spy.count(), 2);
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(facade.message(), QStringLiteral("hello"));
    QVERIFY(facade.canSend());
}

void GiftsFacadeTest::addHistoryFormatsRows()
{
    GiftsFacade facade;
    facade.addHistory(QStringLiteral("Bear"), QStringLiteral("2026-01-02"),
                      QStringLiteral("50"), true);
    facade.addHistory(QStringLiteral("Panda"), QStringLiteral("2026-01-01"),
                      QStringLiteral("100"), false);

    // The newer date is first.
    QCOMPARE(facade.history(),
             QStringList({historyRow(QStringLiteral("Received"), QStringLiteral("Bear"),
                                     50, QStringLiteral("2026-01-02")),
                          historyRow(QStringLiteral("Sent"), QStringLiteral("Panda"),
                                     100, QStringLiteral("2026-01-01"))}));
}

void GiftsFacadeTest::historyIsNewestFirst()
{
    GiftsFacade facade;
    facade.addHistory(QStringLiteral("Old"), QStringLiteral("2026-01-01"),
                      QStringLiteral("10"), true);
    facade.addHistory(QStringLiteral("New"), QStringLiteral("2026-03-01"),
                      QStringLiteral("20"), true);
    facade.addHistory(QStringLiteral("Mid"), QStringLiteral("2026-02-01"),
                      QStringLiteral("30"), false);

    QCOMPARE(facade.history(),
             QStringList({historyRow(QStringLiteral("Received"), QStringLiteral("New"),
                                     20, QStringLiteral("2026-03-01")),
                          historyRow(QStringLiteral("Sent"), QStringLiteral("Mid"),
                                     30, QStringLiteral("2026-02-01")),
                          historyRow(QStringLiteral("Received"), QStringLiteral("Old"),
                                     10, QStringLiteral("2026-01-01"))}));
}

void GiftsFacadeTest::historySortsUndatedRowsNewestFirst()
{
    GiftsFacade facade;
    facade.addHistory(QStringLiteral("First"), QString(), QStringLiteral("1"), true);
    facade.addHistory(QStringLiteral("Second"), QString(), QStringLiteral("2"), true);
    facade.addHistory(QStringLiteral("Third"), QStringLiteral("not a date"),
                      QStringLiteral("3"), true);

    // Undated rows tie on date, so the newest synthetic id comes first.
    QCOMPARE(facade.history(),
             QStringList({historyRow(QStringLiteral("Received"), QStringLiteral("Third"),
                                     3, QStringLiteral("not a date")),
                          historyRow(QStringLiteral("Received"), QStringLiteral("Second"),
                                     2, QString()),
                          historyRow(QStringLiteral("Received"), QStringLiteral("First"),
                                     1, QString())}));
}

void GiftsFacadeTest::addHistoryOmitsEmptyTitleAndDate()
{
    GiftsFacade facade;
    facade.addHistory(QString(), QString(), QStringLiteral("25"), false);

    QCOMPARE(facade.history(),
             QStringList({historyRow(QStringLiteral("Sent"), QString(), 25, QString())}));
}

void GiftsFacadeTest::addHistoryNormalizesStars()
{
    GiftsFacade facade;
    facade.addHistory(QStringLiteral("A"), QString(), QStringLiteral("-5"), false);
    facade.addHistory(QStringLiteral("B"), QString(),
                      QStringLiteral("50 ") + QChar(0x2B50), true);
    facade.addHistory(QStringLiteral("C"), QString(), QStringLiteral("1 000"), true);
    facade.addHistory(QStringLiteral("D"), QString(), QStringLiteral("abc"), true);

    // Negative and malformed amounts clamp to 0 in the history model; the star
    // glyph and grouping are tolerated in the input. All rows are undated, so
    // the newest id is first.
    QCOMPARE(facade.history(),
             QStringList({historyRow(QStringLiteral("Received"), QStringLiteral("D"),
                                     0, QString()),
                          historyRow(QStringLiteral("Received"), QStringLiteral("C"),
                                     1000, QString()),
                          historyRow(QStringLiteral("Received"), QStringLiteral("B"),
                                     50, QString()),
                          historyRow(QStringLiteral("Sent"), QStringLiteral("A"),
                                     0, QString())}));
}

void GiftsFacadeTest::historyIsIndependentOfCatalog()
{
    GiftsFacade facade;
    facade.addHistory(QStringLiteral("Bear"), QStringLiteral("2026-01-02"),
                      QStringLiteral("50"), true);

    loadCatalog(facade);
    facade.setCatalog({}, {}, {});

    QCOMPARE(facade.history().size(), 1);
    QVERIFY(facade.titles().isEmpty());
}

void GiftsFacadeTest::resetClearsSendFormButKeepsCatalogAndHistory()
{
    GiftsFacade facade;
    makeReady(facade);
    facade.setMessage(QStringLiteral("for you"));
    facade.setAnonymous(true);
    facade.addHistory(QStringLiteral("Bear"), QStringLiteral("2026-01-02"),
                      QStringLiteral("50"), true);

    const QStringList titles = facade.titles();
    const QStringList prices = facade.prices();
    const QStringList history = facade.history();

    QSignalSpy spy(&facade, &GiftsFacade::changed);
    facade.reset();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.selectedIndex(), -1);
    QCOMPARE(facade.message(), QString());
    QVERIFY(!facade.anonymous());
    QVERIFY(!facade.canSend());
    QCOMPARE(facade.titles(), titles);
    QCOMPARE(facade.prices(), prices);
    QCOMPARE(facade.history(), history);

    // The recipient is part of the form too, so it must be set again.
    facade.select(0);
    QVERIFY(!facade.canSend());
    facade.setRecipient(QStringLiteral("42"), QStringLiteral("0"));
    QVERIFY(facade.canSend());
}

void GiftsFacadeTest::resetEmitsChangedOnlyWhenSomethingWasSet()
{
    GiftsFacade facade;
    QSignalSpy spy(&facade, &GiftsFacade::changed);

    facade.reset();
    QCOMPARE(spy.count(), 0);

    facade.setMessage(QStringLiteral("hi"));
    QCOMPARE(spy.count(), 1);

    facade.reset();
    QCOMPARE(spy.count(), 2);

    facade.reset();
    QCOMPARE(spy.count(), 2);
}

void GiftsFacadeTest::changedFiresForEachRealUpdate()
{
    GiftsFacade facade;
    QSignalSpy spy(&facade, &GiftsFacade::changed);

    loadCatalog(facade);                                    // 1
    facade.select(0);                                       // 2
    facade.setMessage(QStringLiteral("hi"));                // 3
    facade.setAnonymous(true);                              // 4
    facade.setRecipient(QStringLiteral("42"), QStringLiteral("0")); // 5
    facade.addHistory(QStringLiteral("Bear"), QString(), QStringLiteral("50"), true); // 6
    facade.reset();                                         // 7

    QCOMPARE(spy.count(), 7);
}

void GiftsFacadeTest::redundantUpdatesStaySilent()
{
    GiftsFacade facade;
    loadCatalog(facade);

    QSignalSpy spy(&facade, &GiftsFacade::changed);

    loadCatalog(facade);                                    // no-op
    facade.select(-1);                                      // already none
    facade.setMessage(QString());                           // already empty
    facade.setAnonymous(false);                             // already false
    facade.setRecipient(QStringLiteral("0"), QStringLiteral("0")); // already none
    facade.reset();                                         // already empty
    QCOMPARE(spy.count(), 0);

    facade.select(0);                                       // 1
    facade.select(0);                                       // no-op
    facade.select(99);                                      // ignored
    QCOMPARE(spy.count(), 1);

    facade.select(-1);                                      // 2
    facade.select(-1);                                      // no-op
    QCOMPARE(spy.count(), 2);
}

QTEST_GUILESS_MAIN(GiftsFacadeTest)
#include "tst_gifts_facade.moc"
