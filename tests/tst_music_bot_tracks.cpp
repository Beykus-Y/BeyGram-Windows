#include "telegram/music/MusicBotTracks.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>
#include <QVector>
#include <QtTest>

#include <limits>

using beygram::telegram::BotTrack;
using beygram::telegram::MusicBotCache;
using beygram::telegram::botTrackFromMessage;

namespace {

QJsonObject fileObject(int id, const QString& uniqueId)
{
    QJsonObject remote;
    if (!uniqueId.isEmpty())
        remote.insert(QStringLiteral("unique_id"), uniqueId);

    QJsonObject file{
        {QStringLiteral("id"), id},
    };
    if (!remote.isEmpty())
        file.insert(QStringLiteral("remote"), remote);
    return file;
}

QJsonObject audioObject(const QString& title, const QString& performer, double duration,
                        int fileId, const QString& uniqueId)
{
    QJsonObject audio{
        {QStringLiteral("@type"), QStringLiteral("audio")},
        {QStringLiteral("title"), title},
        {QStringLiteral("performer"), performer},
        {QStringLiteral("duration"), duration},
    };
    if (fileId != 0 || !uniqueId.isEmpty())
        audio.insert(QStringLiteral("audio"), fileObject(fileId, uniqueId));
    return audio;
}

QJsonObject messageWithContent(const QJsonObject& content)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("message")},
        {QStringLiteral("id"), 1},
        {QStringLiteral("content"), content},
    };
}

QJsonObject audioMessage(const QString& title, const QString& performer, double duration,
                         int fileId, const QString& uniqueId)
{
    return messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
        {QStringLiteral("audio"),
         audioObject(title, performer, duration, fileId, uniqueId)},
    });
}

BotTrack makeTrack(const QString& trackId)
{
    BotTrack track;
    track.trackId = trackId;
    track.title = QStringLiteral("Title for %1").arg(trackId);
    track.performer = QStringLiteral("Performer for %1").arg(trackId);
    track.fileUniqueId = QStringLiteral("unique-%1").arg(trackId);
    track.durationSeconds = 200;
    track.fileId = 42;
    track.cachedAt = 0;
    return track;
}

QString idFor(int index)
{
    return QStringLiteral("track-%1").arg(index);
}

} // namespace

class MusicBotTracksTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesAudioMessage();
    void requiresMessageAudioContent();
    void requiresAudioObject();
    void trimsTitleForTrackId();
    void fallsBackToFileUniqueId();
    void fallsBackToFileAliasUniqueId();
    void parsesWithoutAnyIdentity();
    void clampsNegativeDuration();
    void clampsHugeDuration();
    void readsDurationAliases();
    void rejectsInvalidFileIds();
    void readsDirectFileAliases();
    void ignoresMalformedContent();

    void startsEmpty();
    void defaultCapacity();
    void setCapacityClampsToBounds();
    void setCapacityToCurrentValueDoesNotSignal();
    void setCapacityShrinkEvictsOldest();
    void setCapacityIncreaseKeepsEntries();
    void putTrimsTrackId();
    void putIgnoresEmptyTrackId();
    void putStampsCachedAt();
    void putKeepsFields();
    void putReplacesTrackWithSameId();
    void putSameTrackAndNowIsNoOp();
    void putSameTrackWithNewerNowSignals();
    void putAtCapacityEvictsOldest();
    void putNewcomerOlderThanAllChangesNothing();
    void evictionTieBreaksOnTrackId();
    void getReturnsStoredCopy();
    void getOfMissingIdIsDefault();
    void getDoesNotTouchRecency();
    void containsTracksPutAndRemove();
    void removeDropsOnlyThatTrack();
    void removeMissingOrEmptyIsNoOp();
    void removeTrimsTrackId();
    void clearEmptiesStore();
    void clearOnEmptyDoesNotSignal();
    void putAfterRemoveAddsAgain();
    void replacementAtCapacityDoesNotEvict();
    void replacementOlderThanAllBecomesOldest();
    void changedOnlyOnRealChanges();
};

void MusicBotTracksTest::parsesAudioMessage()
{
    const QJsonObject message =
        audioMessage(QStringLiteral("Get Lucky"), QStringLiteral("Daft Punk"), 369, 4242,
                     QStringLiteral("unique-42"));

    const BotTrack track = botTrackFromMessage(message);

    QCOMPARE(track.trackId, QStringLiteral("Get Lucky"));
    QCOMPARE(track.title, QStringLiteral("Get Lucky"));
    QCOMPARE(track.performer, QStringLiteral("Daft Punk"));
    QCOMPARE(track.fileUniqueId, QStringLiteral("unique-42"));
    QCOMPARE(track.durationSeconds, 369);
    QCOMPARE(track.fileId, 4242);
    QCOMPARE(track.cachedAt, qint64(0));
}

void MusicBotTracksTest::requiresMessageAudioContent()
{
    const QJsonObject textMessage = messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageText")},
        {QStringLiteral("text"),
         QJsonObject{{QStringLiteral("text"), QStringLiteral("hello")}}},
    });

    const BotTrack fromText = botTrackFromMessage(textMessage);
    QVERIFY(fromText.trackId.isEmpty());
    QVERIFY(fromText.title.isEmpty());
    QVERIFY(fromText.performer.isEmpty());
    QVERIFY(fromText.fileUniqueId.isEmpty());
    QCOMPARE(fromText.durationSeconds, 0);
    QCOMPARE(fromText.fileId, 0);

    const BotTrack fromMessageWithoutContent =
        botTrackFromMessage(QJsonObject{{QStringLiteral("@type"), QStringLiteral("message")}});
    QVERIFY(fromMessageWithoutContent.trackId.isEmpty());
    QVERIFY(fromMessageWithoutContent.title.isEmpty());

    // A bare audio object is not a message carrying messageAudio content.
    const BotTrack fromAudio =
        botTrackFromMessage(audioObject(QStringLiteral("Song"), QStringLiteral("Band"), 10,
                                        7, QStringLiteral("unique-7")));
    QVERIFY(fromAudio.trackId.isEmpty());
}

void MusicBotTracksTest::requiresAudioObject()
{
    const QJsonObject message = messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
    });

    const BotTrack track = botTrackFromMessage(message);
    QVERIFY(track.trackId.isEmpty());
    QVERIFY(track.title.isEmpty());
    QVERIFY(track.fileUniqueId.isEmpty());
    QCOMPARE(track.durationSeconds, 0);
    QCOMPARE(track.fileId, 0);
}

void MusicBotTracksTest::trimsTitleForTrackId()
{
    const BotTrack track = botTrackFromMessage(audioMessage(
        QStringLiteral("  Get Lucky  "), QStringLiteral("Daft Punk"), 369, 4242,
        QStringLiteral("unique-42")));

    QCOMPARE(track.trackId, QStringLiteral("Get Lucky"));
    // The title stays a display tag; only the identity is trimmed.
    QCOMPARE(track.title, QStringLiteral("  Get Lucky  "));
}

void MusicBotTracksTest::fallsBackToFileUniqueId()
{
    const BotTrack blank = botTrackFromMessage(audioMessage(
        QStringLiteral("   "), QStringLiteral("Band"), 90, 55, QStringLiteral("unique-55")));
    QCOMPARE(blank.trackId, QStringLiteral("unique-55"));

    const BotTrack missing = botTrackFromMessage(audioMessage(
        QString(), QStringLiteral("Band"), 90, 55, QStringLiteral("unique-55")));
    QCOMPARE(missing.trackId, QStringLiteral("unique-55"));
}

void MusicBotTracksTest::fallsBackToFileAliasUniqueId()
{
    QJsonObject audio{
        {QStringLiteral("title"), QString()},
        {QStringLiteral("performer"), QStringLiteral("Band")},
        {QStringLiteral("duration"), 12},
        {QStringLiteral("file"), fileObject(88, QStringLiteral("file-unique"))},
    };

    const BotTrack track = botTrackFromMessage(messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
        {QStringLiteral("audio"), audio},
    }));

    QCOMPARE(track.trackId, QStringLiteral("file-unique"));
    QCOMPARE(track.fileUniqueId, QStringLiteral("file-unique"));
    QCOMPARE(track.fileId, 88);
}

void MusicBotTracksTest::parsesWithoutAnyIdentity()
{
    const BotTrack track = botTrackFromMessage(audioMessage(
        QString(), QStringLiteral("Band"), 12, 0, QString()));

    QVERIFY(track.trackId.isEmpty());
    QVERIFY(track.title.isEmpty());
    QCOMPARE(track.performer, QStringLiteral("Band"));
    QVERIFY(track.fileUniqueId.isEmpty());
    QCOMPARE(track.durationSeconds, 12);
    QCOMPARE(track.fileId, 0);
}

void MusicBotTracksTest::clampsNegativeDuration()
{
    const BotTrack track = botTrackFromMessage(audioMessage(
        QStringLiteral("Song"), QStringLiteral("Band"), -30, 10, QStringLiteral("unique-10")));

    QCOMPARE(track.durationSeconds, 0);
}

void MusicBotTracksTest::clampsHugeDuration()
{
    const BotTrack track = botTrackFromMessage(audioMessage(
        QStringLiteral("Song"), QStringLiteral("Band"), 3000000000, 10,
        QStringLiteral("unique-10")));

    QCOMPARE(track.durationSeconds, std::numeric_limits<int>::max());
}

void MusicBotTracksTest::readsDurationAliases()
{
    const QJsonObject message = messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
        {QStringLiteral("audio"),
         QJsonObject{
             {QStringLiteral("title"), QStringLiteral("Song")},
             {QStringLiteral("duration_seconds"), 77},
         }},
    });

    QCOMPARE(botTrackFromMessage(message).durationSeconds, 77);
}

void MusicBotTracksTest::rejectsInvalidFileIds()
{
    const BotTrack zero = botTrackFromMessage(audioMessage(
        QStringLiteral("Song"), QString(), 10, 0, QStringLiteral("unique-1")));
    QCOMPARE(zero.fileId, 0);

    const BotTrack negative = botTrackFromMessage(audioMessage(
        QStringLiteral("Song"), QString(), 10, -7, QStringLiteral("unique-1")));
    QCOMPARE(negative.fileId, 0);

    QJsonObject audio{
        {QStringLiteral("title"), QStringLiteral("Song")},
        {QStringLiteral("audio"),
         QJsonObject{{QStringLiteral("id"), 3000000000.0}}},
    };
    const BotTrack outOfRange = botTrackFromMessage(messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
        {QStringLiteral("audio"), audio},
    }));
    QCOMPARE(outOfRange.fileId, 0);
}

void MusicBotTracksTest::readsDirectFileAliases()
{
    const QJsonObject message = messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
        {QStringLiteral("audio"),
         QJsonObject{
             {QStringLiteral("title"), QStringLiteral("Song")},
             {QStringLiteral("file_id"), 321},
             {QStringLiteral("file_unique_id"), QStringLiteral("direct-unique")},
         }},
    });

    const BotTrack track = botTrackFromMessage(message);
    QCOMPARE(track.fileId, 321);
    QCOMPARE(track.fileUniqueId, QStringLiteral("direct-unique"));
    QCOMPARE(track.trackId, QStringLiteral("Song"));
}

void MusicBotTracksTest::ignoresMalformedContent()
{
    const BotTrack stringContent = botTrackFromMessage(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("message")},
        {QStringLiteral("content"), QStringLiteral("not an object")},
    });
    QVERIFY(stringContent.trackId.isEmpty());

    const BotTrack nullContent = botTrackFromMessage(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("message")},
        {QStringLiteral("content"), QJsonValue::Null},
    });
    QVERIFY(nullContent.trackId.isEmpty());

    const BotTrack wrongAudio = botTrackFromMessage(messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageAudio")},
        {QStringLiteral("audio"), QStringLiteral("not an object")},
    }));
    QVERIFY(wrongAudio.trackId.isEmpty());
}

void MusicBotTracksTest::startsEmpty()
{
    MusicBotCache cache;

    QCOMPARE(cache.count(), 0);
    QVERIFY(!cache.contains(idFor(1)));
    QVERIFY(!cache.contains(QString()));
    QVERIFY(cache.get(idFor(1)).trackId.isEmpty());
}

void MusicBotTracksTest::defaultCapacity()
{
    MusicBotCache cache;

    QCOMPARE(cache.capacity(), 100);
}

void MusicBotTracksTest::setCapacityClampsToBounds()
{
    MusicBotCache cache;
    QSignalSpy spy(&cache, &MusicBotCache::changed);

    cache.setCapacity(0);
    QCOMPARE(cache.capacity(), 16);

    cache.setCapacity(15);
    QCOMPARE(cache.capacity(), 16);

    cache.setCapacity(-100);
    QCOMPARE(cache.capacity(), 16);

    cache.setCapacity(1001);
    QCOMPARE(cache.capacity(), 1000);

    cache.setCapacity(1000000);
    QCOMPARE(cache.capacity(), 1000);

    cache.setCapacity(250);
    QCOMPARE(cache.capacity(), 250);

    // 100 -> 16, 16 -> 1000, 1000 -> 250; the clamped no-ops did not signal.
    QCOMPARE(spy.count(), 3);
}

void MusicBotTracksTest::setCapacityToCurrentValueDoesNotSignal()
{
    MusicBotCache cache;
    QSignalSpy spy(&cache, &MusicBotCache::changed);

    cache.setCapacity(100);
    QCOMPARE(spy.count(), 0);

    cache.setCapacity(50);
    QCOMPARE(cache.capacity(), 50);
    QCOMPARE(spy.count(), 1);

    cache.setCapacity(50);
    QCOMPARE(spy.count(), 1);

    cache.setCapacity(1); // clamps to 16
    QCOMPARE(cache.capacity(), 16);
    QCOMPARE(spy.count(), 2);

    cache.setCapacity(16);
    QCOMPARE(spy.count(), 2);
}

void MusicBotTracksTest::setCapacityShrinkEvictsOldest()
{
    MusicBotCache cache;
    cache.setCapacity(20);
    for (int i = 1; i <= 20; ++i)
        cache.put(makeTrack(idFor(i)), i * 100);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.setCapacity(16);

    QCOMPARE(cache.capacity(), 16);
    QCOMPARE(cache.count(), 16);
    for (int i = 1; i <= 4; ++i)
        QVERIFY(!cache.contains(idFor(i)));
    for (int i = 5; i <= 20; ++i)
        QVERIFY(cache.contains(idFor(i)));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::setCapacityIncreaseKeepsEntries()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        cache.put(makeTrack(idFor(i)), i);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.setCapacity(1000);

    QCOMPARE(cache.capacity(), 1000);
    QCOMPARE(cache.count(), 16);
    for (int i = 1; i <= 16; ++i)
        QVERIFY(cache.contains(idFor(i)));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::putTrimsTrackId()
{
    MusicBotCache cache;
    cache.put(makeTrack(QStringLiteral("  track-a  ")), 100);

    QCOMPARE(cache.count(), 1);
    QVERIFY(cache.contains(QStringLiteral("track-a")));
    QVERIFY(cache.contains(QStringLiteral("  track-a\n")));
    QCOMPARE(cache.get(QStringLiteral(" track-a ")).trackId,
             QStringLiteral("track-a"));
}

void MusicBotTracksTest::putIgnoresEmptyTrackId()
{
    MusicBotCache cache;
    QSignalSpy spy(&cache, &MusicBotCache::changed);

    cache.put(makeTrack(QString()), 1);
    cache.put(makeTrack(QStringLiteral("   ")), 2);
    cache.put(makeTrack(QStringLiteral("\t\n ")), 3);

    QCOMPARE(cache.count(), 0);
    QCOMPARE(spy.count(), 0);

    cache.put(makeTrack(idFor(1)), 10);
    QCOMPARE(cache.count(), 1);
    QCOMPARE(spy.count(), 1);

    cache.put(makeTrack(QStringLiteral("  ")), 20);
    QCOMPARE(cache.count(), 1);
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::putStampsCachedAt()
{
    MusicBotCache cache;

    BotTrack track = makeTrack(idFor(1));
    track.cachedAt = 999999;
    cache.put(track, 1234);
    QCOMPARE(cache.get(idFor(1)).cachedAt, qint64(1234));

    cache.put(makeTrack(idFor(2)), 0);
    QCOMPARE(cache.get(idFor(2)).cachedAt, qint64(0));
}

void MusicBotTracksTest::putKeepsFields()
{
    MusicBotCache cache;
    BotTrack track;
    track.trackId = QStringLiteral("track-a");
    track.title = QStringLiteral("A song");
    track.performer = QStringLiteral("A band");
    track.fileUniqueId = QStringLiteral("unique-a");
    track.durationSeconds = 245;
    track.fileId = 77;
    track.cachedAt = 5;

    cache.put(track, 1700000000);

    const BotTrack stored = cache.get(track.trackId);
    QCOMPARE(stored.trackId, track.trackId);
    QCOMPARE(stored.title, track.title);
    QCOMPARE(stored.performer, track.performer);
    QCOMPARE(stored.fileUniqueId, track.fileUniqueId);
    QCOMPARE(stored.durationSeconds, 245);
    QCOMPARE(stored.fileId, 77);
    QCOMPARE(stored.cachedAt, qint64(1700000000));
}

void MusicBotTracksTest::putReplacesTrackWithSameId()
{
    MusicBotCache cache;
    cache.put(makeTrack(idFor(1)), 100);

    BotTrack updated = makeTrack(idFor(1));
    updated.title = QStringLiteral("Updated");
    updated.performer = QStringLiteral("New band");
    updated.durationSeconds = 300;
    updated.fileId = 9;
    updated.fileUniqueId = QStringLiteral("unique-updated");
    cache.put(updated, 200);

    QCOMPARE(cache.count(), 1);
    const BotTrack stored = cache.get(idFor(1));
    QCOMPARE(stored.title, QStringLiteral("Updated"));
    QCOMPARE(stored.performer, QStringLiteral("New band"));
    QCOMPARE(stored.durationSeconds, 300);
    QCOMPARE(stored.fileId, 9);
    QCOMPARE(stored.fileUniqueId, QStringLiteral("unique-updated"));
    QCOMPARE(stored.cachedAt, qint64(200));
}

void MusicBotTracksTest::putSameTrackAndNowIsNoOp()
{
    MusicBotCache cache;
    const BotTrack track = makeTrack(idFor(1));
    cache.put(track, 100);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(track, 100);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(cache.count(), 1);
}

void MusicBotTracksTest::putSameTrackWithNewerNowSignals()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        cache.put(makeTrack(idFor(i)), i * 10);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(makeTrack(idFor(1)), 500);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(cache.get(idFor(1)).cachedAt, qint64(500));

    // The refreshed track is now the newest, so track-2 is the oldest left.
    cache.put(makeTrack(idFor(17)), 510);
    QCOMPARE(cache.count(), 16);
    QVERIFY(cache.contains(idFor(1)));
    QVERIFY(!cache.contains(idFor(2)));
    QVERIFY(cache.contains(idFor(17)));
    QCOMPARE(spy.count(), 2);
}

void MusicBotTracksTest::putAtCapacityEvictsOldest()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        cache.put(makeTrack(idFor(i)), i * 10);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(makeTrack(idFor(17)), 170);

    QCOMPARE(cache.count(), 16);
    QVERIFY(!cache.contains(idFor(1)));
    QVERIFY(cache.contains(idFor(2)));
    QVERIFY(cache.contains(idFor(17)));
    QCOMPARE(cache.get(idFor(17)).cachedAt, qint64(170));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::putNewcomerOlderThanAllChangesNothing()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    for (int i = 2; i <= 17; ++i)
        cache.put(makeTrack(idFor(i)), 1000);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(makeTrack(idFor(1)), 500);

    QCOMPARE(cache.count(), 16);
    QVERIFY(!cache.contains(idFor(1)));
    QVERIFY(cache.contains(idFor(2)));
    QVERIFY(cache.contains(idFor(17)));
    QCOMPARE(spy.count(), 0);
}

void MusicBotTracksTest::evictionTieBreaksOnTrackId()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    const QVector<int> insertionOrder{15, 11, 17, 10, 14, 12, 16, 13,
                                      19, 21, 18, 20, 23, 22, 25, 24};
    for (int index : insertionOrder)
        cache.put(makeTrack(idFor(index)), 1000);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(makeTrack(idFor(26)), 1000);

    QCOMPARE(cache.count(), 16);
    QVERIFY(!cache.contains(idFor(10)));
    for (int i = 11; i <= 26; ++i)
        QVERIFY(cache.contains(idFor(i)));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::getReturnsStoredCopy()
{
    MusicBotCache cache;
    const BotTrack track = makeTrack(idFor(1));
    cache.put(track, 100);

    BotTrack copy = cache.get(idFor(1));
    copy.title = QStringLiteral("mutated");

    QCOMPARE(cache.get(idFor(1)).title, track.title);
    QCOMPARE(cache.count(), 1);
}

void MusicBotTracksTest::getOfMissingIdIsDefault()
{
    MusicBotCache cache;
    cache.put(makeTrack(idFor(1)), 100);

    const BotTrack missing = cache.get(idFor(2));
    QVERIFY(missing.trackId.isEmpty());
    QVERIFY(missing.title.isEmpty());
    QVERIFY(missing.performer.isEmpty());
    QVERIFY(missing.fileUniqueId.isEmpty());
    QCOMPARE(missing.durationSeconds, 0);
    QCOMPARE(missing.fileId, 0);
    QCOMPARE(missing.cachedAt, qint64(0));

    QVERIFY(cache.get(QString()).trackId.isEmpty());
    QVERIFY(cache.get(QStringLiteral("   ")).trackId.isEmpty());
}

void MusicBotTracksTest::getDoesNotTouchRecency()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        cache.put(makeTrack(idFor(i)), i * 10);

    QCOMPARE(cache.get(idFor(1)).trackId, idFor(1));
    QCOMPARE(cache.get(idFor(1)).trackId, idFor(1));
    QVERIFY(cache.contains(idFor(1)));

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(makeTrack(idFor(17)), 170);

    // A pure read must not have refreshed track-1, so it is still the oldest.
    QVERIFY(!cache.contains(idFor(1)));
    QVERIFY(cache.contains(idFor(2)));
    QVERIFY(cache.contains(idFor(17)));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::containsTracksPutAndRemove()
{
    MusicBotCache cache;

    QVERIFY(!cache.contains(idFor(3)));
    cache.put(makeTrack(idFor(3)), 100);
    QVERIFY(cache.contains(idFor(3)));
    cache.remove(idFor(3));
    QVERIFY(!cache.contains(idFor(3)));
}

void MusicBotTracksTest::removeDropsOnlyThatTrack()
{
    MusicBotCache cache;
    cache.put(makeTrack(idFor(1)), 10);
    cache.put(makeTrack(idFor(2)), 20);
    cache.put(makeTrack(idFor(3)), 30);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.remove(idFor(2));

    QCOMPARE(cache.count(), 2);
    QVERIFY(cache.contains(idFor(1)));
    QVERIFY(!cache.contains(idFor(2)));
    QVERIFY(cache.contains(idFor(3)));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::removeMissingOrEmptyIsNoOp()
{
    MusicBotCache cache;
    cache.put(makeTrack(idFor(1)), 100);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.remove(idFor(2));
    cache.remove(QString());
    cache.remove(QStringLiteral("   "));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(cache.count(), 1);
}

void MusicBotTracksTest::removeTrimsTrackId()
{
    MusicBotCache cache;
    cache.put(makeTrack(QStringLiteral("  track-a  ")), 100);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.remove(QStringLiteral("track-a"));

    QCOMPARE(cache.count(), 0);
    QVERIFY(!cache.contains(QStringLiteral("track-a")));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::clearEmptiesStore()
{
    MusicBotCache cache;
    cache.put(makeTrack(idFor(1)), 100);
    cache.put(makeTrack(idFor(2)), 200);

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.clear();

    QCOMPARE(cache.count(), 0);
    QVERIFY(!cache.contains(idFor(1)));
    QVERIFY(!cache.contains(idFor(2)));
    QVERIFY(cache.get(idFor(1)).trackId.isEmpty());
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::clearOnEmptyDoesNotSignal()
{
    MusicBotCache cache;
    QSignalSpy spy(&cache, &MusicBotCache::changed);

    cache.clear();
    cache.clear();

    QCOMPARE(spy.count(), 0);
}

void MusicBotTracksTest::putAfterRemoveAddsAgain()
{
    MusicBotCache cache;
    cache.put(makeTrack(idFor(1)), 100);
    cache.remove(idFor(1));

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(makeTrack(idFor(1)), 200);

    QCOMPARE(cache.count(), 1);
    QCOMPARE(cache.get(idFor(1)).cachedAt, qint64(200));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::replacementAtCapacityDoesNotEvict()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        cache.put(makeTrack(idFor(i)), i * 10);

    BotTrack updated = makeTrack(idFor(1));
    updated.title = QStringLiteral("Updated");

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(updated, 999);

    QCOMPARE(cache.count(), 16);
    for (int i = 1; i <= 16; ++i)
        QVERIFY(cache.contains(idFor(i)));
    QCOMPARE(cache.get(idFor(1)).title, QStringLiteral("Updated"));
    QCOMPARE(cache.get(idFor(1)).cachedAt, qint64(999));
    QCOMPARE(spy.count(), 1);
}

void MusicBotTracksTest::replacementOlderThanAllBecomesOldest()
{
    MusicBotCache cache;
    cache.setCapacity(16);
    for (int i = 1; i <= 16; ++i)
        cache.put(makeTrack(idFor(i)), i * 1000);

    BotTrack stale = makeTrack(idFor(1));
    stale.title = QStringLiteral("Stale");

    QSignalSpy spy(&cache, &MusicBotCache::changed);
    cache.put(stale, 0);

    // Replacing an existing entry never changes the count; it just becomes the
    // oldest row and is the next victim.
    QCOMPARE(cache.count(), 16);
    QVERIFY(cache.contains(idFor(1)));
    QCOMPARE(cache.get(idFor(1)).title, QStringLiteral("Stale"));
    QCOMPARE(spy.count(), 1);

    cache.put(makeTrack(idFor(17)), 500);
    QCOMPARE(cache.count(), 16);
    QVERIFY(!cache.contains(idFor(1)));
    QVERIFY(cache.contains(idFor(2)));
    QVERIFY(cache.contains(idFor(17)));
    QCOMPARE(spy.count(), 2);
}

void MusicBotTracksTest::changedOnlyOnRealChanges()
{
    MusicBotCache cache;
    QSignalSpy spy(&cache, &MusicBotCache::changed);

    cache.put(makeTrack(idFor(1)), 100);          // new entry
    cache.put(makeTrack(idFor(1)), 100);          // identical
    cache.put(makeTrack(QStringLiteral("  ")), 100); // empty id
    cache.put(makeTrack(idFor(2)), 200);          // new entry
    cache.put(makeTrack(idFor(1)), 300);          // recency moved
    cache.remove(idFor(99));                      // absent
    cache.remove(idFor(2));                       // present
    cache.clear();                                // one left
    cache.clear();                                // already empty

    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(MusicBotTracksTest)
#include "tst_music_bot_tracks.moc"
