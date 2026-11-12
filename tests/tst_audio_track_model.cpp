#include "telegram/AudioTrackModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

AudioTrack track(qint64 messageId, const QString& title = {},
                 const QString& performer = {}, int durationSeconds = 0,
                 const QString& fileName = {})
{
    return AudioTrack{messageId, title, performer, durationSeconds, fileName};
}

QVector<qint64> idsOf(const QVector<AudioTrack>& tracks)
{
    QVector<qint64> ids;
    ids.reserve(tracks.size());
    for (const AudioTrack& item : tracks)
        ids.append(item.messageId);
    return ids;
}

// "Performer — Title" with a real em dash, kept ASCII in this source file.
QString joined(const QString& performer, const QString& title)
{
    return performer + QLatin1Char(' ') + QChar(0x2014) + QLatin1Char(' ')
        + title;
}

} // namespace

class AudioTrackModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setTracksDropsNonPositiveIds();
    void setTracksDedupesByMessageIdKeepingFirst();
    void setTracksKeepsIncomingOrder();
    void setTracksClampsNegativeDuration();
    void setTracksKeepsZeroDuration();
    void setTracksReplacesPreviousTracks();

    void byPerformerMatchesCaseInsensitively();
    void byPerformerKeepsStoredOrder();
    void byPerformerEmptyMatchesNothing();
    void byPerformerWithoutMatchIsEmpty();
    void byPerformerIgnoresUntaggedTracks();

    void displayTitleJoinsPerformerAndTitle();
    void displayTitleFallsBackToTitleOnly();
    void displayTitleFallsBackToPerformerOnly();
    void displayTitleIsUnknownWhenBothEmpty();
    void displayTitleOutOfRangeIsEmpty();

    void totalDurationSumsTracks();
    void totalDurationOfEmptyModelIsZero();
    void totalDurationSkipsDroppedTracks();

    void clearDropsTracks();
    void clearOnEmptyIsNoOp();

    void emitsChangedOnRealSetTracks();
    void doesNotEmitChangedForIdenticalSetTracks();
    void emitsChangedOnlyOncePerClear();
    void returnsIndependentCopies();
};

void AudioTrackModelTest::startsEmpty()
{
    AudioTrackModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.tracks().isEmpty());
    QCOMPARE(model.totalDuration(), 0);
}

void AudioTrackModelTest::setTracksDropsNonPositiveIds()
{
    AudioTrackModel model;
    model.setTracks({track(0, QStringLiteral("Zero")),
                     track(-4, QStringLiteral("Negative")),
                     track(7, QStringLiteral("Kept"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.tracks().at(0).messageId, 7);
    QCOMPARE(model.tracks().at(0).title, QStringLiteral("Kept"));
}

void AudioTrackModelTest::setTracksDedupesByMessageIdKeepingFirst()
{
    AudioTrackModel model;
    model.setTracks({track(5, QStringLiteral("First"), QStringLiteral("A"), 10),
                     track(5, QStringLiteral("Second"), QStringLiteral("B"), 20)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.tracks().at(0).title, QStringLiteral("First"));
    QCOMPARE(model.tracks().at(0).performer, QStringLiteral("A"));
    QCOMPARE(model.tracks().at(0).durationSeconds, 10);
}

void AudioTrackModelTest::setTracksKeepsIncomingOrder()
{
    AudioTrackModel model;
    model.setTracks({track(9), track(2), track(30)});

    QCOMPARE(idsOf(model.tracks()), (QVector<qint64>{9, 2, 30}));
}

void AudioTrackModelTest::setTracksClampsNegativeDuration()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Song"), {}, -12)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.tracks().at(0).durationSeconds, 0);
    QCOMPARE(model.totalDuration(), 0);
}

void AudioTrackModelTest::setTracksKeepsZeroDuration()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Song"), {}, 0)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.tracks().at(0).durationSeconds, 0);
}

void AudioTrackModelTest::setTracksReplacesPreviousTracks()
{
    AudioTrackModel model;
    model.setTracks({track(1), track(2)});
    model.setTracks({track(3)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.tracks().at(0).messageId, 3);
}

void AudioTrackModelTest::byPerformerMatchesCaseInsensitively()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("One"), QStringLiteral("Daft Punk")),
                     track(2, QStringLiteral("Two"), QStringLiteral("daft punk")),
                     track(3, QStringLiteral("Three"), QStringLiteral("Other"))});

    const QVector<AudioTrack> matches = model.byPerformer(QStringLiteral("DAFT PUNK"));

    QCOMPARE(matches.size(), 2);
    QCOMPARE(idsOf(matches), (QVector<qint64>{1, 2}));
}

void AudioTrackModelTest::byPerformerKeepsStoredOrder()
{
    AudioTrackModel model;
    model.setTracks({track(4, {}, QStringLiteral("Band")),
                     track(2, {}, QStringLiteral("band")),
                     track(9, {}, QStringLiteral("BAND"))});

    QCOMPARE(idsOf(model.byPerformer(QStringLiteral("Band"))),
             (QVector<qint64>{4, 2, 9}));
}

void AudioTrackModelTest::byPerformerEmptyMatchesNothing()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Untagged")),
                     track(2, {}, QStringLiteral("Named"))});

    QVERIFY(model.byPerformer(QString()).isEmpty());
}

void AudioTrackModelTest::byPerformerWithoutMatchIsEmpty()
{
    AudioTrackModel model;
    model.setTracks({track(1, {}, QStringLiteral("Someone"))});

    QVERIFY(model.byPerformer(QStringLiteral("Nobody")).isEmpty());
}

void AudioTrackModelTest::byPerformerIgnoresUntaggedTracks()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("No performer")),
                     track(2, {}, QStringLiteral("Tagged"))});

    const QVector<AudioTrack> matches = model.byPerformer(QStringLiteral("Tagged"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).messageId, 2);
}

void AudioTrackModelTest::displayTitleJoinsPerformerAndTitle()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Get Lucky"),
                          QStringLiteral("Daft Punk"))});

    QCOMPARE(model.displayTitle(0),
             joined(QStringLiteral("Daft Punk"), QStringLiteral("Get Lucky")));
}

void AudioTrackModelTest::displayTitleFallsBackToTitleOnly()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Untitled Artist Song"))});

    QCOMPARE(model.displayTitle(0), QStringLiteral("Untitled Artist Song"));
}

void AudioTrackModelTest::displayTitleFallsBackToPerformerOnly()
{
    AudioTrackModel model;
    model.setTracks({track(1, {}, QStringLiteral("Daft Punk"))});

    QCOMPARE(model.displayTitle(0), QStringLiteral("Daft Punk"));
}

void AudioTrackModelTest::displayTitleIsUnknownWhenBothEmpty()
{
    AudioTrackModel model;
    model.setTracks({track(1)});

    QCOMPARE(model.displayTitle(0), QStringLiteral("Unknown title"));
}

void AudioTrackModelTest::displayTitleOutOfRangeIsEmpty()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Song"))});

    QVERIFY(model.displayTitle(-1).isEmpty());
    QVERIFY(model.displayTitle(1).isEmpty());
    QVERIFY(model.displayTitle(99).isEmpty());
}

void AudioTrackModelTest::totalDurationSumsTracks()
{
    AudioTrackModel model;
    model.setTracks({track(1, {}, {}, 30), track(2, {}, {}, 45),
                     track(3, {}, {}, 15)});

    QCOMPARE(model.totalDuration(), 90);
}

void AudioTrackModelTest::totalDurationOfEmptyModelIsZero()
{
    AudioTrackModel model;

    QCOMPARE(model.totalDuration(), 0);
}

void AudioTrackModelTest::totalDurationSkipsDroppedTracks()
{
    AudioTrackModel model;
    model.setTracks({track(0, {}, {}, 100), track(1, {}, {}, 20),
                     track(1, {}, {}, 70), track(2, {}, {}, -5)});

    QCOMPARE(model.totalDuration(), 20);
}

void AudioTrackModelTest::clearDropsTracks()
{
    AudioTrackModel model;
    model.setTracks({track(1), track(2)});
    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.tracks().isEmpty());
    QCOMPARE(model.totalDuration(), 0);
}

void AudioTrackModelTest::clearOnEmptyIsNoOp()
{
    AudioTrackModel model;
    QSignalSpy spy(&model, &AudioTrackModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void AudioTrackModelTest::emitsChangedOnRealSetTracks()
{
    AudioTrackModel model;
    QSignalSpy spy(&model, &AudioTrackModel::changed);

    model.setTracks({track(1, QStringLiteral("Song"))});

    QCOMPARE(spy.count(), 1);
}

void AudioTrackModelTest::doesNotEmitChangedForIdenticalSetTracks()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"),
                          12, QStringLiteral("song.ogg"))});
    QSignalSpy spy(&model, &AudioTrackModel::changed);

    model.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"),
                          12, QStringLiteral("song.ogg"))});

    QCOMPARE(spy.count(), 0);
}

void AudioTrackModelTest::emitsChangedOnlyOncePerClear()
{
    AudioTrackModel model;
    model.setTracks({track(1), track(2)});
    QSignalSpy spy(&model, &AudioTrackModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 1);
}

void AudioTrackModelTest::returnsIndependentCopies()
{
    AudioTrackModel model;
    model.setTracks({track(1, QStringLiteral("Song"), QStringLiteral("Band"))});

    QVector<AudioTrack> copy = model.tracks();
    copy[0].title = QStringLiteral("Changed");

    QCOMPARE(model.tracks().at(0).title, QStringLiteral("Song"));
}

QTEST_GUILESS_MAIN(AudioTrackModelTest)
#include "tst_audio_track_model.moc"
