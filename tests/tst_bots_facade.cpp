#include "ui/BotsFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

#include <initializer_list>

using beygram::ui::BotsFacade;

namespace {

/// A QStringList from ASCII literals, so a test reads as the list it checks.
QStringList strings(std::initializer_list<const char*> values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const char* value : values)
        result.append(QString::fromLatin1(value));
    return result;
}

/// A URL the Web App gate accepts, used wherever a test only cares about the
/// session and not about URL validation.
QString safeUrl()
{
    return QStringLiteral("https://mini.example/app");
}

} // namespace

class BotsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setCommandsPairsNamesAndKeepsOrder();
    void setCommandsNormalizesNames();
    void setCommandsDropsRepeatedNames();
    void setCommandsClearsMenu();

    void filterCommandsByPrefix();
    void filterCommandsAcceptsSlashAndTrims();
    void filterCommandsIgnoresCase();

    void openInlineRequiresPositiveId();
    void openInlineStartsSession();
    void openInlineSameBotIsQuiet();
    void openInlineOtherBotResetsSession();

    void setInlineQueryRequiresOpenSession();
    void setInlineQueryStoresTrimmedQuery();
    void setInlineQueryStartsNewAnswer();
    void setInlineQueryEmptyCancelsAnswer();

    void setInlineResultsRequiresQuery();
    void setInlineResultsPairsTitles();
    void setInlineResultsDropsEmptyAndDuplicateIds();

    void selectInlineResultEmitsId();
    void selectInlineResultOutOfRangeIsIgnored();
    void selectInlineResultRepeatsChoice();
    void closeInlineEndsSession();

    void openWebAppRequiresPositiveIds();
    void openWebAppRejectsUnsafeUrl();
    void openWebAppStartsSession();
    void openWebAppReopeningIsQuietButRequests();
    void closeWebAppEndsSession();

    void changedForwardedFromEverySurface();
};

void BotsFacadeTest::startsEmpty()
{
    BotsFacade facade;

    QVERIFY(facade.commands().isEmpty());
    QVERIFY(facade.filterCommands(QString()).isEmpty());
    QVERIFY(facade.filterCommands(QStringLiteral("st")).isEmpty());

    QVERIFY(!facade.inlineActive());
    QVERIFY(facade.inlineQuery().isEmpty());
    QVERIFY(facade.inlineTitles().isEmpty());

    QVERIFY(!facade.webAppActive());
    QVERIFY(facade.webAppTitle().isEmpty());
}

void BotsFacadeTest::setCommandsPairsNamesAndKeepsOrder()
{
    BotsFacade facade;
    QSignalSpy spy(&facade, &BotsFacade::changed);

    facade.setCommands(strings({"start", "help"}), strings({"Begin", "Get help"}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.commands(), strings({"start", "help"}));
    QCOMPARE(facade.filterCommands(QString()), strings({"start", "help"}));

    // Replacing the menu replaces it entirely; the old entry is gone.
    facade.setCommands(strings({"settings"}), strings({"Preferences"}));
    QCOMPARE(facade.commands(), strings({"settings"}));
}

void BotsFacadeTest::setCommandsNormalizesNames()
{
    BotsFacade facade;

    // The slash, the surrounding whitespace and the empty names all follow the
    // BotCommands rule: the menu stores bare, non-empty names.
    facade.setCommands(strings({"/start", "  help  ", "", "/", "   ", "settings"}),
                       strings({"Begin", "Get help", "ignored", "ignored", "ignored"}));

    QCOMPARE(facade.commands(), strings({"start", "help", "settings"}));
    QCOMPARE(facade.filterCommands(QString()), strings({"start", "help", "settings"}));

    // A name without a description is still a usable command.
    facade.setCommands(strings({"plain"}), QStringList());
    QCOMPARE(facade.commands(), strings({"plain"}));
}

void BotsFacadeTest::setCommandsDropsRepeatedNames()
{
    BotsFacade facade;

    facade.setCommands(strings({"start", "Start", "START", "help", "HELP"}),
                       strings({"First", "Second", "Third", "Help", "Help again"}));

    QCOMPARE(facade.commands(), strings({"start", "help"}));
}

void BotsFacadeTest::setCommandsClearsMenu()
{
    BotsFacade facade;
    facade.setCommands(strings({"start", "help"}), QStringList());

    QSignalSpy spy(&facade, &BotsFacade::changed);
    facade.setCommands(QStringList(), QStringList());

    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.commands().isEmpty());
    QVERIFY(facade.filterCommands(QString()).isEmpty());
}

void BotsFacadeTest::filterCommandsByPrefix()
{
    BotsFacade facade;
    facade.setCommands(strings({"start", "settings", "stop", "help"}), QStringList());

    QCOMPARE(facade.filterCommands(QStringLiteral("st")), strings({"start", "stop"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("set")), strings({"settings"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("start")), strings({"start"}));
    QVERIFY(facade.filterCommands(QStringLiteral("zzz")).isEmpty());

    // Filtering never reorders or mutates the stored menu.
    QCOMPARE(facade.commands(), strings({"start", "settings", "stop", "help"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("st")).size(), 2);
}

void BotsFacadeTest::filterCommandsAcceptsSlashAndTrims()
{
    BotsFacade facade;
    facade.setCommands(strings({"start", "settings", "stop"}), QStringList());

    // A composer may pass the whole text field value, slash included.
    QCOMPARE(facade.filterCommands(QStringLiteral("/st")), strings({"start", "stop"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("  /set  ")), strings({"settings"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("  st  ")), strings({"start", "stop"}));

    // A bare slash and whitespace are the empty prefix, so everything matches.
    QCOMPARE(facade.filterCommands(QStringLiteral("/")), facade.commands());
    QCOMPARE(facade.filterCommands(QStringLiteral("   ")), facade.commands());
}

void BotsFacadeTest::filterCommandsIgnoresCase()
{
    BotsFacade facade;
    facade.setCommands(strings({"Start", "HELP"}), QStringList());

    QCOMPARE(facade.filterCommands(QStringLiteral("st")), strings({"Start"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("ST")), strings({"Start"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("he")), strings({"HELP"}));
    QCOMPARE(facade.filterCommands(QStringLiteral("/HE")), strings({"HELP"}));
}

void BotsFacadeTest::openInlineRequiresPositiveId()
{
    BotsFacade facade;
    QSignalSpy spy(&facade, &BotsFacade::changed);

    facade.openInline(QString());
    facade.openInline(QStringLiteral("0"));
    facade.openInline(QStringLiteral("-5"));
    facade.openInline(QStringLiteral("abc"));
    facade.openInline(QStringLiteral("12x"));
    facade.openInline(QStringLiteral("1 2"));
    facade.openInline(QStringLiteral("   "));
    facade.openInline(QStringLiteral("999999999999999999999999"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.inlineActive());

    // A malformed id must not disturb the session already on screen.
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    spy.clear();

    facade.openInline(QStringLiteral("0"));
    facade.openInline(QStringLiteral("nope"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.inlineActive());
    QCOMPARE(facade.inlineQuery(), QStringLiteral("cats"));
}

void BotsFacadeTest::openInlineStartsSession()
{
    BotsFacade facade;
    QSignalSpy spy(&facade, &BotsFacade::changed);

    facade.openInline(QStringLiteral("  777  "));

    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.inlineActive());
    QVERIFY(facade.inlineQuery().isEmpty());
    QVERIFY(facade.inlineTitles().isEmpty());
}

void BotsFacadeTest::openInlineSameBotIsQuiet()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineResults(strings({"a"}), strings({"Cats"}));

    QSignalSpy spy(&facade, &BotsFacade::changed);
    facade.openInline(QStringLiteral("777"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.inlineActive());
    QCOMPARE(facade.inlineQuery(), QStringLiteral("cats"));
    QCOMPARE(facade.inlineTitles(), strings({"Cats"}));
}

void BotsFacadeTest::openInlineOtherBotResetsSession()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineResults(strings({"a", "b"}), strings({"A", "B"}));
    facade.selectInlineResult(1);

    QSignalSpy spy(&facade, &BotsFacade::changed);
    facade.openInline(QStringLiteral("888"));

    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.inlineActive());
    QVERIFY(facade.inlineQuery().isEmpty());
    QVERIFY(facade.inlineTitles().isEmpty());
}

void BotsFacadeTest::setInlineQueryRequiresOpenSession()
{
    BotsFacade facade;
    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::inlineQueryRequested);

    facade.setInlineQuery(QStringLiteral("cats"));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QVERIFY(facade.inlineQuery().isEmpty());

    // A query typed after the session closed has nowhere to go either.
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.closeInline();
    changedSpy.clear();
    requestSpy.clear();

    facade.setInlineQuery(QStringLiteral("dogs"));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QVERIFY(facade.inlineQuery().isEmpty());
}

void BotsFacadeTest::setInlineQueryStoresTrimmedQuery()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));

    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::inlineQueryRequested);

    facade.setInlineQuery(QStringLiteral("  cats  "));

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(facade.inlineQuery(), QStringLiteral("cats"));
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toString(), QStringLiteral("cats"));

    // The same trimmed text, however it is spaced, is not a new request.
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineQuery(QStringLiteral("   cats   "));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
}

void BotsFacadeTest::setInlineQueryStartsNewAnswer()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineResults(strings({"a", "b"}), strings({"A", "B"}));
    facade.selectInlineResult(1);

    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::inlineQueryRequested);

    facade.setInlineQuery(QStringLiteral("dogs"));

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toString(), QStringLiteral("dogs"));
    QCOMPARE(facade.inlineQuery(), QStringLiteral("dogs"));
    QVERIFY(facade.inlineTitles().isEmpty());

    // The new answer belongs to the new query and can be selected again.
    facade.setInlineResults(strings({"c"}), strings({"C"}));
    QCOMPARE(facade.inlineTitles(), strings({"C"}));

    QSignalSpy chosenSpy(&facade, &BotsFacade::inlineResultChosen);
    facade.selectInlineResult(0);
    QCOMPARE(chosenSpy.count(), 1);
    QCOMPARE(chosenSpy.at(0).at(0).toString(), QStringLiteral("c"));
}

void BotsFacadeTest::setInlineQueryEmptyCancelsAnswer()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineResults(strings({"a"}), strings({"A"}));

    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::inlineQueryRequested);

    facade.setInlineQuery(QString());

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toString(), QString());
    QVERIFY(facade.inlineQuery().isEmpty());
    QVERIFY(facade.inlineTitles().isEmpty());

    // Whitespace is the same empty query, so it stays silent.
    facade.setInlineQuery(QStringLiteral("   "));
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
}

void BotsFacadeTest::setInlineResultsRequiresQuery()
{
    BotsFacade facade;

    // No session at all: the answer is dropped, not queued.
    facade.setInlineResults(strings({"a"}), strings({"A"}));
    QVERIFY(facade.inlineTitles().isEmpty());

    // An open bot with nothing typed yet cannot own an answer either.
    facade.openInline(QStringLiteral("777"));
    QSignalSpy spy(&facade, &BotsFacade::changed);

    facade.setInlineResults(strings({"a"}), strings({"A"}));

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.inlineTitles().isEmpty());
}

void BotsFacadeTest::setInlineResultsPairsTitles()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));

    QSignalSpy spy(&facade, &BotsFacade::changed);
    facade.setInlineResults(strings({"a", "b", "c"}), strings({"A", "B"}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.inlineTitles(), strings({"A", "B", ""}));
}

void BotsFacadeTest::setInlineResultsDropsEmptyAndDuplicateIds()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));

    QSignalSpy spy(&facade, &BotsFacade::changed);
    facade.setInlineResults(strings({"a", "a", "", "b"}),
                            strings({"A first", "A second", "Empty id", "B"}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.inlineTitles(), strings({"A first", "B"}));

    // The identical answer is not a change.
    spy.clear();
    facade.setInlineResults(strings({"a", "b"}), strings({"A first", "B"}));
    QCOMPARE(spy.count(), 0);

    // Rewriting a title is a real change, because the view shows it.
    facade.setInlineResults(strings({"a", "b"}), strings({"A renamed", "B"}));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.inlineTitles(), strings({"A renamed", "B"}));
}

void BotsFacadeTest::selectInlineResultEmitsId()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineResults(strings({"a", "b"}), strings({"A", "B"}));

    QSignalSpy chosenSpy(&facade, &BotsFacade::inlineResultChosen);
    QSignalSpy changedSpy(&facade, &BotsFacade::changed);

    facade.selectInlineResult(1);

    QCOMPARE(chosenSpy.count(), 1);
    QCOMPARE(chosenSpy.at(0).at(0).toString(), QStringLiteral("b"));
    QCOMPARE(changedSpy.count(), 1);

    facade.selectInlineResult(0);

    QCOMPARE(chosenSpy.count(), 2);
    QCOMPARE(chosenSpy.at(1).at(0).toString(), QStringLiteral("a"));
    QCOMPARE(changedSpy.count(), 2);
}

void BotsFacadeTest::selectInlineResultOutOfRangeIsIgnored()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));

    // Nothing to choose from yet.
    QSignalSpy chosenSpy(&facade, &BotsFacade::inlineResultChosen);
    facade.selectInlineResult(0);
    QCOMPARE(chosenSpy.count(), 0);

    facade.setInlineResults(strings({"a", "b"}), strings({"A", "B"}));
    QSignalSpy changedSpy(&facade, &BotsFacade::changed);

    facade.selectInlineResult(-1);
    facade.selectInlineResult(2);
    facade.selectInlineResult(99);

    QCOMPARE(chosenSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);

    // A rejected tap must not break the next valid one.
    facade.selectInlineResult(1);
    QCOMPARE(chosenSpy.count(), 1);
    QCOMPARE(chosenSpy.at(0).at(0).toString(), QStringLiteral("b"));
}

void BotsFacadeTest::selectInlineResultRepeatsChoice()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineResults(strings({"a"}), strings({"A"}));

    QSignalSpy chosenSpy(&facade, &BotsFacade::inlineResultChosen);
    QSignalSpy changedSpy(&facade, &BotsFacade::changed);

    facade.selectInlineResult(0);
    facade.selectInlineResult(0);

    // The tap is a real user action both times, even though the stored
    // selection only changed once.
    QCOMPARE(chosenSpy.count(), 2);
    QCOMPARE(chosenSpy.at(0).at(0).toString(), QStringLiteral("a"));
    QCOMPARE(chosenSpy.at(1).at(0).toString(), QStringLiteral("a"));
    QCOMPARE(changedSpy.count(), 1);
}

void BotsFacadeTest::closeInlineEndsSession()
{
    BotsFacade facade;
    facade.openInline(QStringLiteral("777"));
    facade.setInlineQuery(QStringLiteral("cats"));
    facade.setInlineResults(strings({"a"}), strings({"A"}));

    QSignalSpy spy(&facade, &BotsFacade::changed);
    facade.closeInline();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.inlineActive());
    QVERIFY(facade.inlineQuery().isEmpty());
    QVERIFY(facade.inlineTitles().isEmpty());

    // Closing again changes nothing.
    facade.closeInline();
    QCOMPARE(spy.count(), 1);
}

void BotsFacadeTest::openWebAppRequiresPositiveIds()
{
    BotsFacade facade;
    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::webAppOpenRequested);

    facade.openWebApp(QStringLiteral("0"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("0"), QStringLiteral("App"),
                      safeUrl());
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("-777"), QStringLiteral("App"),
                      safeUrl());
    facade.openWebApp(QStringLiteral("abc"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());
    facade.openWebApp(QStringLiteral("4 2"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QVERIFY(!facade.webAppActive());

    // A malformed id must not tear down the app already open.
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());
    changedSpy.clear();
    requestSpy.clear();

    facade.openWebApp(QStringLiteral("0"), QStringLiteral("777"), QStringLiteral("Other"),
                      QStringLiteral("https://other.example/app"));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QVERIFY(facade.webAppActive());
    QCOMPARE(facade.webAppTitle(), QStringLiteral("App"));
}

void BotsFacadeTest::openWebAppRejectsUnsafeUrl()
{
    BotsFacade facade;
    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::webAppOpenRequested);

    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      QString());
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      QStringLiteral("http://mini.example/app"));
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      QStringLiteral("javascript:alert(1)"));
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      QStringLiteral("tg://resolve?domain=bot"));
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      QStringLiteral("https://user:pass@mini.example/app"));
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      QStringLiteral("https://"));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QVERIFY(!facade.webAppActive());
    QVERIFY(facade.webAppTitle().isEmpty());

    // The same call with an accepted https URL goes through, trimmed.
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      QStringLiteral("  https://mini.example/app  "));

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toString(), QStringLiteral("https://mini.example/app"));
    QVERIFY(facade.webAppActive());
}

void BotsFacadeTest::openWebAppStartsSession()
{
    BotsFacade facade;
    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::webAppOpenRequested);

    facade.openWebApp(QStringLiteral(" 42 "), QStringLiteral(" 777 "),
                      QStringLiteral("  Mini App  "), safeUrl());

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toString(), safeUrl());
    QVERIFY(facade.webAppActive());
    QCOMPARE(facade.webAppTitle(), QStringLiteral("Mini App"));
}

void BotsFacadeTest::openWebAppReopeningIsQuietButRequests()
{
    BotsFacade facade;
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());

    QSignalSpy changedSpy(&facade, &BotsFacade::changed);
    QSignalSpy requestSpy(&facade, &BotsFacade::webAppOpenRequested);

    // The identical session stores nothing new, but the caller asked for the
    // app to be launched again.
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 1);
    QVERIFY(facade.webAppActive());

    // A different app replaces the session and is reported as a change.
    facade.openWebApp(QStringLiteral("43"), QStringLiteral("888"), QStringLiteral("Other"),
                      QStringLiteral("https://other.example/app"));

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 2);
    QVERIFY(facade.webAppActive());
    QCOMPARE(facade.webAppTitle(), QStringLiteral("Other"));
}

void BotsFacadeTest::closeWebAppEndsSession()
{
    BotsFacade facade;
    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());

    QSignalSpy spy(&facade, &BotsFacade::changed);
    facade.closeWebApp();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.webAppActive());
    QVERIFY(facade.webAppTitle().isEmpty());

    // Closing again changes nothing.
    facade.closeWebApp();
    QCOMPARE(spy.count(), 1);
}

void BotsFacadeTest::changedForwardedFromEverySurface()
{
    BotsFacade facade;
    QSignalSpy spy(&facade, &BotsFacade::changed);

    facade.setCommands(strings({"start"}), QStringList());
    QCOMPARE(spy.count(), 1);

    facade.openInline(QStringLiteral("777"));
    QCOMPARE(spy.count(), 2);

    facade.openWebApp(QStringLiteral("42"), QStringLiteral("777"), QStringLiteral("App"),
                      safeUrl());
    QCOMPARE(spy.count(), 3);
}

QTEST_GUILESS_MAIN(BotsFacadeTest)
#include "tst_bots_facade.moc"
