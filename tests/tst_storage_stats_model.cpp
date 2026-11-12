#include "telegram/StorageStatsModel.h"

#include <QSignalSpy>
#include <QtTest>

#include <algorithm>
#include <limits>

using namespace beygram::telegram;

namespace {

StorageRow makeRow(const QString& kind, int fileCount = 0, qint64 bytes = 0)
{
    StorageRow row;
    row.kind = kind;
    row.fileCount = fileCount;
    row.bytes = bytes;
    return row;
}

QVector<QString> kindsOf(const QVector<StorageRow>& rows)
{
    QVector<QString> kinds;
    kinds.reserve(rows.size());
    for (const StorageRow& row : rows)
        kinds.append(row.kind);
    return kinds;
}

QVector<StorageRow> threeRows()
{
    return {makeRow(QStringLiteral("photo"), 12, 40LL * 1024 * 1024),
            makeRow(QStringLiteral("video"), 3, 700LL * 1024 * 1024),
            makeRow(QStringLiteral("document"), 40, 90LL * 1024 * 1024)};
}

} // namespace

class StorageStatsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setRowsKeepsOrderAndPayload();
    void setRowsDropsRowsWithoutAKind();
    void setRowsDedupesKeepingTheFirstCopy();
    void setRowsClampsNegativeCounts();
    void setRowsReplacesThePreviousRows();
    void setRowsEmitsChangedOnceForARealChange();
    void setRowsIsSilentForEquivalentRows();
    void setRowsTreatsCleanedRowsAsEquivalent();
    void rowsReturnsACopy();
    void countMatchesRows();
    void totalFilesSumsFileCounts();
    void totalBytesSumsBytes();
    void totalFilesSaturatesAtIntMax();
    void totalBytesSaturatesAtInt64Max();
    void clampedCountsDoNotReachTheTotals();
    void sortedBySizeIsDescendingByDefault();
    void sortedBySizeAscending();
    void sortedBySizeKeepsStoredOrderForTies();
    void sortedBySizeLeavesStoredRowsAlone();
    void clearDropsRowsAndEmitsChanged();
    void clearIsSilentWhenEmpty();
    void setRowsWithNoRowsClearsExistingRows();
};

void StorageStatsModelTest::startsEmpty()
{
    const StorageStatsModel model;
    QCOMPARE(model.count(), 0);
    QVERIFY(model.rows().isEmpty());
    QCOMPARE(model.totalFiles(), 0);
    QCOMPARE(model.totalBytes(), qint64(0));
    QVERIFY(model.sortedBySize().isEmpty());
}

void StorageStatsModelTest::setRowsKeepsOrderAndPayload()
{
    StorageStatsModel model;
    model.setRows(threeRows());

    QCOMPARE(model.count(), 3);
    QCOMPARE(kindsOf(model.rows()),
             (QVector<QString>{QStringLiteral("photo"), QStringLiteral("video"),
                               QStringLiteral("document")}));

    const StorageRow video = model.rows().at(1);
    QCOMPARE(video.fileCount, 3);
    QCOMPARE(video.bytes, 700LL * 1024 * 1024);
}

void StorageStatsModelTest::setRowsDropsRowsWithoutAKind()
{
    StorageStatsModel model;
    model.setRows({makeRow(QString()), makeRow(QStringLiteral("photo"), 1, 10),
                   makeRow(QStringLiteral()), makeRow(QStringLiteral("audio"), 2, 20)});

    QCOMPARE(model.count(), 2);
    QCOMPARE(kindsOf(model.rows()),
             (QVector<QString>{QStringLiteral("photo"), QStringLiteral("audio")}));
}

void StorageStatsModelTest::setRowsDedupesKeepingTheFirstCopy()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), 1, 10),
                   makeRow(QStringLiteral("video"), 2, 20),
                   makeRow(QStringLiteral("photo"), 99, 999),
                   makeRow(QStringLiteral("video"), 88, 888)});

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.rows().at(0).fileCount, 1);
    QCOMPARE(model.rows().at(0).bytes, qint64(10));
    QCOMPARE(model.rows().at(1).fileCount, 2);
    QCOMPARE(model.rows().at(1).bytes, qint64(20));
}

void StorageStatsModelTest::setRowsClampsNegativeCounts()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), -4, -1024),
                   makeRow(QStringLiteral("video"), 0, 0),
                   makeRow(QStringLiteral("audio"), -1, 5)});

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.rows().at(0).fileCount, 0);
    QCOMPARE(model.rows().at(0).bytes, qint64(0));
    QCOMPARE(model.rows().at(2).fileCount, 0);
    QCOMPARE(model.rows().at(2).bytes, qint64(5));
}

void StorageStatsModelTest::setRowsReplacesThePreviousRows()
{
    StorageStatsModel model;
    model.setRows(threeRows());
    model.setRows({makeRow(QStringLiteral("sticker"), 7, 30)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rows().at(0).kind, QStringLiteral("sticker"));
    QCOMPARE(model.rows().at(0).fileCount, 7);
}

void StorageStatsModelTest::setRowsEmitsChangedOnceForARealChange()
{
    StorageStatsModel model;
    QSignalSpy spy(&model, &StorageStatsModel::changed);

    model.setRows(threeRows());
    QCOMPARE(spy.count(), 1);

    model.setRows({makeRow(QStringLiteral("photo"), 1, 1)});
    QCOMPARE(spy.count(), 2);
}

void StorageStatsModelTest::setRowsIsSilentForEquivalentRows()
{
    StorageStatsModel model;
    model.setRows(threeRows());

    QSignalSpy spy(&model, &StorageStatsModel::changed);
    model.setRows(threeRows());
    QCOMPARE(spy.count(), 0);

    // The stored order is part of the state, so a re-ordered copy is a change.
    QVector<StorageRow> reversed = threeRows();
    std::reverse(reversed.begin(), reversed.end());
    model.setRows(reversed);
    QCOMPARE(spy.count(), 1);
}

void StorageStatsModelTest::setRowsTreatsCleanedRowsAsEquivalent()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), -1, -5),
                   makeRow(QStringLiteral("photo"), 9, 9)});
    QCOMPARE(model.count(), 1);

    QSignalSpy spy(&model, &StorageStatsModel::changed);
    // Same result after cleaning: negative counts clamp to 0 and the repeated
    // kind keeps its first copy.
    model.setRows({makeRow(QStringLiteral("photo"), 0, 0)});
    QCOMPARE(spy.count(), 0);

    model.setRows({makeRow(QStringLiteral("photo"), 0, 0), makeRow(QString()), makeRow(QString())});
    QCOMPARE(spy.count(), 0);
}

void StorageStatsModelTest::rowsReturnsACopy()
{
    StorageStatsModel model;
    model.setRows(threeRows());

    QVector<StorageRow> copy = model.rows();
    copy[0].kind = QStringLiteral("changed");
    copy.clear();

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.rows().at(0).kind, QStringLiteral("photo"));
}

void StorageStatsModelTest::countMatchesRows()
{
    StorageStatsModel model;
    QCOMPARE(model.count(), 0);

    model.setRows({makeRow(QStringLiteral("photo"), 1, 1)});
    QCOMPARE(model.count(), 1);

    model.setRows(threeRows());
    QCOMPARE(model.count(), static_cast<int>(model.rows().size()));
    QCOMPARE(model.count(), 3);
}

void StorageStatsModelTest::totalFilesSumsFileCounts()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), 12, 0),
                   makeRow(QStringLiteral("video"), 3, 0),
                   makeRow(QStringLiteral("document"), 40, 0)});

    QCOMPARE(model.totalFiles(), 55);
}

void StorageStatsModelTest::totalBytesSumsBytes()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), 0, 10),
                   makeRow(QStringLiteral("video"), 0, 25),
                   makeRow(QStringLiteral("document"), 0, 5)});

    QCOMPARE(model.totalBytes(), qint64(40));
}

void StorageStatsModelTest::totalFilesSaturatesAtIntMax()
{
    const int maximum = std::numeric_limits<int>::max();
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), maximum, 0),
                   makeRow(QStringLiteral("video"), 5, 0)});

    QCOMPARE(model.totalFiles(), maximum);

    // The exact maximum is not a saturation, it is the real total.
    model.setRows({makeRow(QStringLiteral("photo"), maximum, 0)});
    QCOMPARE(model.totalFiles(), maximum);
}

void StorageStatsModelTest::totalBytesSaturatesAtInt64Max()
{
    const qint64 maximum = std::numeric_limits<qint64>::max();
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), 0, maximum),
                   makeRow(QStringLiteral("video"), 0, 1)});

    QCOMPARE(model.totalBytes(), maximum);

    model.setRows({makeRow(QStringLiteral("photo"), 0, maximum),
                   makeRow(QStringLiteral("video"), 0, maximum)});
    QCOMPARE(model.totalBytes(), maximum);

    model.setRows({makeRow(QStringLiteral("photo"), 0, maximum)});
    QCOMPARE(model.totalBytes(), maximum);
}

void StorageStatsModelTest::clampedCountsDoNotReachTheTotals()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), -4, -1024),
                   makeRow(QStringLiteral("video"), 2, 8)});

    QCOMPARE(model.totalFiles(), 2);
    QCOMPARE(model.totalBytes(), qint64(8));
}

void StorageStatsModelTest::sortedBySizeIsDescendingByDefault()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), 1, 10),
                   makeRow(QStringLiteral("video"), 1, 900),
                   makeRow(QStringLiteral("document"), 1, 100)});

    QCOMPARE(kindsOf(model.sortedBySize()),
             (QVector<QString>{QStringLiteral("video"), QStringLiteral("document"),
                               QStringLiteral("photo")}));
    QCOMPARE(kindsOf(model.sortedBySize(true)),
             kindsOf(model.sortedBySize()));
}

void StorageStatsModelTest::sortedBySizeAscending()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("photo"), 1, 10),
                   makeRow(QStringLiteral("video"), 1, 900),
                   makeRow(QStringLiteral("document"), 1, 100)});

    QCOMPARE(kindsOf(model.sortedBySize(false)),
             (QVector<QString>{QStringLiteral("photo"), QStringLiteral("document"),
                               QStringLiteral("video")}));
}

void StorageStatsModelTest::sortedBySizeKeepsStoredOrderForTies()
{
    StorageStatsModel model;
    model.setRows({makeRow(QStringLiteral("a"), 1, 10),
                   makeRow(QStringLiteral("b"), 2, 10),
                   makeRow(QStringLiteral("c"), 3, 20),
                   makeRow(QStringLiteral("d"), 4, 10)});

    QCOMPARE(kindsOf(model.sortedBySize()),
             (QVector<QString>{QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b"),
                               QStringLiteral("d")}));
    QCOMPARE(kindsOf(model.sortedBySize(false)),
             (QVector<QString>{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("d"),
                               QStringLiteral("c")}));
}

void StorageStatsModelTest::sortedBySizeLeavesStoredRowsAlone()
{
    StorageStatsModel model;
    model.setRows(threeRows());

    QSignalSpy spy(&model, &StorageStatsModel::changed);
    const QVector<StorageRow> sorted = model.sortedBySize();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(kindsOf(model.rows()),
             (QVector<QString>{QStringLiteral("photo"), QStringLiteral("video"),
                               QStringLiteral("document")}));
    QCOMPARE(sorted.at(0).kind, QStringLiteral("video"));
}

void StorageStatsModelTest::clearDropsRowsAndEmitsChanged()
{
    StorageStatsModel model;
    model.setRows(threeRows());

    QSignalSpy spy(&model, &StorageStatsModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.rows().isEmpty());
    QCOMPARE(model.totalFiles(), 0);
    QCOMPARE(model.totalBytes(), qint64(0));
}

void StorageStatsModelTest::clearIsSilentWhenEmpty()
{
    StorageStatsModel model;
    QSignalSpy spy(&model, &StorageStatsModel::changed);

    model.clear();
    model.clear();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void StorageStatsModelTest::setRowsWithNoRowsClearsExistingRows()
{
    StorageStatsModel model;
    model.setRows(threeRows());

    QSignalSpy spy(&model, &StorageStatsModel::changed);
    model.setRows({});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);

    model.setRows({});
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(StorageStatsModelTest)
#include "tst_storage_stats_model.moc"
