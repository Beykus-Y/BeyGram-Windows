#include "telegram/MediaPlaylistModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

PlaylistEntry makeEntry(qint64 messageId, qint64 chatId = 100,
                        const QString& title = {}, int durationSeconds = 0)
{
    PlaylistEntry entry;
    entry.messageId = messageId;
    entry.chatId = chatId;
    entry.title = title;
    entry.durationSeconds = durationSeconds;
    return entry;
}

QVector<qint64> messageIdsOf(const QVector<PlaylistEntry>& entries)
{
    QVector<qint64> ids;
    ids.reserve(entries.size());
    for (const PlaylistEntry& entry : entries)
        ids.append(entry.messageId);
    return ids;
}

QVector<PlaylistEntry> threeTracks()
{
    return {makeEntry(11, 100, QStringLiteral("first"), 30),
            makeEntry(22, 100, QStringLiteral("second"), 45),
            makeEntry(33, 200, QStringLiteral("third"), 60)};
}

} // namespace

class MediaPlaylistModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setEntriesKeepsValidEntriesAndPayload();
    void setEntriesDropsNonPositiveIds();
    void setEntriesDedupesByMessageIdKeepingTheFirstCopy();
    void setEntriesResetsIndexToMinusOne();
    void setEntriesPreservesRepeatAndShuffle();
    void entriesReturnsACopy();
    void countMatchesEntries();
    void currentReturnsDefaultWithoutCurrent();
    void playAtMakesEntryCurrent();
    void playAtIgnoresIndexesOutsideTheQueue();
    void nextAdvancesThroughQueue();
    void nextAtEndWithoutRepeatKeepsIndex();
    void nextWrapsAtEndWithRepeatAll();
    void nextStartsAtFirstWithoutCurrent();
    void previousStepsBack();
    void previousAtStartWithoutRepeatKeepsIndex();
    void previousWrapsAtStartWithRepeatAll();
    void previousStartsAtLastWithoutCurrent();
    void nextAndPreviousOnEmptyQueueFail();
    void repeatAllOnSingleEntryKeepsPlaying();
    void clearResetsQueueAndKeepsFlags();
    void shuffleDoesNotReorderQueue();
    void changedFiresOnlyOnRealChanges();
};

void MediaPlaylistModelTest::startsEmpty()
{
    MediaPlaylistModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.entries().isEmpty());
    QCOMPARE(model.currentIndex(), -1);
    QCOMPARE(model.repeatAll(), false);
    QCOMPARE(model.shuffle(), false);
}

void MediaPlaylistModelTest::setEntriesKeepsValidEntriesAndPayload()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());

    const QVector<PlaylistEntry> entries = model.entries();

    QCOMPARE(model.count(), 3);
    QCOMPARE(messageIdsOf(entries), (QVector<qint64>{11, 22, 33}));
    QCOMPARE(entries.at(0).chatId, qint64(100));
    QCOMPARE(entries.at(0).title, QStringLiteral("first"));
    QCOMPARE(entries.at(0).durationSeconds, 30);
    QCOMPARE(entries.at(2).chatId, qint64(200));
    QCOMPARE(entries.at(2).title, QStringLiteral("third"));
    QCOMPARE(entries.at(2).durationSeconds, 60);
}

void MediaPlaylistModelTest::setEntriesDropsNonPositiveIds()
{
    MediaPlaylistModel model;
    model.setEntries({makeEntry(0, 100), makeEntry(-7, 100), makeEntry(11, 0),
                      makeEntry(22, -3), makeEntry(33, 100)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(messageIdsOf(model.entries()), (QVector<qint64>{33}));
    QCOMPARE(model.entries().constFirst().chatId, qint64(100));
}

void MediaPlaylistModelTest::setEntriesDedupesByMessageIdKeepingTheFirstCopy()
{
    MediaPlaylistModel model;
    model.setEntries({makeEntry(11, 100, QStringLiteral("first")),
                      makeEntry(11, 200, QStringLiteral("duplicate")),
                      makeEntry(22, 100), makeEntry(11, 300)});

    const QVector<PlaylistEntry> entries = model.entries();

    QCOMPARE(model.count(), 2);
    QCOMPARE(messageIdsOf(entries), (QVector<qint64>{11, 22}));
    QCOMPARE(entries.constFirst().title, QStringLiteral("first"));
    QCOMPARE(entries.constFirst().chatId, qint64(100));
}

void MediaPlaylistModelTest::setEntriesResetsIndexToMinusOne()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.playAt(1);
    QCOMPARE(model.currentIndex(), 1);

    model.setEntries(threeTracks());

    QCOMPARE(model.currentIndex(), -1);
    QCOMPARE(model.current().messageId, qint64(0));
    QCOMPARE(model.current().chatId, qint64(0));
    QVERIFY(model.current().title.isEmpty());
    QCOMPARE(model.current().durationSeconds, 0);
    // The queue itself is still there, only playback stopped.
    QCOMPARE(model.count(), 3);
}

void MediaPlaylistModelTest::setEntriesPreservesRepeatAndShuffle()
{
    MediaPlaylistModel model;
    model.setRepeatAll(true);
    model.setShuffle(true);

    model.setEntries(threeTracks());

    QCOMPARE(model.repeatAll(), true);
    QCOMPARE(model.shuffle(), true);

    model.clear();
    model.setEntries({});

    QCOMPARE(model.repeatAll(), true);
    QCOMPARE(model.shuffle(), true);
}

void MediaPlaylistModelTest::entriesReturnsACopy()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());

    QVector<PlaylistEntry> copy = model.entries();
    copy.clear();

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.entries().size(), 3);
}

void MediaPlaylistModelTest::countMatchesEntries()
{
    MediaPlaylistModel model;
    QCOMPARE(model.count(), static_cast<int>(model.entries().size()));

    model.setEntries(threeTracks());
    QCOMPARE(model.count(), static_cast<int>(model.entries().size()));
    QCOMPARE(model.count(), 3);

    model.clear();
    QCOMPARE(model.count(), static_cast<int>(model.entries().size()));
    QCOMPARE(model.count(), 0);
}

void MediaPlaylistModelTest::currentReturnsDefaultWithoutCurrent()
{
    MediaPlaylistModel model;

    const PlaylistEntry empty = model.current();
    QCOMPARE(empty.messageId, qint64(0));
    QCOMPARE(empty.chatId, qint64(0));
    QVERIFY(empty.title.isEmpty());
    QCOMPARE(empty.durationSeconds, 0);

    // An entry is only current after playAt(): filling the queue is not enough.
    model.setEntries(threeTracks());
    QCOMPARE(model.current().messageId, qint64(0));

    model.playAt(0);
    QCOMPARE(model.current().messageId, qint64(11));
    QCOMPARE(model.current().title, QStringLiteral("first"));
}

void MediaPlaylistModelTest::playAtMakesEntryCurrent()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());

    model.playAt(0);
    QCOMPARE(model.currentIndex(), 0);
    QCOMPARE(model.current().messageId, qint64(11));

    model.playAt(2);
    QCOMPARE(model.currentIndex(), 2);
    QCOMPARE(model.current().messageId, qint64(33));
    QCOMPARE(model.current().chatId, qint64(200));
    QCOMPARE(model.current().durationSeconds, 60);

    model.playAt(1);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.current().messageId, qint64(22));
}

void MediaPlaylistModelTest::playAtIgnoresIndexesOutsideTheQueue()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.playAt(1);

    model.playAt(-1);
    QCOMPARE(model.currentIndex(), 1);

    model.playAt(3);
    QCOMPARE(model.currentIndex(), 1);

    model.playAt(100);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.current().messageId, qint64(22));

    // An empty queue has no valid index at all.
    MediaPlaylistModel empty;
    empty.playAt(0);
    QCOMPARE(empty.currentIndex(), -1);
}

void MediaPlaylistModelTest::nextAdvancesThroughQueue()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.playAt(0);

    QCOMPARE(model.next(), true);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.current().messageId, qint64(22));

    QCOMPARE(model.next(), true);
    QCOMPARE(model.currentIndex(), 2);
    QCOMPARE(model.current().messageId, qint64(33));
}

void MediaPlaylistModelTest::nextAtEndWithoutRepeatKeepsIndex()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.playAt(2);

    QCOMPARE(model.next(), false);
    QCOMPARE(model.currentIndex(), 2);
    QCOMPARE(model.current().messageId, qint64(33));
}

void MediaPlaylistModelTest::nextWrapsAtEndWithRepeatAll()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.setRepeatAll(true);
    model.playAt(2);

    QCOMPARE(model.next(), true);
    QCOMPARE(model.currentIndex(), 0);
    QCOMPARE(model.current().messageId, qint64(11));

    QCOMPARE(model.next(), true);
    QCOMPARE(model.currentIndex(), 1);
}

void MediaPlaylistModelTest::nextStartsAtFirstWithoutCurrent()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    QCOMPARE(model.currentIndex(), -1);

    QCOMPARE(model.next(), true);
    QCOMPARE(model.currentIndex(), 0);
    QCOMPARE(model.current().messageId, qint64(11));
}

void MediaPlaylistModelTest::previousStepsBack()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.playAt(2);

    QCOMPARE(model.previous(), true);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.current().messageId, qint64(22));

    QCOMPARE(model.previous(), true);
    QCOMPARE(model.currentIndex(), 0);
    QCOMPARE(model.current().messageId, qint64(11));
}

void MediaPlaylistModelTest::previousAtStartWithoutRepeatKeepsIndex()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.playAt(0);

    QCOMPARE(model.previous(), false);
    QCOMPARE(model.currentIndex(), 0);
    QCOMPARE(model.current().messageId, qint64(11));
}

void MediaPlaylistModelTest::previousWrapsAtStartWithRepeatAll()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.setRepeatAll(true);
    model.playAt(0);

    QCOMPARE(model.previous(), true);
    QCOMPARE(model.currentIndex(), 2);
    QCOMPARE(model.current().messageId, qint64(33));

    QCOMPARE(model.previous(), true);
    QCOMPARE(model.currentIndex(), 1);
}

void MediaPlaylistModelTest::previousStartsAtLastWithoutCurrent()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());

    QCOMPARE(model.previous(), true);
    QCOMPARE(model.currentIndex(), 2);
    QCOMPARE(model.current().messageId, qint64(33));
}

void MediaPlaylistModelTest::nextAndPreviousOnEmptyQueueFail()
{
    MediaPlaylistModel model;
    model.setRepeatAll(true);

    QCOMPARE(model.next(), false);
    QCOMPARE(model.previous(), false);
    QCOMPARE(model.currentIndex(), -1);

    model.setEntries({});
    QCOMPARE(model.next(), false);
    QCOMPARE(model.previous(), false);
    QCOMPARE(model.currentIndex(), -1);
}

void MediaPlaylistModelTest::repeatAllOnSingleEntryKeepsPlaying()
{
    MediaPlaylistModel model;
    model.setEntries({makeEntry(11)});
    model.setRepeatAll(true);
    model.playAt(0);

    QCOMPARE(model.next(), true);
    QCOMPARE(model.currentIndex(), 0);
    QCOMPARE(model.previous(), true);
    QCOMPARE(model.currentIndex(), 0);

    model.setRepeatAll(false);
    QCOMPARE(model.next(), false);
    QCOMPARE(model.previous(), false);
    QCOMPARE(model.currentIndex(), 0);
}

void MediaPlaylistModelTest::clearResetsQueueAndKeepsFlags()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.playAt(1);
    model.setRepeatAll(true);
    model.setShuffle(true);

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.entries().isEmpty());
    QCOMPARE(model.currentIndex(), -1);
    QCOMPARE(model.current().messageId, qint64(0));
    QCOMPARE(model.repeatAll(), true);
    QCOMPARE(model.shuffle(), true);
}

void MediaPlaylistModelTest::shuffleDoesNotReorderQueue()
{
    MediaPlaylistModel model;
    model.setEntries(threeTracks());
    model.setShuffle(true);
    model.playAt(0);

    QCOMPARE(messageIdsOf(model.entries()), (QVector<qint64>{11, 22, 33}));
    QCOMPARE(model.next(), true);
    QCOMPARE(model.currentIndex(), 1);
    QCOMPARE(model.current().messageId, qint64(22));
    QCOMPARE(model.previous(), true);
    QCOMPARE(model.currentIndex(), 0);
}

void MediaPlaylistModelTest::changedFiresOnlyOnRealChanges()
{
    MediaPlaylistModel model;
    QSignalSpy spy(&model, &MediaPlaylistModel::changed);
    QVERIFY(spy.isValid());

    model.setEntries(threeTracks());
    QCOMPARE(spy.count(), 1);

    // Same queue again, including a duplicate that de-duplicates to it.
    model.setEntries(threeTracks());
    QCOMPARE(spy.count(), 1);
    model.setEntries({makeEntry(11, 100, QStringLiteral("first"), 30),
                      makeEntry(11, 100, QStringLiteral("first"), 30),
                      makeEntry(22, 100, QStringLiteral("second"), 45),
                      makeEntry(33, 200, QStringLiteral("third"), 60)});
    QCOMPARE(spy.count(), 1);

    // A payload edit is a real change.
    model.setEntries({makeEntry(11, 100, QStringLiteral("renamed"), 30),
                      makeEntry(22, 100, QStringLiteral("second"), 45),
                      makeEntry(33, 200, QStringLiteral("third"), 60)});
    QCOMPARE(spy.count(), 2);

    model.playAt(0);
    QCOMPARE(spy.count(), 3);
    model.playAt(0);
    QCOMPARE(spy.count(), 3);
    model.playAt(7);
    QCOMPARE(spy.count(), 3);

    model.next();
    QCOMPARE(spy.count(), 4);
    model.next();
    QCOMPARE(spy.count(), 5);
    // At the end without repeat: nothing moved, nothing emitted.
    model.next();
    QCOMPARE(spy.count(), 5);

    model.setRepeatAll(true);
    QCOMPARE(spy.count(), 6);
    model.setRepeatAll(true);
    QCOMPARE(spy.count(), 6);

    model.setShuffle(true);
    QCOMPARE(spy.count(), 7);
    model.setShuffle(true);
    QCOMPARE(spy.count(), 7);

    // Replacing the queue with the same entries still stops playback, so the
    // reset index is a change.
    model.setEntries(model.entries());
    QCOMPARE(spy.count(), 8);
    model.setEntries(model.entries());
    QCOMPARE(spy.count(), 8);

    model.clear();
    QCOMPARE(spy.count(), 9);
    model.clear();
    QCOMPARE(spy.count(), 9);
    model.setEntries({});
    QCOMPARE(spy.count(), 9);
}

QTEST_GUILESS_MAIN(MediaPlaylistModelTest)
#include "tst_media_playlist_model.moc"
