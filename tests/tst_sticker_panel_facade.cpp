#include "ui/StickerPanelFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::ui::StickerPanelFacade;

namespace {

// Every mutator is expected to report a real change exactly once and a no-op
// not at all, so the helpers below always look at the same two spies.
struct Facade {
    StickerPanelFacade facade;
    QSignalSpy changed{&facade, &StickerPanelFacade::changed};
    QSignalSpy activated{&facade, &StickerPanelFacade::stickerActivated};

    void reset()
    {
        changed.clear();
        activated.clear();
    }
};

QString activatedId(const QSignalSpy& spy, int index = 0)
{
    const QList<QVariant> arguments = spy.at(index);
    return arguments.isEmpty() ? QString() : arguments.at(0).toString();
}

} // namespace

class StickerPanelFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void defaults();
    void openAndClose();
    void queryDelegation();
    void setsAndTabs();
    void stickersBelongToTheSelectedSet();
    void recentOrdering();
    void recordSentEmitsActivation();
    void invalidIdsAreIgnored();
    void loadingMirrorsTheSearchController();
    void clearResetsEverything();
    void noOpCallsStaySilent();
};

void StickerPanelFacadeTest::defaults()
{
    StickerPanelFacade facade;

    QCOMPARE(facade.open(), false);
    QCOMPARE(facade.query(), QString());
    QCOMPARE(facade.recentStickers(), QStringList());
    QCOMPARE(facade.setTitles(), QStringList());
    QCOMPARE(facade.currentSetIndex(), -1);
    QCOMPARE(facade.loading(), false);
    QCOMPARE(facade.stickers(), QStringList());
}

void StickerPanelFacadeTest::openAndClose()
{
    Facade f;

    f.facade.setOpen(true);
    QCOMPARE(f.facade.open(), true);
    QCOMPARE(f.changed.count(), 1);

    // Already open: no state moved, so no repaint is requested.
    f.reset();
    f.facade.setOpen(true);
    QCOMPARE(f.facade.open(), true);
    QCOMPARE(f.changed.count(), 0);

    f.facade.setOpen(false);
    QCOMPARE(f.facade.open(), false);
    QCOMPARE(f.changed.count(), 1);

    // Closing keeps the loaded state; only the flag moves.
    f.facade.setSets({QStringLiteral("Cats")});
    f.facade.setSetStickers({QStringLiteral("42")});
    f.reset();
    f.facade.setOpen(true);
    QCOMPARE(f.changed.count(), 1);
    QCOMPARE(f.facade.setTitles(), QStringList{QStringLiteral("Cats")});
    QCOMPARE(f.facade.stickers(), QStringList{QStringLiteral("42")});
}

void StickerPanelFacadeTest::queryDelegation()
{
    Facade f;

    f.facade.setQuery(QStringLiteral("  cats  "));
    // StickerSearchController trims, and the facade reports the trimmed value.
    QCOMPARE(f.facade.query(), QStringLiteral("cats"));
    QCOMPARE(f.changed.count(), 1);

    // Same query after trimming: the controller keeps its results, so nothing
    // is emitted.
    f.reset();
    f.facade.setQuery(QStringLiteral("cats "));
    QCOMPARE(f.facade.query(), QStringLiteral("cats"));
    QCOMPARE(f.changed.count(), 0);

    f.facade.setQuery(QStringLiteral(""));
    QCOMPARE(f.facade.query(), QString());
    QCOMPARE(f.changed.count(), 1);

    f.reset();
    f.facade.setQuery(QStringLiteral("   "));
    QCOMPARE(f.facade.query(), QString());
    QCOMPARE(f.changed.count(), 0);
}

void StickerPanelFacadeTest::setsAndTabs()
{
    Facade f;

    f.facade.setSets({QStringLiteral(" Cats "), QString(), QStringLiteral("Dogs")});
    // Blank titles are skipped, the rest keep their order.
    QCOMPARE(f.facade.setTitles(),
             QStringList({QStringLiteral("Cats"), QStringLiteral("Dogs")}));
    QCOMPARE(f.facade.currentSetIndex(), 0);
    QCOMPARE(f.changed.count(), 1);

    f.reset();
    f.facade.setCurrentSetIndex(1);
    QCOMPARE(f.facade.currentSetIndex(), 1);
    QCOMPARE(f.changed.count(), 1);

    // Out of range indices are ignored, not clamped, so a stale click cannot
    // move the panel somewhere the strip does not show.
    f.reset();
    f.facade.setCurrentSetIndex(-1);
    f.facade.setCurrentSetIndex(2);
    f.facade.setCurrentSetIndex(1);
    QCOMPARE(f.facade.currentSetIndex(), 1);
    QCOMPARE(f.changed.count(), 0);

    // A shrinking strip clamps the selection instead of leaving it on a dead
    // tab.
    f.facade.setSets({QStringLiteral("Cats")});
    QCOMPARE(f.facade.setTitles(), QStringList{QStringLiteral("Cats")});
    QCOMPARE(f.facade.currentSetIndex(), 0);

    // No sets at all means no selection.
    f.facade.setSets({});
    QCOMPARE(f.facade.setTitles(), QStringList());
    QCOMPARE(f.facade.currentSetIndex(), -1);
}

void StickerPanelFacadeTest::stickersBelongToTheSelectedSet()
{
    Facade f;

    f.facade.setSets({QStringLiteral("Cats"), QStringLiteral("Dogs")});
    f.facade.setSetStickers({QStringLiteral("11"), QStringLiteral("12")});
    QCOMPARE(f.facade.stickers(), QStringList({QStringLiteral("11"), QStringLiteral("12")}));

    // Switching the tab drops the previous set's stickers: the picker
    // controller does that, and the facade surfaces it as a change.
    f.reset();
    f.facade.setCurrentSetIndex(1);
    QCOMPARE(f.facade.stickers(), QStringList());
    QCOMPARE(f.changed.count(), 1);

    f.facade.setSetStickers({QStringLiteral("21"), QStringLiteral("22")});
    QCOMPARE(f.facade.stickers(), QStringList({QStringLiteral("21"), QStringLiteral("22")}));

    // Coming back shows the first set empty, because its stickers were dropped
    // when the tab moved.
    f.facade.setCurrentSetIndex(0);
    QCOMPARE(f.facade.stickers(), QStringList());

    f.facade.setSetStickers({QStringLiteral("11")});
    QCOMPARE(f.facade.stickers(), QStringList{QStringLiteral("11")});

    // Repeated ids are kept once, first-seen order, and non-positive or
    // unparsable ids are dropped.
    f.facade.setSetStickers({QStringLiteral("12"), QStringLiteral("11"),
                             QStringLiteral("12"), QStringLiteral("0"),
                             QStringLiteral("-3"), QStringLiteral("nope")});
    QCOMPARE(f.facade.stickers(), QStringList({QStringLiteral("12"), QStringLiteral("11")}));
}

void StickerPanelFacadeTest::recentOrdering()
{
    Facade f;

    f.facade.recordSent(QStringLiteral("7"));
    QCOMPARE(f.facade.recentStickers(), QStringList{QStringLiteral("7")});

    f.facade.recordSent(QStringLiteral("9"));
    QCOMPARE(f.facade.recentStickers(),
             QStringList({QStringLiteral("9"), QStringLiteral("7")}));

    // Re-sending moves the sticker to the front instead of duplicating it.
    f.facade.recordSent(QStringLiteral("7"));
    QCOMPARE(f.facade.recentStickers(),
             QStringList({QStringLiteral("7"), QStringLiteral("9")}));
    QCOMPARE(f.facade.recentStickers().count(), 2);

    // Re-sending the newest entry keeps the order, so there is nothing to
    // repaint.
    f.reset();
    f.facade.recordSent(QStringLiteral("7"));
    QCOMPARE(f.facade.recentStickers(),
             QStringList({QStringLiteral("7"), QStringLiteral("9")}));
    QCOMPARE(f.changed.count(), 0);
}

void StickerPanelFacadeTest::recordSentEmitsActivation()
{
    Facade f;

    f.facade.recordSent(QStringLiteral(" 12345 "));
    QCOMPARE(f.activated.count(), 1);
    QCOMPARE(activatedId(f.activated), QStringLiteral("12345"));

    f.facade.recordSent(QStringLiteral("99"));
    QCOMPARE(f.activated.count(), 2);
    QCOMPARE(activatedId(f.activated, 1), QStringLiteral("99"));

    // A re-sent sticker still asks the owner to send it again.
    f.facade.recordSent(QStringLiteral("99"));
    QCOMPARE(f.activated.count(), 3);
    QCOMPARE(activatedId(f.activated, 2), QStringLiteral("99"));
}

void StickerPanelFacadeTest::invalidIdsAreIgnored()
{
    Facade f;

    f.facade.recordSent(QStringLiteral("0"));
    f.facade.recordSent(QStringLiteral("-4"));
    f.facade.recordSent(QStringLiteral("abc"));
    f.facade.recordSent(QString());

    QCOMPARE(f.facade.recentStickers(), QStringList());
    QCOMPARE(f.changed.count(), 0);
    QCOMPARE(f.activated.count(), 0);
}

void StickerPanelFacadeTest::loadingMirrorsTheSearchController()
{
    StickerPanelFacade facade;

    QCOMPARE(facade.loading(), false);

    // A new query drops any in-flight request, so the panel cannot stay busy on
    // results that no longer belong to the text.
    facade.setQuery(QStringLiteral("cats"));
    QCOMPARE(facade.loading(), false);

    facade.setQuery(QStringLiteral("dogs"));
    QCOMPARE(facade.loading(), false);

    facade.setQuery(QString());
    QCOMPARE(facade.loading(), false);
}

void StickerPanelFacadeTest::clearResetsEverything()
{
    Facade f;

    f.facade.setOpen(true);
    f.facade.setQuery(QStringLiteral("cats"));
    f.facade.setSets({QStringLiteral("Cats"), QStringLiteral("Dogs")});
    f.facade.setCurrentSetIndex(1);
    f.facade.setSetStickers({QStringLiteral("21")});
    f.facade.recordSent(QStringLiteral("21"));

    f.reset();
    f.facade.clear();

    QCOMPARE(f.facade.open(), false);
    QCOMPARE(f.facade.query(), QString());
    QCOMPARE(f.facade.setTitles(), QStringList());
    QCOMPARE(f.facade.currentSetIndex(), -1);
    QCOMPARE(f.facade.stickers(), QStringList());
    QCOMPARE(f.facade.recentStickers(), QStringList());
    QCOMPARE(f.facade.loading(), false);
    // One repaint for the whole reset, not one per inner controller.
    QCOMPARE(f.changed.count(), 1);
    QCOMPARE(f.activated.count(), 0);

    // Clearing an already empty panel is silent.
    f.reset();
    f.facade.clear();
    QCOMPARE(f.changed.count(), 0);
}

void StickerPanelFacadeTest::noOpCallsStaySilent()
{
    Facade f;

    f.facade.setOpen(false);
    f.facade.setQuery(QString());
    f.facade.setCurrentSetIndex(0);
    f.facade.setSetStickers({});
    f.facade.setSets({});

    QCOMPARE(f.changed.count(), 0);

    // Same content handed in twice is not a change either.
    f.facade.setSets({QStringLiteral("Cats")});
    f.reset();
    f.facade.setSets({QStringLiteral("Cats")});
    QCOMPARE(f.changed.count(), 0);

    f.facade.setSetStickers({QStringLiteral("5")});
    f.reset();
    f.facade.setSetStickers({QStringLiteral("5")});
    QCOMPARE(f.changed.count(), 0);
}

QTEST_GUILESS_MAIN(StickerPanelFacadeTest)

#include "tst_sticker_panel_facade.moc"
