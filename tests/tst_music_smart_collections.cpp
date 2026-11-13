#include "telegram/music/MusicSmartCollections.h"

#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MusicTrackModel track(const QString& id, int durationSeconds = 300,
                      const QString& albumId = QStringLiteral("album"))
{
    MusicTrackModel model;
    model.id = id;
    model.title = id;
    model.durationSeconds = durationSeconds;
    model.albumId = albumId;
    return model;
}

QString numberedId(int number)
{
    return QStringLiteral("t%1").arg(number, 2, 10, QLatin1Char('0'));
}

const SmartCollection* findCollection(const QVector<SmartCollection>& collections,
                                      const QString& id)
{
    for (const SmartCollection& collection : collections) {
        if (collection.id == id)
            return &collection;
    }
    return nullptr;
}

QVector<QString> ids(const QVector<SmartCollection>& collections, const QString& id)
{
    const SmartCollection* collection = findCollection(collections, id);
    return collection == nullptr ? QVector<QString>{} : collection->trackIds;
}

} // namespace

class MusicSmartCollectionsTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyTracksProduceNoCollections();
    void tracksWithoutIdsProduceNoCollections();

    void recentTakesLastThirtyInGivenOrder();
    void recentWithFewerThanThirtyKeepsAll();
    void recentDedupesKeepingTheNewestOccurrence();
    void recentTrimsIdsAndSkipsEmptyOnes();

    void longIncludesTenMinutesAndLonger();
    void shortIncludesTwoMinutesAndShorter();
    void noAlbumIncludesEmptyAndBlankAlbumIds();

    void collectionsAreOrderedWithStableTitles();
    void eachCollectionDedupesAndKeepsGivenOrder();
    void emptyCollectionsAreOmitted();
    void nowUnixDoesNotChangeTheResult();
};

void MusicSmartCollectionsTest::emptyTracksProduceNoCollections()
{
    QVERIFY(smartCollections(QVector<MusicTrackModel>{}, 0).isEmpty());
}

void MusicSmartCollectionsTest::tracksWithoutIdsProduceNoCollections()
{
    const QVector<MusicTrackModel> tracks{
        track(QString(), 300, QString()),
        track(QStringLiteral("   "), 700, QString()),
    };

    QVERIFY(smartCollections(tracks, 0).isEmpty());
}

void MusicSmartCollectionsTest::recentTakesLastThirtyInGivenOrder()
{
    QVector<MusicTrackModel> tracks;
    tracks.reserve(35);
    for (int number = 0; number < 35; ++number)
        tracks.append(track(numberedId(number)));

    QVector<QString> expected;
    expected.reserve(30);
    for (int number = 5; number < 35; ++number)
        expected.append(numberedId(number));

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 1);
    QCOMPARE(collections.at(0).id, QStringLiteral("recent"));
    QCOMPARE(collections.at(0).title, QStringLiteral("Recent"));
    QCOMPARE(collections.at(0).trackIds, expected);
}

void MusicSmartCollectionsTest::recentWithFewerThanThirtyKeepsAll()
{
    const QVector<MusicTrackModel> tracks{
        track(numberedId(0)),
        track(numberedId(1)),
        track(numberedId(2)),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 1);
    QCOMPARE(collections.at(0).trackIds,
             (QVector<QString>{numberedId(0), numberedId(1), numberedId(2)}));
}

void MusicSmartCollectionsTest::recentDedupesKeepingTheNewestOccurrence()
{
    QVector<MusicTrackModel> tracks;
    tracks.reserve(32);
    for (int number = 0; number <= 30; ++number)
        tracks.append(track(numberedId(number)));
    tracks.append(track(numberedId(0)));

    QVector<QString> expected;
    expected.reserve(30);
    for (int number = 2; number <= 30; ++number)
        expected.append(numberedId(number));
    expected.append(numberedId(0));

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 1);
    QCOMPARE(collections.at(0).trackIds, expected);
}

void MusicSmartCollectionsTest::recentTrimsIdsAndSkipsEmptyOnes()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("  a  ")),
        track(QString()),
        track(QStringLiteral("   ")),
        track(QStringLiteral("b")),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 1);
    QCOMPARE(collections.at(0).trackIds,
             (QVector<QString>{QStringLiteral("a"), QStringLiteral("b")}));
}

void MusicSmartCollectionsTest::longIncludesTenMinutesAndLonger()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("exact"), 600),
        track(QStringLiteral("below"), 599),
        track(QStringLiteral("above"), 601),
        track(QStringLiteral("middle"), 300),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 2);
    QCOMPARE(ids(collections, QStringLiteral("long")),
             (QVector<QString>{QStringLiteral("exact"), QStringLiteral("above")}));
    QVERIFY(findCollection(collections, QStringLiteral("short")) == nullptr);
}

void MusicSmartCollectionsTest::shortIncludesTwoMinutesAndShorter()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("exact"), 120),
        track(QStringLiteral("above"), 121),
        track(QStringLiteral("zero"), 0),
        track(QStringLiteral("negative"), -5),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 2);
    QCOMPARE(ids(collections, QStringLiteral("short")),
             (QVector<QString>{QStringLiteral("exact"), QStringLiteral("zero"),
                               QStringLiteral("negative")}));
    QVERIFY(findCollection(collections, QStringLiteral("long")) == nullptr);
}

void MusicSmartCollectionsTest::noAlbumIncludesEmptyAndBlankAlbumIds()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("empty"), 300, QString()),
        track(QStringLiteral("blank"), 300, QStringLiteral("   ")),
        track(QStringLiteral("named"), 300, QStringLiteral("Album")),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 2);
    QCOMPARE(ids(collections, QStringLiteral("no-album")),
             (QVector<QString>{QStringLiteral("empty"), QStringLiteral("blank")}));
}

void MusicSmartCollectionsTest::collectionsAreOrderedWithStableTitles()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("plain"), 300, QStringLiteral("Album")),
        track(QStringLiteral("long"), 600, QStringLiteral("Album")),
        track(QStringLiteral("short"), 120, QStringLiteral("Album")),
        track(QStringLiteral("no-album"), 300, QString()),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 4);
    QCOMPARE(collections.at(0).id, QStringLiteral("recent"));
    QCOMPARE(collections.at(0).title, QStringLiteral("Recent"));
    QCOMPARE(collections.at(1).id, QStringLiteral("long"));
    QCOMPARE(collections.at(1).title, QStringLiteral("Long tracks"));
    QCOMPARE(collections.at(2).id, QStringLiteral("short"));
    QCOMPARE(collections.at(2).title, QStringLiteral("Short tracks"));
    QCOMPARE(collections.at(3).id, QStringLiteral("no-album"));
    QCOMPARE(collections.at(3).title, QStringLiteral("No album"));

    QCOMPARE(collections.at(0).trackIds,
             (QVector<QString>{QStringLiteral("plain"), QStringLiteral("long"),
                               QStringLiteral("short"), QStringLiteral("no-album")}));
    QCOMPARE(collections.at(1).trackIds, (QVector<QString>{QStringLiteral("long")}));
    QCOMPARE(collections.at(2).trackIds, (QVector<QString>{QStringLiteral("short")}));
    QCOMPARE(collections.at(3).trackIds, (QVector<QString>{QStringLiteral("no-album")}));
}

void MusicSmartCollectionsTest::eachCollectionDedupesAndKeepsGivenOrder()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("b"), 700, QString()),
        track(QStringLiteral("a"), 800, QString()),
        track(QStringLiteral("b"), 900, QString()),
        track(QStringLiteral("c"), 600, QString()),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 3);
    QCOMPARE(ids(collections, QStringLiteral("long")),
             (QVector<QString>{QStringLiteral("b"), QStringLiteral("a"),
                               QStringLiteral("c")}));
    QCOMPARE(ids(collections, QStringLiteral("no-album")),
             (QVector<QString>{QStringLiteral("b"), QStringLiteral("a"),
                               QStringLiteral("c")}));
    QCOMPARE(ids(collections, QStringLiteral("recent")),
             (QVector<QString>{QStringLiteral("a"), QStringLiteral("b"),
                               QStringLiteral("c")}));
}

void MusicSmartCollectionsTest::emptyCollectionsAreOmitted()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("long"), 600, QStringLiteral("Album")),
    };

    const QVector<SmartCollection> collections = smartCollections(tracks, 0);

    QCOMPARE(collections.size(), 2);
    QCOMPARE(collections.at(0).id, QStringLiteral("recent"));
    QCOMPARE(collections.at(1).id, QStringLiteral("long"));
    QVERIFY(findCollection(collections, QStringLiteral("short")) == nullptr);
    QVERIFY(findCollection(collections, QStringLiteral("no-album")) == nullptr);
}

void MusicSmartCollectionsTest::nowUnixDoesNotChangeTheResult()
{
    const QVector<MusicTrackModel> tracks{
        track(QStringLiteral("plain"), 300, QStringLiteral("Album")),
        track(QStringLiteral("long"), 600, QStringLiteral("Album")),
        track(QStringLiteral("short"), 120, QStringLiteral("Album")),
        track(QStringLiteral("no-album"), 300, QString()),
    };

    const QVector<SmartCollection> first = smartCollections(tracks, 0);
    const QVector<SmartCollection> second = smartCollections(tracks, 1700000000);

    QCOMPARE(first.size(), second.size());
    for (qsizetype index = 0; index < first.size(); ++index) {
        QCOMPARE(first.at(index).id, second.at(index).id);
        QCOMPARE(first.at(index).title, second.at(index).title);
        QCOMPARE(first.at(index).trackIds, second.at(index).trackIds);
    }
}

QTEST_GUILESS_MAIN(MusicSmartCollectionsTest)
#include "tst_music_smart_collections.moc"
