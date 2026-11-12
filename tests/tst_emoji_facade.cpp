#include "ui/EmojiFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

// Emoji are built from UTF-8 bytes so the tests do not depend on the source
// encoding the compiler assumes.
QString grinning()
{
    return QString::fromUtf8("\xF0\x9F\x98\x80"); // U+1F600
}

QString joy()
{
    return QString::fromUtf8("\xF0\x9F\x98\x82"); // U+1F602
}

QString thumbsUp()
{
    return QString::fromUtf8("\xF0\x9F\x91\x8D"); // U+1F44D
}

QString heart()
{
    return QString::fromUtf8("\xE2\x9D\xA4"); // U+2764
}

/// Four emoji in three sections: Smileys (two), People (one), Symbols (one).
void loadSample(EmojiFacade& facade)
{
    facade.setCatalog(
        {grinning(), joy(), thumbsUp(), heart()},
        {QStringLiteral("Grinning face"), QStringLiteral("Face with tears of joy"),
         QStringLiteral("Thumbs up"), QStringLiteral("Red heart")},
        {QStringLiteral("Smileys"), QStringLiteral("Smileys"),
         QStringLiteral("People"), QStringLiteral("Symbols")});
}

} // namespace

class EmojiFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setCatalogListsEmojisInOrder();
    void setCatalogBuildsCategoriesInFirstSeenOrder();
    void setCatalogDropsEmptyAndDuplicateEmojis();
    void setCatalogIgnoresNamesPastTheEmojiList();
    void setCatalogReadsMissingNamesAndCategoriesAsEmpty();

    void categoryIndexStartsAtAll();
    void selectCategoryFiltersEmojis();
    void selectCategoryOutOfRangeSelectsAll();
    void selectCategoryEmitsChangedOnlyOnRealChange();
    void selectionSurvivesCatalogReplacement();
    void selectionFallsBackToAllWhenCategoryDisappears();
    void setCatalogInvalidatingSelectionEmitsOnce();
    void setCatalogOfSameContentIsSilent();

    void searchMatchesEmojiOrName();
    void searchIsCaseInsensitive();
    void searchIgnoresSurroundingWhitespace();
    void searchCombinesWithSelectedCategory();
    void searchEmitsChangedOnlyOnRealChange();
    void clearingQueryRestoresTheSection();

    void emojisAreACopy();

    void activateEmitsEmojiActivated();
    void activateIgnoresEmptyEmoji();
    void activateDoesNotTouchTheList();

    void replacementsAreEmptyByDefault();
    void applyReplacementsRewritesEveryOccurrence();
    void applyReplacementsIsCaseSensitive();
    void applyReplacementsPrefersTheLongestNeedle();
    void applyReplacementsDoesNotCascade();
    void addReplacementKeepsFirstRuleForNeedle();
    void addReplacementIgnoresEmptyNeedle();
    void addReplacementAllowsEmptyReplacement();
    void addReplacementDoesNotEmitChanged();
};

void EmojiFacadeTest::startsEmpty()
{
    EmojiFacade facade;

    QVERIFY(facade.categories().isEmpty());
    QVERIFY(facade.emojis().isEmpty());
    QCOMPARE(facade.query(), QString());
    QCOMPARE(facade.categoryIndex(), -1);
    QCOMPARE(facade.applyReplacements(QStringLiteral("hello")),
             QStringLiteral("hello"));
}

void EmojiFacadeTest::setCatalogListsEmojisInOrder()
{
    EmojiFacade facade;
    loadSample(facade);

    // The facade hands the emoji out in the order the catalog supplied them.
    QCOMPARE(facade.emojis(),
             QStringList({grinning(), joy(), thumbsUp(), heart()}));
}

void EmojiFacadeTest::setCatalogBuildsCategoriesInFirstSeenOrder()
{
    EmojiFacade facade;
    loadSample(facade);

    QCOMPARE(facade.categories(),
             QStringList({QStringLiteral("Smileys"), QStringLiteral("People"),
                          QStringLiteral("Symbols")}));
}

void EmojiFacadeTest::setCatalogDropsEmptyAndDuplicateEmojis()
{
    EmojiFacade facade;
    facade.setCatalog(
        {QString(), grinning(), grinning(), joy()},
        {QStringLiteral("Blank"), QStringLiteral("First"),
         QStringLiteral("Second"), QStringLiteral("Joy")},
        {QStringLiteral("Misc"), QStringLiteral("Smileys"),
         QStringLiteral("People"), QStringLiteral("Smileys")});

    // An entry without an emoji cannot be inserted, and the first row of a
    // repeated emoji wins, so the duplicate's name is not searchable either.
    QCOMPARE(facade.emojis(), QStringList({grinning(), joy()}));
    QCOMPARE(facade.categories(), QStringList({QStringLiteral("Smileys")}));

    facade.setQuery(QStringLiteral("Second"));
    QVERIFY(facade.emojis().isEmpty());
}

void EmojiFacadeTest::setCatalogIgnoresNamesPastTheEmojiList()
{
    EmojiFacade facade;
    facade.setCatalog({grinning()},
                      {QStringLiteral("Grinning face"), QStringLiteral("Ghost")},
                      {QStringLiteral("Smileys"),
                       QStringLiteral("Ghost section")});

    // The emoji list defines the rows: a name or category with no emoji at its
    // offset belongs to nothing.
    QCOMPARE(facade.categories(), QStringList({QStringLiteral("Smileys")}));
    facade.setQuery(QStringLiteral("Ghost"));
    QVERIFY(facade.emojis().isEmpty());
}

void EmojiFacadeTest::setCatalogReadsMissingNamesAndCategoriesAsEmpty()
{
    EmojiFacade facade;
    facade.setCatalog({grinning(), joy()}, {QStringLiteral("Grinning face")},
                      {QStringLiteral("Smileys")});

    QCOMPARE(facade.categories(), QStringList({QStringLiteral("Smileys")}));

    // joy() has no name, so it cannot be found by one.
    facade.setQuery(QStringLiteral("tears"));
    QVERIFY(facade.emojis().isEmpty());

    // It has no section either, so it is not part of "Smileys".
    facade.setQuery(QString());
    facade.selectCategory(0);
    QCOMPARE(facade.emojis(), QStringList({grinning()}));
}

void EmojiFacadeTest::categoryIndexStartsAtAll()
{
    EmojiFacade facade;
    loadSample(facade);

    QCOMPARE(facade.categoryIndex(), -1);
    QCOMPARE(facade.categories().size(), 3);
    QCOMPARE(facade.emojis().size(), 4);
}

void EmojiFacadeTest::selectCategoryFiltersEmojis()
{
    EmojiFacade facade;
    loadSample(facade);

    facade.selectCategory(0);
    QCOMPARE(facade.categoryIndex(), 0);
    QCOMPARE(facade.emojis(), QStringList({grinning(), joy()}));

    facade.selectCategory(1);
    QCOMPARE(facade.categoryIndex(), 1);
    QCOMPARE(facade.emojis(), QStringList({thumbsUp()}));

    facade.selectCategory(2);
    QCOMPARE(facade.categoryIndex(), 2);
    QCOMPARE(facade.emojis(), QStringList({heart()}));
}

void EmojiFacadeTest::selectCategoryOutOfRangeSelectsAll()
{
    EmojiFacade facade;
    loadSample(facade);

    facade.selectCategory(1);
    QCOMPARE(facade.categoryIndex(), 1);

    // A stale or impossible index must never leave the picker on nothing.
    facade.selectCategory(99);
    QCOMPARE(facade.categoryIndex(), -1);
    QCOMPARE(facade.emojis().size(), 4);

    facade.selectCategory(0);
    facade.selectCategory(-5);
    QCOMPARE(facade.categoryIndex(), -1);
    QCOMPARE(facade.emojis().size(), 4);
}

void EmojiFacadeTest::selectCategoryEmitsChangedOnlyOnRealChange()
{
    EmojiFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &EmojiFacade::changed);

    facade.selectCategory(0);
    QCOMPARE(spy.count(), 1);

    // Same section again is not a change.
    facade.selectCategory(0);
    QCOMPARE(spy.count(), 1);

    facade.selectCategory(1);
    QCOMPARE(spy.count(), 2);

    facade.selectCategory(-1);
    QCOMPARE(spy.count(), 3);

    // "All" twice, and an impossible index that is also "all", stay silent.
    facade.selectCategory(-1);
    facade.selectCategory(42);
    QCOMPARE(spy.count(), 3);
}

void EmojiFacadeTest::selectionSurvivesCatalogReplacement()
{
    EmojiFacade facade;
    loadSample(facade);
    facade.selectCategory(0); // Smileys

    // The sections are reordered and re-spelled; the selection is the section,
    // not the offset it used to sit at.
    facade.setCatalog({thumbsUp(), grinning(), joy()},
                      {QStringLiteral("Thumbs up"), QStringLiteral("Grinning face"),
                       QStringLiteral("Face with tears of joy")},
                      {QStringLiteral("People"), QStringLiteral("smileys"),
                       QStringLiteral("smileys")});

    QCOMPARE(facade.categories(),
             QStringList({QStringLiteral("People"), QStringLiteral("smileys")}));
    QCOMPARE(facade.categoryIndex(), 1);
    QCOMPARE(facade.emojis(), QStringList({grinning(), joy()}));
}

void EmojiFacadeTest::selectionFallsBackToAllWhenCategoryDisappears()
{
    EmojiFacade facade;
    loadSample(facade);
    facade.selectCategory(2); // Symbols

    facade.setCatalog({grinning()}, {QStringLiteral("Grinning face")},
                      {QStringLiteral("Smileys")});

    QCOMPARE(facade.categoryIndex(), -1);
    QCOMPARE(facade.emojis(), QStringList({grinning()}));
}

void EmojiFacadeTest::setCatalogInvalidatingSelectionEmitsOnce()
{
    EmojiFacade facade;
    loadSample(facade);
    facade.selectCategory(2); // Symbols

    QSignalSpy spy(&facade, &EmojiFacade::changed);
    facade.setCatalog({grinning()}, {QStringLiteral("Grinning face")},
                      {QStringLiteral("Smileys")});

    // One call, one notification, even though the catalog and the selection
    // both moved.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.categoryIndex(), -1);
}

void EmojiFacadeTest::setCatalogOfSameContentIsSilent()
{
    EmojiFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &EmojiFacade::changed);
    loadSample(facade);

    QCOMPARE(spy.count(), 0);
}

void EmojiFacadeTest::searchMatchesEmojiOrName()
{
    EmojiFacade facade;
    loadSample(facade);

    facade.setQuery(QStringLiteral("thumb"));
    QCOMPARE(facade.emojis(), QStringList({thumbsUp()}));

    facade.setQuery(QStringLiteral("face"));
    QCOMPARE(facade.emojis(), QStringList({grinning(), joy()}));

    // The emoji itself is searchable too.
    facade.setQuery(grinning());
    QCOMPARE(facade.emojis(), QStringList({grinning()}));

    // Section titles are not part of the search.
    facade.setQuery(QStringLiteral("Smileys"));
    QVERIFY(facade.emojis().isEmpty());

    facade.setQuery(QStringLiteral("missing"));
    QVERIFY(facade.emojis().isEmpty());
}

void EmojiFacadeTest::searchIsCaseInsensitive()
{
    EmojiFacade facade;
    loadSample(facade);

    facade.setQuery(QStringLiteral("GRINNING"));
    QCOMPARE(facade.emojis(), QStringList({grinning()}));

    facade.setQuery(QStringLiteral("tEaRs"));
    QCOMPARE(facade.emojis(), QStringList({joy()}));
}

void EmojiFacadeTest::searchIgnoresSurroundingWhitespace()
{
    EmojiFacade facade;
    loadSample(facade);

    facade.setQuery(QStringLiteral("  thumb  "));
    QCOMPARE(facade.query(), QStringLiteral("thumb"));
    QCOMPARE(facade.emojis(), QStringList({thumbsUp()}));

    // Whitespace alone is not a search.
    facade.setQuery(QStringLiteral("   "));
    QCOMPARE(facade.query(), QString());
    QCOMPARE(facade.emojis().size(), 4);
}

void EmojiFacadeTest::searchCombinesWithSelectedCategory()
{
    EmojiFacade facade;
    loadSample(facade);
    facade.selectCategory(0); // Smileys: grinning(), joy()

    facade.setQuery(QStringLiteral("tears"));
    QCOMPARE(facade.emojis(), QStringList({joy()}));

    // A match outside the selected section stays hidden: the query narrows the
    // section instead of escaping it.
    facade.setQuery(QStringLiteral("thumb"));
    QVERIFY(facade.emojis().isEmpty());
}

void EmojiFacadeTest::searchEmitsChangedOnlyOnRealChange()
{
    EmojiFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &EmojiFacade::changed);

    facade.setQuery(QStringLiteral("grin"));
    QCOMPARE(spy.count(), 1);

    // The same query, and one that trims to it, are not changes.
    facade.setQuery(QStringLiteral("grin"));
    QCOMPARE(spy.count(), 1);
    facade.setQuery(QStringLiteral(" grin "));
    QCOMPARE(spy.count(), 1);

    facade.setQuery(QStringLiteral("joy"));
    QCOMPARE(spy.count(), 2);

    facade.setQuery(QString());
    QCOMPARE(spy.count(), 3);

    facade.setQuery(QString());
    QCOMPARE(spy.count(), 3);
}

void EmojiFacadeTest::clearingQueryRestoresTheSection()
{
    EmojiFacade facade;
    loadSample(facade);
    facade.selectCategory(0); // Smileys

    facade.setQuery(QStringLiteral("tears"));
    QCOMPARE(facade.emojis(), QStringList({joy()}));

    facade.setQuery(QString());
    QCOMPARE(facade.emojis(), QStringList({grinning(), joy()}));
}

void EmojiFacadeTest::emojisAreACopy()
{
    EmojiFacade facade;
    loadSample(facade);

    QStringList list = facade.emojis();
    list.clear();
    QCOMPARE(facade.emojis().size(), 4);

    QStringList categories = facade.categories();
    categories.append(QStringLiteral("Extra"));
    QCOMPARE(facade.categories().size(), 3);
}

void EmojiFacadeTest::activateEmitsEmojiActivated()
{
    EmojiFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &EmojiFacade::emojiActivated);
    facade.activate(grinning());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), grinning());
}

void EmojiFacadeTest::activateIgnoresEmptyEmoji()
{
    EmojiFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &EmojiFacade::emojiActivated);
    facade.activate(QString());

    QCOMPARE(spy.count(), 0);
}

void EmojiFacadeTest::activateDoesNotTouchTheList()
{
    EmojiFacade facade;
    loadSample(facade);

    // The recent list is not exposed, so recording a use cannot repaint the
    // grid.
    QSignalSpy spy(&facade, &EmojiFacade::changed);
    facade.activate(grinning());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.emojis().size(), 4);
}

void EmojiFacadeTest::replacementsAreEmptyByDefault()
{
    EmojiFacade facade;

    QCOMPARE(facade.applyReplacements(QString()), QString());
    QCOMPARE(facade.applyReplacements(QStringLiteral("hello")),
             QStringLiteral("hello"));
}

void EmojiFacadeTest::applyReplacementsRewritesEveryOccurrence()
{
    EmojiFacade facade;
    facade.addReplacement(QStringLiteral("brb"), QStringLiteral("be right back"));

    QCOMPARE(facade.applyReplacements(QStringLiteral("brb and brb")),
             QStringLiteral("be right back and be right back"));
}

void EmojiFacadeTest::applyReplacementsIsCaseSensitive()
{
    EmojiFacade facade;
    facade.addReplacement(QStringLiteral("brb"), QStringLiteral("be right back"));

    QCOMPARE(facade.applyReplacements(QStringLiteral("BRB")), QStringLiteral("BRB"));
}

void EmojiFacadeTest::applyReplacementsPrefersTheLongestNeedle()
{
    EmojiFacade facade;
    // The short rule is added first on purpose: order must not matter.
    facade.addReplacement(QStringLiteral("b"), QStringLiteral("B"));
    facade.addReplacement(QStringLiteral("brb"), QStringLiteral("be right back"));

    QCOMPARE(facade.applyReplacements(QStringLiteral("brb")),
             QStringLiteral("be right back"));
    QCOMPARE(facade.applyReplacements(QStringLiteral("b")), QStringLiteral("B"));
}

void EmojiFacadeTest::applyReplacementsDoesNotCascade()
{
    EmojiFacade facade;
    facade.addReplacement(QStringLiteral("a"), QStringLiteral("b"));
    facade.addReplacement(QStringLiteral("b"), QStringLiteral("c"));

    // The substituted "b" is never scanned again, but a real one still is.
    QCOMPARE(facade.applyReplacements(QStringLiteral("a")), QStringLiteral("b"));
    QCOMPARE(facade.applyReplacements(QStringLiteral("ab")), QStringLiteral("bc"));
}

void EmojiFacadeTest::addReplacementKeepsFirstRuleForNeedle()
{
    EmojiFacade facade;
    facade.addReplacement(QStringLiteral("a"), QStringLiteral("1"));
    facade.addReplacement(QStringLiteral("a"), QStringLiteral("2"));

    QCOMPARE(facade.applyReplacements(QStringLiteral("a")), QStringLiteral("1"));
}

void EmojiFacadeTest::addReplacementIgnoresEmptyNeedle()
{
    EmojiFacade facade;
    facade.addReplacement(QString(), QStringLiteral("x"));

    QCOMPARE(facade.applyReplacements(QStringLiteral("a")), QStringLiteral("a"));

    // The ignored rule does not break the rules added after it.
    facade.addReplacement(QStringLiteral("a"), QStringLiteral("A"));
    QCOMPARE(facade.applyReplacements(QStringLiteral("a")), QStringLiteral("A"));
}

void EmojiFacadeTest::addReplacementAllowsEmptyReplacement()
{
    EmojiFacade facade;
    facade.addReplacement(QStringLiteral("secret"), QString());

    QCOMPARE(facade.applyReplacements(QStringLiteral("a secret b")),
             QStringLiteral("a  b"));
}

void EmojiFacadeTest::addReplacementDoesNotEmitChanged()
{
    EmojiFacade facade;
    loadSample(facade);

    // No exposed property depends on the rules, so changing them is not a
    // repaint; applyReplacements() reflects the new rule immediately.
    QSignalSpy spy(&facade, &EmojiFacade::changed);
    facade.addReplacement(QStringLiteral("brb"), QStringLiteral("be right back"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.applyReplacements(QStringLiteral("brb")),
             QStringLiteral("be right back"));
}

QTEST_GUILESS_MAIN(EmojiFacadeTest)
#include "tst_emoji_facade.moc"
