#include "telegram/EmojiCatalogModel.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

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

EmojiEntry entry(const QString& emoji, const QString& name,
                 const QString& category = {})
{
    return EmojiEntry{emoji, name, category};
}

QStringList emojisOf(const QVector<EmojiEntry>& entries)
{
    QStringList emojis;
    emojis.reserve(entries.size());
    for (const EmojiEntry& item : entries)
        emojis.append(item.emoji);
    return emojis;
}

QStringList namesOf(const QVector<EmojiEntry>& entries)
{
    QStringList names;
    names.reserve(entries.size());
    for (const EmojiEntry& item : entries)
        names.append(item.name);
    return names;
}

} // namespace

class EmojiCatalogModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setEntriesDropsEmptyEmoji();
    void setEntriesDedupesByEmojiKeepingFirst();
    void setEntriesReplacesPreviousCatalog();

    void categoriesAreUniqueInFirstSeenOrder();
    void categoriesIgnoreUncategorized();
    void categoriesTreatCaseVariantsAsOne();

    void byCategoryFiltersEntries();
    void byCategoryIgnoresCase();
    void byCategoryKeepsStoredOrder();
    void byCategoryOfEmptyNameMatchesNothing();
    void byCategoryOfUnknownCategoryIsEmpty();

    void searchMatchesEmojiOrName();
    void searchIgnoresCase();
    void searchKeepsStoredOrder();
    void searchRespectsLimit();
    void searchWithZeroLimitReturnsNothing();
    void searchWithNegativeLimitIsUnlimited();
    void searchWithEmptyQueryReturnsEverything();

    void recentStartsEmpty();
    void addRecentKeepsNewestFirst();
    void addRecentMovesExistingEmojiToFront();
    void addRecentCapsList();
    void addRecentIgnoresEmptyEmoji();
    void addRecentIgnoresNonPositiveCap();
    void recentSurvivesCatalogReplacement();

    void clearDropsCatalogAndRecent();

    void emitsChangedOnlyOnRealEntryChanges();
    void emitsChangedForAddRecentChanges();
    void emitsChangedOncePerClear();

    void returnsIndependentCopies();
};

void EmojiCatalogModelTest::startsEmpty()
{
    EmojiCatalogModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.categories().isEmpty());
    QVERIFY(model.byCategory(QStringLiteral("Smileys")).isEmpty());
    QVERIFY(model.search(QString()).isEmpty());
    QVERIFY(model.search(QStringLiteral("smile")).isEmpty());
    QVERIFY(model.recent().isEmpty());
}

void EmojiCatalogModelTest::setEntriesDropsEmptyEmoji()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(QString(), QStringLiteral("Blank"), QStringLiteral("Misc")),
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(QString(), QStringLiteral("Also blank")),
    });

    // An entry without an emoji cannot be inserted, so it never reaches the
    // catalog and its name is not searchable either.
    QCOMPARE(model.count(), 1);
    QCOMPARE(emojisOf(model.search(QString())), QStringList({grinning()}));
    QVERIFY(model.search(QStringLiteral("blank")).isEmpty());
    QCOMPARE(model.categories(), QStringList({QStringLiteral("Smileys")}));
}

void EmojiCatalogModelTest::setEntriesDedupesByEmojiKeepingFirst()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("First"), QStringLiteral("Smileys")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
        entry(grinning(), QStringLiteral("Second"), QStringLiteral("People")),
        entry(joy(), QStringLiteral("Third")),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(emojisOf(model.search(QString())),
             QStringList({grinning(), joy()}));
    QCOMPARE(namesOf(model.search(QString())),
             QStringList({QStringLiteral("First"), QStringLiteral("Joy")}));

    // The first entry also keeps its category, so the duplicate does not
    // create a second section.
    QCOMPARE(model.categories(), QStringList({QStringLiteral("Smileys")}));
    QCOMPARE(namesOf(model.byCategory(QStringLiteral("People"))),
             QStringList());
}

void EmojiCatalogModelTest::setEntriesReplacesPreviousCatalog()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
    });
    model.setEntries({entry(thumbsUp(), QStringLiteral("Thumbs up"),
                            QStringLiteral("People"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(emojisOf(model.search(QString())), QStringList({thumbsUp()}));
    QCOMPARE(model.categories(), QStringList({QStringLiteral("People")}));
    QVERIFY(model.byCategory(QStringLiteral("Smileys")).isEmpty());
}

void EmojiCatalogModelTest::categoriesAreUniqueInFirstSeenOrder()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(thumbsUp(), QStringLiteral("Thumbs up"),
              QStringLiteral("People")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
        entry(heart(), QStringLiteral("Heart"), QStringLiteral("Symbols")),
        entry(heart(), QStringLiteral("Duplicate heart"),
              QStringLiteral("People")),
    });

    QCOMPARE(model.categories(),
             QStringList({QStringLiteral("Smileys"), QStringLiteral("People"),
                          QStringLiteral("Symbols")}));
}

void EmojiCatalogModelTest::categoriesIgnoreUncategorized()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
    });

    QCOMPARE(model.categories(), QStringList({QStringLiteral("Smileys")}));
    QCOMPARE(model.count(), 2);
}

void EmojiCatalogModelTest::categoriesTreatCaseVariantsAsOne()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("smileys")),
    });

    // The first spelling names the section, and both entries belong to it
    // because byCategory() ignores case.
    QCOMPARE(model.categories(), QStringList({QStringLiteral("Smileys")}));
    QCOMPARE(emojisOf(model.byCategory(QStringLiteral("SMILEYS"))),
             QStringList({grinning(), joy()}));
}

void EmojiCatalogModelTest::byCategoryFiltersEntries()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(thumbsUp(), QStringLiteral("Thumbs up"),
              QStringLiteral("People")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
    });

    QCOMPARE(emojisOf(model.byCategory(QStringLiteral("Smileys"))),
             QStringList({grinning(), joy()}));
    QCOMPARE(emojisOf(model.byCategory(QStringLiteral("People"))),
             QStringList({thumbsUp()}));
    QVERIFY(model.byCategory(QStringLiteral("Symbols")).isEmpty());
}

void EmojiCatalogModelTest::byCategoryIgnoresCase()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
    });

    QCOMPARE(emojisOf(model.byCategory(QStringLiteral("smileys"))),
             QStringList({grinning(), joy()}));
    QCOMPARE(emojisOf(model.byCategory(QStringLiteral("sMiLeYs"))),
             QStringList({grinning(), joy()}));
}

void EmojiCatalogModelTest::byCategoryKeepsStoredOrder()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(thumbsUp(), QStringLiteral("Thumbs up"),
              QStringLiteral("People")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
        entry(heart(), QStringLiteral("Heart"), QStringLiteral("Smileys")),
    });

    QCOMPARE(namesOf(model.byCategory(QStringLiteral("Smileys"))),
             QStringList({QStringLiteral("Grinning face"),
                          QStringLiteral("Joy"), QStringLiteral("Heart")}));
}

void EmojiCatalogModelTest::byCategoryOfEmptyNameMatchesNothing()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
    });

    // An empty category is "uncategorized", not a section that can be listed.
    QVERIFY(model.byCategory(QString()).isEmpty());
}

void EmojiCatalogModelTest::byCategoryOfUnknownCategoryIsEmpty()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
    });

    QVERIFY(model.byCategory(QStringLiteral("Smileys ")).isEmpty());
    QVERIFY(model.byCategory(QStringLiteral("Smile")).isEmpty());
}

void EmojiCatalogModelTest::searchMatchesEmojiOrName()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(thumbsUp(), QStringLiteral("Thumbs up"),
              QStringLiteral("People")),
    });

    // Name matches are substring matches.
    QCOMPARE(emojisOf(model.search(QStringLiteral("thumb"))),
             QStringList({thumbsUp()}));
    QCOMPARE(emojisOf(model.search(QStringLiteral("face"))),
             QStringList({grinning()}));
    // The emoji itself is searchable too.
    QCOMPARE(emojisOf(model.search(grinning())), QStringList({grinning()}));
    // Category names are not part of the search.
    QVERIFY(model.search(QStringLiteral("Smileys")).isEmpty());
    QVERIFY(model.search(QStringLiteral("missing")).isEmpty());
}

void EmojiCatalogModelTest::searchIgnoresCase()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face")),
        entry(joy(), QStringLiteral("Face with tears of joy")),
    });

    QCOMPARE(emojisOf(model.search(QStringLiteral("GRINNING"))),
             QStringList({grinning()}));
    QCOMPARE(emojisOf(model.search(QStringLiteral("grin"))),
             QStringList({grinning()}));
    QCOMPARE(emojisOf(model.search(QStringLiteral("TEARS"))),
             QStringList({joy()}));
}

void EmojiCatalogModelTest::searchKeepsStoredOrder()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Smiling face")),
        entry(thumbsUp(), QStringLiteral("Smiling thumb")),
        entry(joy(), QStringLiteral("Tears of joy")),
    });

    QCOMPARE(namesOf(model.search(QStringLiteral("smil"))),
             QStringList({QStringLiteral("Smiling face"),
                          QStringLiteral("Smiling thumb")}));
    QCOMPARE(model.count(), 3);
}

void EmojiCatalogModelTest::searchRespectsLimit()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face")),
        entry(joy(), QStringLiteral("Joy")),
        entry(thumbsUp(), QStringLiteral("Thumbs up")),
        entry(heart(), QStringLiteral("Heart")),
    });

    QCOMPARE(emojisOf(model.search(QString(), 2)),
             QStringList({grinning(), joy()}));
    QCOMPARE(model.search(QString(), 1).size(), 1);
    QCOMPARE(model.search(QString(), 10).size(), 4);
    // "Grinning face" and "Heart" both contain "e"; the cap keeps the first.
    QCOMPARE(emojisOf(model.search(QStringLiteral("e"), 1)),
             QStringList({grinning()}));
}

void EmojiCatalogModelTest::searchWithZeroLimitReturnsNothing()
{
    EmojiCatalogModel model;
    model.setEntries({entry(grinning(), QStringLiteral("Grinning face"))});

    QVERIFY(model.search(QString(), 0).isEmpty());
    QVERIFY(model.search(QStringLiteral("grin"), 0).isEmpty());
    // Asking for nothing does not touch the catalog.
    QCOMPARE(model.count(), 1);
}

void EmojiCatalogModelTest::searchWithNegativeLimitIsUnlimited()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face")),
        entry(joy(), QStringLiteral("Joy")),
        entry(thumbsUp(), QStringLiteral("Thumbs up")),
    });

    QCOMPARE(model.search(QString(), -1).size(), 3);
    QCOMPARE(emojisOf(model.search(QStringLiteral("j"), -5)),
             QStringList({joy()}));
}

void EmojiCatalogModelTest::searchWithEmptyQueryReturnsEverything()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face")),
        entry(joy(), QStringLiteral("Joy")),
        entry(thumbsUp(), QStringLiteral("Thumbs up")),
    });

    QCOMPARE(model.search(QString()).size(), 3);
    QCOMPARE(model.search(QString(), 50).size(), 3);
    // The limit still applies to an empty query.
    QCOMPARE(model.search(QString(), 2).size(), 2);
}

void EmojiCatalogModelTest::recentStartsEmpty()
{
    EmojiCatalogModel model;

    QVERIFY(model.recent().isEmpty());
    model.setEntries({entry(grinning(), QStringLiteral("Grinning face"))});
    QVERIFY(model.recent().isEmpty());
}

void EmojiCatalogModelTest::addRecentKeepsNewestFirst()
{
    EmojiCatalogModel model;
    model.addRecent(grinning());
    model.addRecent(joy());
    model.addRecent(thumbsUp());

    QCOMPARE(model.recent(), QStringList({thumbsUp(), joy(), grinning()}));
}

void EmojiCatalogModelTest::addRecentMovesExistingEmojiToFront()
{
    EmojiCatalogModel model;
    model.addRecent(grinning());
    model.addRecent(joy());
    model.addRecent(grinning());

    // Reusing an emoji moves it to the front instead of duplicating it.
    QCOMPARE(model.recent(), QStringList({grinning(), joy()}));
}

void EmojiCatalogModelTest::addRecentCapsList()
{
    EmojiCatalogModel model;
    model.addRecent(grinning(), 3);
    model.addRecent(joy(), 3);
    model.addRecent(thumbsUp(), 3);
    model.addRecent(heart(), 3);

    QCOMPARE(model.recent(),
             QStringList({heart(), thumbsUp(), joy()}));

    // A later, smaller cap trims the list on the next insert.
    model.addRecent(grinning(), 2);
    QCOMPARE(model.recent(), QStringList({grinning(), heart()}));
}

void EmojiCatalogModelTest::addRecentIgnoresEmptyEmoji()
{
    EmojiCatalogModel model;
    model.addRecent(grinning());
    model.addRecent(QString());

    QCOMPARE(model.recent(), QStringList({grinning()}));
}

void EmojiCatalogModelTest::addRecentIgnoresNonPositiveCap()
{
    EmojiCatalogModel model;
    model.addRecent(grinning(), 0);
    model.addRecent(joy(), -4);

    QVERIFY(model.recent().isEmpty());
}

void EmojiCatalogModelTest::recentSurvivesCatalogReplacement()
{
    EmojiCatalogModel model;
    model.addRecent(grinning());
    model.setEntries({entry(joy(), QStringLiteral("Joy"))});
    model.setEntries({});

    // The recent list is independent of the catalog that supplied the emoji.
    QCOMPARE(model.recent(), QStringList({grinning()}));
}

void EmojiCatalogModelTest::clearDropsCatalogAndRecent()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
    });
    model.addRecent(grinning());

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.categories().isEmpty());
    QVERIFY(model.search(QString()).isEmpty());
    QVERIFY(model.recent().isEmpty());
}

void EmojiCatalogModelTest::emitsChangedOnlyOnRealEntryChanges()
{
    EmojiCatalogModel model;
    QSignalSpy spy(&model, &EmojiCatalogModel::changed);

    model.setEntries({entry(grinning(), QStringLiteral("Grinning face"))});
    QCOMPARE(spy.count(), 1);

    // The same catalog again is not a change.
    model.setEntries({entry(grinning(), QStringLiteral("Grinning face"))});
    QCOMPARE(spy.count(), 1);

    // A different name for the same emoji is a change.
    model.setEntries({entry(grinning(), QStringLiteral("Grin"))});
    QCOMPARE(spy.count(), 2);

    model.setEntries({entry(grinning(), QStringLiteral("Grin")),
                      entry(joy(), QStringLiteral("Joy"))});
    QCOMPARE(spy.count(), 3);

    model.setEntries({});
    QCOMPARE(spy.count(), 4);

    // Already empty stays silent.
    model.setEntries({});
    QCOMPARE(spy.count(), 4);
}

void EmojiCatalogModelTest::emitsChangedForAddRecentChanges()
{
    EmojiCatalogModel model;
    QSignalSpy spy(&model, &EmojiCatalogModel::changed);

    model.addRecent(grinning());
    QCOMPARE(spy.count(), 1);

    // The emoji is already first, so nothing changed.
    model.addRecent(grinning());
    QCOMPARE(spy.count(), 1);

    model.addRecent(joy());
    QCOMPARE(spy.count(), 2);

    // Reordering an existing emoji is a change.
    model.addRecent(grinning());
    QCOMPARE(spy.count(), 3);

    // Ignored calls do not notify.
    model.addRecent(QString());
    model.addRecent(thumbsUp(), 0);
    model.addRecent(thumbsUp(), -1);
    QCOMPARE(spy.count(), 3);
}

void EmojiCatalogModelTest::emitsChangedOncePerClear()
{
    EmojiCatalogModel model;
    QSignalSpy spy(&model, &EmojiCatalogModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setEntries({entry(grinning(), QStringLiteral("Grinning face"))});
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);

    // A recent-only model still reports the clear.
    model.addRecent(joy());
    QCOMPARE(spy.count(), 3);
    model.clear();
    QCOMPARE(spy.count(), 4);
    QVERIFY(model.recent().isEmpty());
}

void EmojiCatalogModelTest::returnsIndependentCopies()
{
    EmojiCatalogModel model;
    model.setEntries({
        entry(grinning(), QStringLiteral("Grinning face"),
              QStringLiteral("Smileys")),
        entry(joy(), QStringLiteral("Joy"), QStringLiteral("Smileys")),
    });
    model.addRecent(grinning());

    QVector<EmojiEntry> snapshot = model.search(QString());
    snapshot[0].name = QStringLiteral("mutated");
    snapshot.append(entry(thumbsUp(), QStringLiteral("extra")));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.search(QString()).at(0).name,
             QStringLiteral("Grinning face"));

    QVector<EmojiEntry> smileys = model.byCategory(QStringLiteral("Smileys"));
    smileys[0].emoji = thumbsUp();
    QCOMPARE(model.byCategory(QStringLiteral("Smileys")).at(0).emoji,
             grinning());

    QStringList categories = model.categories();
    categories.append(QStringLiteral("Extra"));
    QCOMPARE(model.categories(), QStringList({QStringLiteral("Smileys")}));

    QStringList recent = model.recent();
    recent.append(joy());
    QCOMPARE(model.recent(), QStringList({grinning()}));
}

QTEST_GUILESS_MAIN(EmojiCatalogModelTest)
#include "tst_emoji_catalog_model.moc"
