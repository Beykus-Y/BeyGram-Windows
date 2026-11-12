#include "telegram/NotificationSoundModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

SoundOption sound(qint64 id, const QString& title = QString(), int durationMs = 1000)
{
    SoundOption option;
    option.soundId = id;
    option.title = title;
    option.durationMs = durationMs;
    return option;
}

QVector<SoundOption> threeSounds()
{
    return {sound(10, QStringLiteral("Bell")),
            sound(20, QStringLiteral("Chime")),
            sound(30, QStringLiteral("Ping"))};
}

} // namespace

class NotificationSoundModelTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreEmptyAndSilent();
    void setOptionsStoresTheGivenSounds();
    void setOptionsDropsNonPositiveIds();
    void setOptionsKeepsFirstOfDuplicateIds();
    void setOptionsPreservesOrder();
    void setOptionsClampsNegativeDurations();
    void setOptionsEmitsChangedOnlyForRealChange();
    void countTracksStoredOptions();
    void selectAcceptsKnownId();
    void selectZeroClearsSelection();
    void selectIgnoresUnknownId();
    void selectEmitsChangedOnlyForRealChange();
    void selectedReturnsTheStoredOption();
    void selectedReturnsDefaultWhenNoneSelected();
    void selectedReturnsDefaultWhenIdIsMissing();
    void selectedTitleReturnsSilentForZero();
    void selectedTitleReturnsTitleForKnownId();
    void selectedTitleIsEmptyWhenIdIsMissing();
    void setOptionsKeepsSelectionWhenIdSurvives();
    void setOptionsKeepsDanglingSelectionWhenIdIsDropped();
    void clearResetsOptionsAndSelection();
    void clearEmitsChangedOnlyWhenThereWasSomethingToClear();
};

void NotificationSoundModelTest::defaultsAreEmptyAndSilent()
{
    const NotificationSoundModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.options().isEmpty());
    QCOMPARE(model.selectedId(), qint64(0));
    QCOMPARE(model.selected().soundId, qint64(0));
    QVERIFY(model.selected().title.isEmpty());
    QCOMPARE(model.selected().durationMs, 0);
    QCOMPARE(model.selectedTitle(), QStringLiteral("Silent"));
}

void NotificationSoundModelTest::setOptionsStoresTheGivenSounds()
{
    NotificationSoundModel model;
    const QVector<SoundOption> sounds = threeSounds();

    model.setOptions(sounds);

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.options().size(), 3);
    QCOMPARE(model.options().at(0).soundId, qint64(10));
    QCOMPARE(model.options().at(0).title, QStringLiteral("Bell"));
    QCOMPARE(model.options().at(0).durationMs, 1000);
}

void NotificationSoundModelTest::setOptionsDropsNonPositiveIds()
{
    NotificationSoundModel model;

    model.setOptions({sound(0, QStringLiteral("Zero")),
                      sound(-5, QStringLiteral("Negative")),
                      sound(7, QStringLiteral("Kept"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.options().at(0).soundId, qint64(7));
    QCOMPARE(model.options().at(0).title, QStringLiteral("Kept"));
}

void NotificationSoundModelTest::setOptionsKeepsFirstOfDuplicateIds()
{
    NotificationSoundModel model;

    model.setOptions({sound(5, QStringLiteral("First"), 100),
                      sound(5, QStringLiteral("Second"), 200),
                      sound(6, QStringLiteral("Other"), 300)});

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.options().at(0).soundId, qint64(5));
    QCOMPARE(model.options().at(0).title, QStringLiteral("First"));
    QCOMPARE(model.options().at(0).durationMs, 100);
    QCOMPARE(model.options().at(1).soundId, qint64(6));
}

void NotificationSoundModelTest::setOptionsPreservesOrder()
{
    NotificationSoundModel model;

    model.setOptions({sound(30), sound(10), sound(20)});

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.options().at(0).soundId, qint64(30));
    QCOMPARE(model.options().at(1).soundId, qint64(10));
    QCOMPARE(model.options().at(2).soundId, qint64(20));
}

void NotificationSoundModelTest::setOptionsClampsNegativeDurations()
{
    NotificationSoundModel model;

    model.setOptions({sound(1, QStringLiteral("Short"), -1),
                      sound(2, QStringLiteral("Zero"), 0),
                      sound(3, QStringLiteral("Long"), 4000)});

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.options().at(0).durationMs, 0);
    QCOMPARE(model.options().at(1).durationMs, 0);
    QCOMPARE(model.options().at(2).durationMs, 4000);

    // A negative duration clamps back to the stored 0, which is not a change.
    QSignalSpy spy(&model, &NotificationSoundModel::changed);
    model.setOptions({sound(1, QStringLiteral("Short"), -500),
                      sound(2, QStringLiteral("Zero"), 0),
                      sound(3, QStringLiteral("Long"), 4000)});
    QCOMPARE(spy.count(), 0);
}

void NotificationSoundModelTest::setOptionsEmitsChangedOnlyForRealChange()
{
    NotificationSoundModel model;
    QSignalSpy spy(&model, &NotificationSoundModel::changed);

    model.setOptions(threeSounds());
    QCOMPARE(spy.count(), 1);

    // The same list again is not a change.
    model.setOptions(threeSounds());
    QCOMPARE(spy.count(), 1);

    // A duplicate that gets dropped compares equal to what is stored, so a
    // refresh that only repeats it stays silent.
    model.setOptions({sound(10, QStringLiteral("Bell"), 1000),
                      sound(20, QStringLiteral("Chime"), 1000),
                      sound(20, QStringLiteral("Ignored"), 5000),
                      sound(30, QStringLiteral("Ping"), 1000)});
    QCOMPARE(spy.count(), 1);

    // A different title is a real change.
    model.setOptions({sound(10, QStringLiteral("Bell II"), 1000),
                      sound(20, QStringLiteral("Chime"), 1000),
                      sound(30, QStringLiteral("Ping"), 1000)});
    QCOMPARE(spy.count(), 2);
}

void NotificationSoundModelTest::countTracksStoredOptions()
{
    NotificationSoundModel model;
    QCOMPARE(model.count(), 0);

    model.setOptions(threeSounds());
    QCOMPARE(model.count(), 3);

    model.setOptions({sound(10)});
    QCOMPARE(model.count(), 1);

    model.setOptions({});
    QCOMPARE(model.count(), 0);
}

void NotificationSoundModelTest::selectAcceptsKnownId()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());

    model.select(20);

    QCOMPARE(model.selectedId(), qint64(20));
    QCOMPARE(model.selected().soundId, qint64(20));
    QCOMPARE(model.selected().title, QStringLiteral("Chime"));
    QCOMPARE(model.selectedTitle(), QStringLiteral("Chime"));
}

void NotificationSoundModelTest::selectZeroClearsSelection()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    model.select(20);

    model.select(0);

    QCOMPARE(model.selectedId(), qint64(0));
    QCOMPARE(model.selected().soundId, qint64(0));
    QVERIFY(model.selected().title.isEmpty());
    QCOMPARE(model.selectedTitle(), QStringLiteral("Silent"));
}

void NotificationSoundModelTest::selectIgnoresUnknownId()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    model.select(20);
    QSignalSpy spy(&model, &NotificationSoundModel::changed);

    model.select(99);
    QCOMPARE(model.selectedId(), qint64(20));
    QCOMPARE(spy.count(), 0);

    // An unknown id on a fresh model leaves it silent.
    NotificationSoundModel fresh;
    fresh.setOptions(threeSounds());
    fresh.select(99);
    QCOMPARE(fresh.selectedId(), qint64(0));
    QCOMPARE(fresh.selectedTitle(), QStringLiteral("Silent"));
}

void NotificationSoundModelTest::selectEmitsChangedOnlyForRealChange()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    QSignalSpy spy(&model, &NotificationSoundModel::changed);

    model.select(10);
    QCOMPARE(spy.count(), 1);

    // Reselecting the stored id is silent.
    model.select(10);
    QCOMPARE(spy.count(), 1);

    model.select(30);
    QCOMPARE(spy.count(), 2);

    model.select(0);
    QCOMPARE(spy.count(), 3);

    // Already silent.
    model.select(0);
    QCOMPARE(spy.count(), 3);
}

void NotificationSoundModelTest::selectedReturnsTheStoredOption()
{
    NotificationSoundModel model;
    model.setOptions({sound(7, QStringLiteral("Ding"), 250)});

    model.select(7);

    const SoundOption option = model.selected();
    QCOMPARE(option.soundId, qint64(7));
    QCOMPARE(option.title, QStringLiteral("Ding"));
    QCOMPARE(option.durationMs, 250);
}

void NotificationSoundModelTest::selectedReturnsDefaultWhenNoneSelected()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());

    const SoundOption option = model.selected();
    QCOMPARE(option.soundId, qint64(0));
    QVERIFY(option.title.isEmpty());
    QCOMPARE(option.durationMs, 0);
}

void NotificationSoundModelTest::selectedReturnsDefaultWhenIdIsMissing()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    model.select(20);

    // The sound disappeared from the list, so there is nothing to return.
    model.setOptions({sound(10, QStringLiteral("Bell"))});

    QCOMPARE(model.selectedId(), qint64(20));
    const SoundOption option = model.selected();
    QCOMPARE(option.soundId, qint64(0));
    QVERIFY(option.title.isEmpty());
    QCOMPARE(option.durationMs, 0);
}

void NotificationSoundModelTest::selectedTitleReturnsSilentForZero()
{
    NotificationSoundModel model;
    QCOMPARE(model.selectedTitle(), QStringLiteral("Silent"));

    model.setOptions(threeSounds());
    model.select(10);
    model.select(0);
    QCOMPARE(model.selectedTitle(), QStringLiteral("Silent"));
}

void NotificationSoundModelTest::selectedTitleReturnsTitleForKnownId()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());

    model.select(10);
    QCOMPARE(model.selectedTitle(), QStringLiteral("Bell"));

    model.select(30);
    QCOMPARE(model.selectedTitle(), QStringLiteral("Ping"));

    // An empty stored title stays empty; only id 0 spells "Silent".
    model.setOptions({sound(40)});
    model.select(40);
    QCOMPARE(model.selectedId(), qint64(40));
    QVERIFY(model.selectedTitle().isEmpty());
}

void NotificationSoundModelTest::selectedTitleIsEmptyWhenIdIsMissing()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    model.select(20);

    model.setOptions({sound(10, QStringLiteral("Bell"))});

    QCOMPARE(model.selectedTitle(), QString());
}

void NotificationSoundModelTest::setOptionsKeepsSelectionWhenIdSurvives()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    model.select(20);
    QSignalSpy spy(&model, &NotificationSoundModel::changed);

    model.setOptions({sound(20, QStringLiteral("Chime II"), 500),
                      sound(50, QStringLiteral("New"))});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedId(), qint64(20));
    QCOMPARE(model.selectedTitle(), QStringLiteral("Chime II"));
    QCOMPARE(model.selected().durationMs, 500);
}

void NotificationSoundModelTest::setOptionsKeepsDanglingSelectionWhenIdIsDropped()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    model.select(20);
    QSignalSpy spy(&model, &NotificationSoundModel::changed);

    model.setOptions({sound(10, QStringLiteral("Bell"))});

    // The options changed, but the user's choice is remembered by id.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selectedId(), qint64(20));
    QCOMPARE(model.selectedTitle(), QString());
}

void NotificationSoundModelTest::clearResetsOptionsAndSelection()
{
    NotificationSoundModel model;
    model.setOptions(threeSounds());
    model.select(20);

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.options().isEmpty());
    QCOMPARE(model.selectedId(), qint64(0));
    QCOMPARE(model.selected().soundId, qint64(0));
    QCOMPARE(model.selectedTitle(), QStringLiteral("Silent"));

    // A cleared model accepts new sounds and the silent choice again.
    model.setOptions(threeSounds());
    model.select(10);
    QCOMPARE(model.selectedTitle(), QStringLiteral("Bell"));
    model.select(0);
    QCOMPARE(model.selectedId(), qint64(0));
}

void NotificationSoundModelTest::clearEmitsChangedOnlyWhenThereWasSomethingToClear()
{
    NotificationSoundModel model;
    QSignalSpy spy(&model, &NotificationSoundModel::changed);

    // Nothing stored and nothing selected.
    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setOptions({sound(10, QStringLiteral("Bell"))});
    QCOMPARE(spy.count(), 1);

    // Options but no selection still count as something to clear.
    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);

    // A selection that outlived its options is something to clear too.
    model.setOptions({sound(10, QStringLiteral("Bell"))});
    model.select(10);
    model.setOptions({});
    QCOMPARE(model.selectedId(), qint64(10));
    const int before = spy.count();

    model.clear();
    QCOMPARE(spy.count(), before + 1);
    QCOMPARE(model.selectedId(), qint64(0));
}

QTEST_GUILESS_MAIN(NotificationSoundModelTest)
#include "tst_notification_sound_model.moc"
