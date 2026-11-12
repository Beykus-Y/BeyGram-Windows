#include "telegram/ShareTargets.h"

#include <QSignalSpy>
#include <QVector>
#include <QtTest>

#include <initializer_list>

using namespace beygram::telegram;

namespace {

ShareTarget makeTarget(qint64 chatId, const QString& title,
                       const QString& subtitle = QString(),
                       const QString& kind = QString(), int unreadCount = 0)
{
    ShareTarget target;
    target.chatId = chatId;
    target.title = title;
    target.subtitle = subtitle;
    target.kind = kind;
    target.unreadCount = unreadCount;
    return target;
}

QVector<qint64> ids(std::initializer_list<qint64> values)
{
    return QVector<qint64>(values);
}

QVector<qint64> idsOf(const QVector<ShareTarget>& targets)
{
    QVector<qint64> result;
    result.reserve(targets.size());
    for (const ShareTarget& target : targets)
        result.append(target.chatId);
    return result;
}

QVector<QString> titlesOf(const QVector<ShareTarget>& targets)
{
    QVector<QString> result;
    result.reserve(targets.size());
    for (const ShareTarget& target : targets)
        result.append(target.title);
    return result;
}

QVector<qint64> countedIds(int count)
{
    QVector<qint64> values;
    values.reserve(count);
    for (int i = 1; i <= count; ++i)
        values.append(qint64(i));
    return values;
}

} // namespace

class ShareTargetModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setTargetsDropsNonPositiveAndDuplicates();
    void setTargetsKeepsEveryField();
    void setTargetsDoesNotTouchRecents();
    void setTargetsWithEmptyListDropsAll();

    void filteredMatchesTitleCaseInsensitively();
    void filteredMatchesSubtitle();
    void filteredIgnoresKind();
    void filteredBlankQueryReturnsEverything();
    void filteredTrimsTheQuery();
    void filteredWithoutMatchIsEmpty();

    void setRecentDedupesNewestFirst();
    void setRecentDropsNonPositiveIds();
    void setRecentCapsAtDefault();
    void setRecentReplacesPreviousList();

    void recentTargetsFollowsRecencyOrder();
    void recentTargetsSkipsIdsWithoutTarget();
    void recentTargetsSurvivesTargetReload();
    void recentTargetsHidesChatThatDisappeared();

    void addRecentPutsNewestFirst();
    void addRecentMovesExistingToFront();
    void addRecentIgnoresNonPositiveIds();
    void addRecentCapsAtDefault();
    void addRecentRespectsCustomCap();
    void addRecentClampsCapToOne();
    void addRecentOnNewestIsSilent();
    void addRecentTrimsWhenNewestExceedsCap();
    void addRecentKeepsIdWithoutTarget();

    void clearDropsTargetsAndRecents();
    void clearOnEmptyIsSilent();

    void changedOnlyOnRealTargetChanges();
    void changedOnlyOnRealRecentChanges();
    void returnsIndependentCopies();
};

void ShareTargetModelTest::startsEmpty()
{
    ShareTargetModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.targets().isEmpty());
    QVERIFY(model.filtered(QString()).isEmpty());
    QVERIFY(model.recent().isEmpty());
    QVERIFY(model.recentTargets().isEmpty());
}

void ShareTargetModelTest::setTargetsDropsNonPositiveAndDuplicates()
{
    ShareTargetModel model;
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    model.setTargets({makeTarget(3, QStringLiteral("Three")),
                      makeTarget(0, QStringLiteral("Zero")),
                      makeTarget(1, QStringLiteral("One")),
                      makeTarget(3, QStringLiteral("Three again")),
                      makeTarget(-5, QStringLiteral("Negative")),
                      makeTarget(2, QStringLiteral("Two"))});

    // The first occurrence of an id wins, so the duplicate title is gone.
    QCOMPARE(idsOf(model.targets()), (QVector<qint64>{3, 1, 2}));
    QCOMPARE(titlesOf(model.targets()),
             (QVector<QString>{QStringLiteral("Three"), QStringLiteral("One"),
                               QStringLiteral("Two")}));
    QCOMPARE(model.count(), 3);
    QCOMPARE(spy.count(), 1);
}

void ShareTargetModelTest::setTargetsKeepsEveryField()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(7, QStringLiteral("Team"),
                                 QStringLiteral("team_username"),
                                 QStringLiteral("group"), 4)});

    const ShareTarget stored = model.targets().first();
    QCOMPARE(stored.chatId, qint64(7));
    QCOMPARE(stored.title, QStringLiteral("Team"));
    QCOMPARE(stored.subtitle, QStringLiteral("team_username"));
    QCOMPARE(stored.kind, QStringLiteral("group"));
    QCOMPARE(stored.unreadCount, 4);
}

void ShareTargetModelTest::setTargetsDoesNotTouchRecents()
{
    ShareTargetModel model;
    model.setRecent({2, 1});
    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"))});

    // A chat list refresh must not forget the destinations the user used.
    QCOMPARE(model.recent(), ids({2, 1}));
    QCOMPARE(idsOf(model.recentTargets()), ids({2, 1}));
}

void ShareTargetModelTest::setTargetsWithEmptyListDropsAll()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("One"))});
    model.setTargets({});

    QCOMPARE(model.count(), 0);
    QVERIFY(model.targets().isEmpty());
}

void ShareTargetModelTest::filteredMatchesTitleCaseInsensitively()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("Alice Adams")),
                      makeTarget(2, QStringLiteral("Bob Brown")),
                      makeTarget(3, QStringLiteral("Team ALPHA"))});

    QCOMPARE(idsOf(model.filtered(QStringLiteral("alice"))), ids({1}));
    QCOMPARE(idsOf(model.filtered(QStringLiteral("ALPHA"))), ids({3}));
    QCOMPARE(idsOf(model.filtered(QStringLiteral("brown"))), ids({2}));
}

void ShareTargetModelTest::filteredMatchesSubtitle()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("Alice"),
                                 QStringLiteral("alice_username")),
                      makeTarget(2, QStringLiteral("Bob"),
                                 QStringLiteral("bob_username"))});

    QCOMPARE(idsOf(model.filtered(QStringLiteral("USERNAME"))), ids({1, 2}));
    QCOMPARE(idsOf(model.filtered(QStringLiteral("bob_user"))), ids({2}));
}

void ShareTargetModelTest::filteredIgnoresKind()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("Alice"), QString(),
                                 QStringLiteral("group")),
                      makeTarget(2, QStringLiteral("Bob"), QString(),
                                 QStringLiteral("channel"))});

    // kind is a label for the UI, not a searchable name.
    QVERIFY(model.filtered(QStringLiteral("group")).isEmpty());
    QVERIFY(model.filtered(QStringLiteral("channel")).isEmpty());
}

void ShareTargetModelTest::filteredBlankQueryReturnsEverything()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("Alice")),
                      makeTarget(2, QStringLiteral("Bob"))});

    QCOMPARE(idsOf(model.filtered(QString())), ids({1, 2}));
    QCOMPARE(idsOf(model.filtered(QStringLiteral("   "))), ids({1, 2}));
}

void ShareTargetModelTest::filteredTrimsTheQuery()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("Alice")),
                      makeTarget(2, QStringLiteral("Bob"))});

    // A mobile keyboard easily leaves spaces around the typed name.
    QCOMPARE(idsOf(model.filtered(QStringLiteral("  ali  "))), ids({1}));
}

void ShareTargetModelTest::filteredWithoutMatchIsEmpty()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("Alice")),
                      makeTarget(2, QStringLiteral("Bob"))});

    QVERIFY(model.filtered(QStringLiteral("carol")).isEmpty());
    // The stored list is not filtered in place.
    QCOMPARE(model.count(), 2);
}

void ShareTargetModelTest::setRecentDedupesNewestFirst()
{
    ShareTargetModel model;
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    model.setRecent({3, 1, 3, 2, 1});

    QCOMPARE(model.recent(), ids({3, 1, 2}));
    QCOMPARE(spy.count(), 1);
}

void ShareTargetModelTest::setRecentDropsNonPositiveIds()
{
    ShareTargetModel model;
    model.setRecent({0, -1, 5, -7, 6});

    QCOMPARE(model.recent(), ids({5, 6}));
}

void ShareTargetModelTest::setRecentCapsAtDefault()
{
    ShareTargetModel model;
    model.setRecent(countedIds(12));

    QCOMPARE(model.recent().size(),
             static_cast<qsizetype>(ShareTargetModel::kDefaultMaxRecent));
    QCOMPARE(model.recent(), countedIds(ShareTargetModel::kDefaultMaxRecent));
}

void ShareTargetModelTest::setRecentReplacesPreviousList()
{
    ShareTargetModel model;
    model.setRecent({1, 2, 3});
    model.setRecent({4, 5});

    QCOMPARE(model.recent(), ids({4, 5}));
}

void ShareTargetModelTest::recentTargetsFollowsRecencyOrder()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two")),
                      makeTarget(3, QStringLiteral("Three"))});
    model.setRecent({2, 9, 3, 1});

    // The order comes from the recents, not from the target list. The id
    // without a chat is kept in recent() but not projected.
    QCOMPARE(idsOf(model.recentTargets()), ids({2, 3, 1}));
    QCOMPARE(titlesOf(model.recentTargets()),
             (QVector<QString>{QStringLiteral("Two"), QStringLiteral("Three"),
                               QStringLiteral("One")}));
    QCOMPARE(model.recent(), ids({2, 9, 3, 1}));
}

void ShareTargetModelTest::recentTargetsSkipsIdsWithoutTarget()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("One"))});
    model.setRecent({1, 42});

    QCOMPARE(idsOf(model.recentTargets()), ids({1}));
}

void ShareTargetModelTest::recentTargetsSurvivesTargetReload()
{
    ShareTargetModel model;

    // Recents can arrive before the chat list has loaded.
    model.setRecent({2});
    QVERIFY(model.recentTargets().isEmpty());

    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"))});
    QCOMPARE(idsOf(model.recentTargets()), ids({2}));

    // A later refresh with a new title is reflected immediately.
    model.setTargets({makeTarget(2, QStringLiteral("Two renamed"))});
    QCOMPARE(titlesOf(model.recentTargets()),
             (QVector<QString>{QStringLiteral("Two renamed")}));
}

void ShareTargetModelTest::recentTargetsHidesChatThatDisappeared()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"))});
    model.setRecent({1, 2});

    model.setTargets({makeTarget(2, QStringLiteral("Two"))});

    QCOMPARE(idsOf(model.recentTargets()), ids({2}));
    // The id is remembered, so the chat returns to the strip when it reloads.
    QCOMPARE(model.recent(), ids({1, 2}));
}

void ShareTargetModelTest::addRecentPutsNewestFirst()
{
    ShareTargetModel model;
    model.addRecent(1);
    model.addRecent(2);
    model.addRecent(3);

    QCOMPARE(model.recent(), ids({3, 2, 1}));
}

void ShareTargetModelTest::addRecentMovesExistingToFront()
{
    ShareTargetModel model;
    model.setRecent({3, 2, 1});
    model.addRecent(1);

    QCOMPARE(model.recent(), ids({1, 3, 2}));
    QCOMPARE(model.recent().size(), static_cast<qsizetype>(3));
}

void ShareTargetModelTest::addRecentIgnoresNonPositiveIds()
{
    ShareTargetModel model;
    model.setRecent({4});
    model.addRecent(0);
    model.addRecent(-9);

    QCOMPARE(model.recent(), ids({4}));
}

void ShareTargetModelTest::addRecentCapsAtDefault()
{
    ShareTargetModel model;
    model.setRecent(countedIds(ShareTargetModel::kDefaultMaxRecent));
    model.addRecent(9);

    QCOMPARE(model.recent().size(),
             static_cast<qsizetype>(ShareTargetModel::kDefaultMaxRecent));
    QCOMPARE(model.recent(), ids({9, 1, 2, 3, 4, 5, 6, 7}));
}

void ShareTargetModelTest::addRecentRespectsCustomCap()
{
    ShareTargetModel model;
    model.setRecent({1, 2, 3});
    model.addRecent(4, 2);

    QCOMPARE(model.recent(), ids({4, 1}));
}

void ShareTargetModelTest::addRecentClampsCapToOne()
{
    ShareTargetModel model;
    model.setRecent({1, 2});

    // A non-positive cap must not drop the chat the user just used.
    model.addRecent(9, 0);
    QCOMPARE(model.recent(), ids({9}));

    model.addRecent(8, -3);
    QCOMPARE(model.recent(), ids({8}));
}

void ShareTargetModelTest::addRecentOnNewestIsSilent()
{
    ShareTargetModel model;
    model.setRecent({3, 2, 1});
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    model.addRecent(3);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.recent(), ids({3, 2, 1}));
}

void ShareTargetModelTest::addRecentTrimsWhenNewestExceedsCap()
{
    ShareTargetModel model;
    model.setRecent({1, 2, 3});
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    // The order of the newest entry does not change, but the tail does.
    model.addRecent(1, 2);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.recent(), ids({1, 2}));
}

void ShareTargetModelTest::addRecentKeepsIdWithoutTarget()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("One"))});
    model.addRecent(99);

    QCOMPARE(model.recent(), ids({99}));
    QVERIFY(model.recentTargets().isEmpty());

    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(99, QStringLiteral("Ninety-nine"))});
    QCOMPARE(idsOf(model.recentTargets()), ids({99}));
}

void ShareTargetModelTest::clearDropsTargetsAndRecents()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"))});
    model.setRecent({1});
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.targets().isEmpty());
    QVERIFY(model.recent().isEmpty());
    QVERIFY(model.recentTargets().isEmpty());
}

void ShareTargetModelTest::clearOnEmptyIsSilent()
{
    ShareTargetModel model;
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
}

void ShareTargetModelTest::changedOnlyOnRealTargetChanges()
{
    ShareTargetModel model;
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"))});
    QCOMPARE(spy.count(), 1);

    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"))});
    QCOMPARE(spy.count(), 1);

    // Ids that normalize away leave the stored list untouched.
    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(0, QStringLiteral("Zero")),
                      makeTarget(2, QStringLiteral("Two")),
                      makeTarget(1, QStringLiteral("Duplicate"))});
    QCOMPARE(spy.count(), 1);

    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"), QString(), QString(), 5)});
    QCOMPARE(spy.count(), 2);

    model.setTargets({makeTarget(2, QStringLiteral("Two"), QString(), QString(), 5),
                      makeTarget(1, QStringLiteral("One"))});
    QCOMPARE(spy.count(), 3);

    model.setTargets({makeTarget(1, QStringLiteral("One"))});
    QCOMPARE(spy.count(), 4);

    // Already empty stays silent.
    model.setTargets({});
    QCOMPARE(spy.count(), 5);
    model.setTargets({});
    QCOMPARE(spy.count(), 5);
}

void ShareTargetModelTest::changedOnlyOnRealRecentChanges()
{
    ShareTargetModel model;
    QSignalSpy spy(&model, &ShareTargetModel::changed);

    model.setRecent({1, 2});
    QCOMPARE(spy.count(), 1);

    model.setRecent({1, 2});
    QCOMPARE(spy.count(), 1);

    model.setRecent({1, 2, 2, 0, -1});
    QCOMPARE(spy.count(), 1);

    model.setRecent({2, 1});
    QCOMPARE(spy.count(), 2);

    model.setRecent({2, 1});
    QCOMPARE(spy.count(), 2);

    // The newest entry already is 2, so recording it again is silent.
    model.addRecent(2);
    QCOMPARE(spy.count(), 2);

    model.addRecent(3);
    QCOMPARE(spy.count(), 3);

    model.addRecent(0);
    model.addRecent(-4);
    QCOMPARE(spy.count(), 3);
}

void ShareTargetModelTest::returnsIndependentCopies()
{
    ShareTargetModel model;
    model.setTargets({makeTarget(1, QStringLiteral("One")),
                      makeTarget(2, QStringLiteral("Two"))});
    model.setRecent({1, 2});

    QVector<ShareTarget> targetCopy = model.targets();
    targetCopy[0].title = QStringLiteral("Changed");
    targetCopy.append(makeTarget(3, QStringLiteral("Three")));

    QVector<qint64> recentCopy = model.recent();
    recentCopy[0] = 99;
    recentCopy.append(3);

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.targets().first().title, QStringLiteral("One"));
    QCOMPARE(model.recent(), ids({1, 2}));
}

QTEST_GUILESS_MAIN(ShareTargetModelTest)
#include "tst_share_targets.moc"
