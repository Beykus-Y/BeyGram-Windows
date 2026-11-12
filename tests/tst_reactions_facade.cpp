#include "ui/ReactionsFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::ui::ReactionsFacade;

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

QStringList defaultTop()
{
    return {thumbsUp(), heart(), grinning()};
}

QStringList defaultRecent()
{
    return {joy(), thumbsUp()};
}

/// Applies the shared three-top/two-recent catalog with a limit of 3.
void useDefaultCatalog(ReactionsFacade& facade)
{
    facade.setCatalog(defaultTop(), defaultRecent(), 3);
}

} // namespace

class ReactionsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setCatalogKeepsOrderAndDuplicates();
    void setCatalogDropsEmptyEntries();
    void setCatalogCopiesMaxCountVerbatim();
    void setCatalogEmitsChangedOnce();
    void setCatalogDropsVanishedSelection();
    void setCatalogKeepsSurvivingSelection();
    void toggleWithoutTargetOnlyUpdatesSelection();
    void toggleEmitsRequestForOpenTarget();
    void toggleSameEmojiEmitsEmptyRemoval();
    void toggleUnavailableEmojiIsIgnored();
    void toggleEmptyEmojiIsIgnored();
    void selectingAnotherEmojiReplacesReaction();
    void clearSelectionDoesNotRequest();
    void clearSelectionAllowsReselect();
    void expandedStartsCollapsed();
    void setExpandedEmitsOnFlipOnly();
    void openForClearsStaleSelection();
    void openForEmptyChatSuppressesRequest();
    void openForEmptyMessageSuppressesRequest();
    void requestsUseTheLatestTarget();
    void toggleEmitsOneRequestPerChange();
};

void ReactionsFacadeTest::startsEmpty()
{
    ReactionsFacade facade;

    QVERIFY(facade.top().isEmpty());
    QVERIFY(facade.recent().isEmpty());
    QCOMPARE(facade.maxCount(), 0);
    QCOMPARE(facade.selected(), QString());
    QVERIFY(!facade.expanded());
}

void ReactionsFacadeTest::setCatalogKeepsOrderAndDuplicates()
{
    ReactionsFacade facade;
    facade.setCatalog({heart(), thumbsUp(), heart()}, {joy(), heart()}, 5);

    QCOMPARE(facade.top(), (QStringList{heart(), thumbsUp(), heart()}));
    QCOMPARE(facade.recent(), (QStringList{joy(), heart()}));
    QCOMPARE(facade.maxCount(), 5);
}

void ReactionsFacadeTest::setCatalogDropsEmptyEntries()
{
    ReactionsFacade facade;
    facade.setCatalog({heart(), QString(), thumbsUp()}, {QString(), joy()}, 2);

    // An empty emoji cannot be selected, so it never becomes a button.
    QCOMPARE(facade.top(), (QStringList{heart(), thumbsUp()}));
    QCOMPARE(facade.recent(), (QStringList{joy()}));
    QCOMPARE(facade.maxCount(), 2);
}

void ReactionsFacadeTest::setCatalogCopiesMaxCountVerbatim()
{
    ReactionsFacade facade;

    facade.setCatalog(defaultTop(), {}, 0);
    QCOMPARE(facade.maxCount(), 0);

    facade.setCatalog(defaultTop(), {}, 25);
    QCOMPARE(facade.maxCount(), 25);
}

void ReactionsFacadeTest::setCatalogEmitsChangedOnce()
{
    ReactionsFacade facade;
    QSignalSpy spy(&facade, &ReactionsFacade::changed);

    useDefaultCatalog(facade);
    QCOMPARE(spy.count(), 1);

    // Re-applying the identical catalog is not a change.
    useDefaultCatalog(facade);
    QCOMPARE(spy.count(), 1);
}

void ReactionsFacadeTest::setCatalogDropsVanishedSelection()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.toggle(thumbsUp());
    QCOMPARE(facade.selected(), thumbsUp());

    QSignalSpy spy(&facade, &ReactionsFacade::changed);
    facade.setCatalog({heart()}, {}, 1);

    QCOMPARE(facade.selected(), QString());
    QCOMPARE(spy.count(), 1);
}

void ReactionsFacadeTest::setCatalogKeepsSurvivingSelection()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.toggle(heart());

    QSignalSpy spy(&facade, &ReactionsFacade::changed);
    facade.setCatalog({heart()}, {}, 1);

    QCOMPARE(facade.selected(), heart());
    QCOMPARE(spy.count(), 1); // Only the catalog changed, not the selection.
}

void ReactionsFacadeTest::toggleWithoutTargetOnlyUpdatesSelection()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);

    QSignalSpy requestSpy(&facade, &ReactionsFacade::reactionRequested);
    QSignalSpy changedSpy(&facade, &ReactionsFacade::changed);

    facade.toggle(thumbsUp());

    QCOMPARE(facade.selected(), thumbsUp());
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 0);
}

void ReactionsFacadeTest::toggleEmitsRequestForOpenTarget()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(thumbsUp());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("42"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("7"));
    QCOMPARE(spy.at(0).at(2).toString(), thumbsUp());
    QCOMPARE(facade.selected(), thumbsUp());
}

void ReactionsFacadeTest::toggleSameEmojiEmitsEmptyRemoval()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(thumbsUp());
    facade.toggle(thumbsUp());

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(2).toString(), QString());
    QCOMPARE(facade.selected(), QString());
}

void ReactionsFacadeTest::toggleUnavailableEmojiIsIgnored()
{
    ReactionsFacade facade;
    facade.setCatalog({thumbsUp()}, {}, 1);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy requestSpy(&facade, &ReactionsFacade::reactionRequested);
    QSignalSpy changedSpy(&facade, &ReactionsFacade::changed);

    facade.toggle(joy());

    QCOMPARE(facade.selected(), QString());
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void ReactionsFacadeTest::toggleEmptyEmojiIsIgnored()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(QString());

    QCOMPARE(facade.selected(), QString());
    QCOMPARE(spy.count(), 0);
}

void ReactionsFacadeTest::selectingAnotherEmojiReplacesReaction()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(thumbsUp());
    facade.toggle(heart());

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(2).toString(), heart());
    QCOMPARE(facade.selected(), heart());
}

void ReactionsFacadeTest::clearSelectionDoesNotRequest()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(thumbsUp());
    facade.clearSelection();

    QCOMPARE(facade.selected(), QString());
    QCOMPARE(spy.count(), 1); // Dismissing the popup is not a removal request.
}

void ReactionsFacadeTest::clearSelectionAllowsReselect()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(thumbsUp());
    facade.clearSelection();
    facade.toggle(thumbsUp());

    QCOMPARE(spy.count(), 2);
    // After a local reset the same emoji is a fresh selection, not a removal.
    QCOMPARE(spy.at(1).at(2).toString(), thumbsUp());
}

void ReactionsFacadeTest::expandedStartsCollapsed()
{
    ReactionsFacade facade;
    QVERIFY(!facade.expanded());
}

void ReactionsFacadeTest::setExpandedEmitsOnFlipOnly()
{
    ReactionsFacade facade;
    QSignalSpy spy(&facade, &ReactionsFacade::changed);

    facade.setExpanded(true);
    QVERIFY(facade.expanded());
    QCOMPARE(spy.count(), 1);

    facade.setExpanded(true);
    QCOMPARE(spy.count(), 1);

    facade.setExpanded(false);
    QVERIFY(!facade.expanded());
    QCOMPARE(spy.count(), 2);
}

void ReactionsFacadeTest::openForClearsStaleSelection()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);

    facade.openFor(QStringLiteral("1"), QStringLiteral("10"));
    facade.toggle(thumbsUp());
    QCOMPARE(facade.selected(), thumbsUp());

    facade.openFor(QStringLiteral("2"), QStringLiteral("20"));
    QCOMPARE(facade.selected(), QString());

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(thumbsUp());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("2"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("20"));
    QCOMPARE(spy.at(0).at(2).toString(), thumbsUp());
}

void ReactionsFacadeTest::openForEmptyChatSuppressesRequest()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QString(), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(heart());

    QCOMPARE(facade.selected(), heart());
    QCOMPARE(spy.count(), 0);
}

void ReactionsFacadeTest::openForEmptyMessageSuppressesRequest()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QString());

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);
    facade.toggle(heart());

    QCOMPARE(facade.selected(), heart());
    QCOMPARE(spy.count(), 0);
}

void ReactionsFacadeTest::requestsUseTheLatestTarget()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);

    facade.openFor(QStringLiteral("1"), QStringLiteral("10"));
    facade.toggle(thumbsUp());

    facade.openFor(QStringLiteral("2"), QStringLiteral("20"));
    facade.toggle(heart());

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("1"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("10"));
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("2"));
    QCOMPARE(spy.at(1).at(1).toString(), QStringLiteral("20"));
}

void ReactionsFacadeTest::toggleEmitsOneRequestPerChange()
{
    ReactionsFacade facade;
    useDefaultCatalog(facade);
    facade.openFor(QStringLiteral("42"), QStringLiteral("7"));

    QSignalSpy spy(&facade, &ReactionsFacade::reactionRequested);

    facade.toggle(thumbsUp()); // select
    facade.toggle(thumbsUp()); // remove
    facade.toggle(thumbsUp()); // select again

    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(0).at(2).toString(), thumbsUp());
    QCOMPARE(spy.at(1).at(2).toString(), QString());
    QCOMPARE(spy.at(2).at(2).toString(), thumbsUp());
}

QTEST_GUILESS_MAIN(ReactionsFacadeTest)
#include "tst_reactions_facade.moc"
