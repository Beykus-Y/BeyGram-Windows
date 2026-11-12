#include "telegram/DataUsageModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

UsageRow row(const QString& category, qint64 sent, qint64 received)
{
    return UsageRow{category, sent, received};
}

QStringList categoriesOf(const QVector<UsageRow>& rows)
{
    QStringList categories;
    categories.reserve(rows.size());
    for (const UsageRow& entry : rows)
        categories.append(entry.category);
    return categories;
}

} // namespace

class DataUsageModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void keepsRowOrder();
    void dropsEmptyCategories();
    void dedupesCategoriesKeepingFirst();
    void dedupeIsCaseSensitive();
    void clampsNegativeSent();
    void clampsNegativeReceived();
    void replacesPreviousRows();
    void countMatchesRows();
    void totalBytesSumsBothDirections();
    void totalBytesIgnoresDroppedRows();
    void bytesForReturnsCategoryTotal();
    void bytesForUnknownCategoryIsZero();
    void resetDropsEverything();
    void resetOnEmptyModelIsNoOp();
    void emitsChangedOnRealChange();
    void silentWhenRowsAreUnchanged();
    void silentWhenOnlyDroppedRowsArrive();
    void silentWhenNegativeClampYieldsSameRows();
    void returnsIndependentCopies();
    void formatBytesBelowOneKibibyte();
    void formatBytesUsesKibibytes();
    void formatBytesUsesMebibytes();
    void formatBytesUsesGibibytes();
    void formatBytesStopsAtGigabytes();
    void formatBytesRoundsToOneDecimal();
    void formatBytesHandlesNegative();
    void formatBytesZero();
};

void DataUsageModelTest::startsEmpty()
{
    DataUsageModel model;
    QVERIFY(model.rows().isEmpty());
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.totalBytes(), qint64(0));
}

void DataUsageModelTest::keepsRowOrder()
{
    DataUsageModel model;
    model.setRows({
        row(QStringLiteral("Photos"), 100, 200),
        row(QStringLiteral("Videos"), 300, 400),
        row(QStringLiteral("Files"), 500, 600),
    });

    QCOMPARE(model.count(), 3);
    QCOMPARE(categoriesOf(model.rows()),
             QStringList({QStringLiteral("Photos"), QStringLiteral("Videos"),
                          QStringLiteral("Files")}));
    QCOMPARE(model.rows().at(0).bytesSent, qint64(100));
    QCOMPARE(model.rows().at(2).bytesReceived, qint64(600));
}

void DataUsageModelTest::dropsEmptyCategories()
{
    DataUsageModel model;
    model.setRows({
        row(QString(), 1, 2),
        row(QStringLiteral("Photos"), 3, 4),
        row(QString(), 5, 6),
        row(QStringLiteral("Videos"), 7, 8),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(categoriesOf(model.rows()),
             QStringList({QStringLiteral("Photos"), QStringLiteral("Videos")}));
    QCOMPARE(model.totalBytes(), qint64(22));
}

void DataUsageModelTest::dedupesCategoriesKeepingFirst()
{
    DataUsageModel model;
    model.setRows({
        row(QStringLiteral("Photos"), 10, 20),
        row(QStringLiteral("Videos"), 30, 40),
        row(QStringLiteral("Photos"), 99, 99),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(categoriesOf(model.rows()),
             QStringList({QStringLiteral("Photos"), QStringLiteral("Videos")}));
    QCOMPARE(model.rows().at(0).bytesSent, qint64(10));
    QCOMPARE(model.rows().at(0).bytesReceived, qint64(20));
}

void DataUsageModelTest::dedupeIsCaseSensitive()
{
    DataUsageModel model;
    model.setRows({
        row(QStringLiteral("Photos"), 1, 1),
        row(QStringLiteral("photos"), 2, 2),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(categoriesOf(model.rows()),
             QStringList({QStringLiteral("Photos"), QStringLiteral("photos")}));
}

void DataUsageModelTest::clampsNegativeSent()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Photos"), -500, 200)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rows().at(0).bytesSent, qint64(0));
    QCOMPARE(model.rows().at(0).bytesReceived, qint64(200));
    QCOMPARE(model.totalBytes(), qint64(200));
}

void DataUsageModelTest::clampsNegativeReceived()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Videos"), 300, -700)});

    QCOMPARE(model.rows().at(0).bytesReceived, qint64(0));
    QCOMPARE(model.rows().at(0).bytesSent, qint64(300));
    QCOMPARE(model.totalBytes(), qint64(300));
}

void DataUsageModelTest::replacesPreviousRows()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Photos"), 1, 2),
                   row(QStringLiteral("Videos"), 3, 4)});
    model.setRows({row(QStringLiteral("Files"), 5, 6)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rows().at(0).category, QStringLiteral("Files"));
    QCOMPARE(model.totalBytes(), qint64(11));
}

void DataUsageModelTest::countMatchesRows()
{
    DataUsageModel model;
    QCOMPARE(model.count(), 0);

    model.setRows({row(QStringLiteral("Photos"), 1, 1)});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.count(), static_cast<int>(model.rows().size()));

    model.reset();
    QCOMPARE(model.count(), 0);
}

void DataUsageModelTest::totalBytesSumsBothDirections()
{
    DataUsageModel model;
    model.setRows({
        row(QStringLiteral("Photos"), 1000, 2000),
        row(QStringLiteral("Videos"), 3000, 4000),
    });

    QCOMPARE(model.totalBytes(), qint64(10000));
}

void DataUsageModelTest::totalBytesIgnoresDroppedRows()
{
    DataUsageModel model;
    model.setRows({
        row(QString(), 5000, 5000),
        row(QStringLiteral("Photos"), 10, 20),
        row(QStringLiteral("Photos"), 9999, 9999),
        row(QStringLiteral("Videos"), -1, 30),
    });

    QCOMPARE(model.totalBytes(), qint64(60));
}

void DataUsageModelTest::bytesForReturnsCategoryTotal()
{
    DataUsageModel model;
    model.setRows({
        row(QStringLiteral("Photos"), 100, 250),
        row(QStringLiteral("Videos"), 300, 400),
    });

    QCOMPARE(model.bytesFor(QStringLiteral("Photos")), qint64(350));
    QCOMPARE(model.bytesFor(QStringLiteral("Videos")), qint64(700));
}

void DataUsageModelTest::bytesForUnknownCategoryIsZero()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Photos"), 100, 250)});

    QCOMPARE(model.bytesFor(QStringLiteral("Music")), qint64(0));
    QCOMPARE(model.bytesFor(QString()), qint64(0));

    DataUsageModel empty;
    QCOMPARE(empty.bytesFor(QStringLiteral("Photos")), qint64(0));
}

void DataUsageModelTest::resetDropsEverything()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Photos"), 1, 2),
                   row(QStringLiteral("Videos"), 3, 4)});

    model.reset();

    QVERIFY(model.rows().isEmpty());
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.totalBytes(), qint64(0));
    QCOMPARE(model.bytesFor(QStringLiteral("Photos")), qint64(0));
}

void DataUsageModelTest::resetOnEmptyModelIsNoOp()
{
    DataUsageModel model;
    QSignalSpy spy(&model, &DataUsageModel::changed);

    model.reset();
    QCOMPARE(spy.count(), 0);

    // Rows that were filtered away entirely leave nothing to reset.
    model.setRows({row(QString(), 1, 2)});
    QCOMPARE(spy.count(), 0);

    model.reset();
    QCOMPARE(spy.count(), 0);
}

void DataUsageModelTest::emitsChangedOnRealChange()
{
    DataUsageModel model;
    QSignalSpy spy(&model, &DataUsageModel::changed);

    model.setRows({row(QStringLiteral("Photos"), 1, 2)});
    QCOMPARE(spy.count(), 1);

    // A different byte count is a real change even though the category matches.
    model.setRows({row(QStringLiteral("Photos"), 1, 3)});
    QCOMPARE(spy.count(), 2);

    // So is replacing the rows with an empty list.
    model.setRows({});
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.rows().isEmpty());

    model.setRows({row(QStringLiteral("Files"), 4, 5)});
    QCOMPARE(spy.count(), 4);

    model.reset();
    QCOMPARE(spy.count(), 5);
}

void DataUsageModelTest::silentWhenRowsAreUnchanged()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Photos"), 1, 2),
                   row(QStringLiteral("Videos"), 3, 4)});
    QSignalSpy spy(&model, &DataUsageModel::changed);

    model.setRows({row(QStringLiteral("Photos"), 1, 2),
                   row(QStringLiteral("Videos"), 3, 4)});
    QCOMPARE(spy.count(), 0);

    // Order is part of the stored value, so reordering does count as a change.
    model.setRows({row(QStringLiteral("Videos"), 3, 4),
                   row(QStringLiteral("Photos"), 1, 2)});
    QCOMPARE(spy.count(), 1);
}

void DataUsageModelTest::silentWhenOnlyDroppedRowsArrive()
{
    DataUsageModel model;
    QSignalSpy spy(&model, &DataUsageModel::changed);

    model.setRows({row(QString(), 1, 2), row(QString(), 3, 4)});
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.rows().isEmpty());

    model.setRows({row(QStringLiteral("Photos"), 1, 2)});
    QCOMPARE(spy.count(), 1);

    // A duplicate that loses to the stored first row changes nothing.
    model.setRows({row(QStringLiteral("Photos"), 1, 2),
                   row(QStringLiteral("Photos"), 7, 8)});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.totalBytes(), qint64(3));
}

void DataUsageModelTest::silentWhenNegativeClampYieldsSameRows()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Photos"), 0, 5)});
    QSignalSpy spy(&model, &DataUsageModel::changed);

    model.setRows({row(QStringLiteral("Photos"), -40, 5)});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.rows().at(0).bytesSent, qint64(0));
}

void DataUsageModelTest::returnsIndependentCopies()
{
    DataUsageModel model;
    model.setRows({row(QStringLiteral("Photos"), 1, 2)});

    QVector<UsageRow> rows = model.rows();
    rows[0].category = QStringLiteral("mutated");
    rows[0].bytesSent = 999;
    rows.append(row(QStringLiteral("extra"), 9, 9));

    QCOMPARE(model.rows().size(), 1);
    QCOMPARE(model.rows().at(0).category, QStringLiteral("Photos"));
    QCOMPARE(model.rows().at(0).bytesSent, qint64(1));
    QCOMPARE(model.totalBytes(), qint64(3));
}

void DataUsageModelTest::formatBytesBelowOneKibibyte()
{
    QCOMPARE(DataUsageModel::formatBytes(0), QStringLiteral("0 B"));
    QCOMPARE(DataUsageModel::formatBytes(1), QStringLiteral("1 B"));
    QCOMPARE(DataUsageModel::formatBytes(512), QStringLiteral("512 B"));
    QCOMPARE(DataUsageModel::formatBytes(1023), QStringLiteral("1023 B"));
}

void DataUsageModelTest::formatBytesUsesKibibytes()
{
    QCOMPARE(DataUsageModel::formatBytes(1024), QStringLiteral("1.0 KB"));
    QCOMPARE(DataUsageModel::formatBytes(1536), QStringLiteral("1.5 KB"));
}

void DataUsageModelTest::formatBytesUsesMebibytes()
{
    QCOMPARE(DataUsageModel::formatBytes(1024 * 1024), QStringLiteral("1.0 MB"));
    QCOMPARE(DataUsageModel::formatBytes(1024 * 1024 * 3 / 2), QStringLiteral("1.5 MB"));
}

void DataUsageModelTest::formatBytesUsesGibibytes()
{
    constexpr qint64 gibibyte = 1024LL * 1024LL * 1024LL;

    QCOMPARE(DataUsageModel::formatBytes(gibibyte), QStringLiteral("1.0 GB"));
    QCOMPARE(DataUsageModel::formatBytes(gibibyte * 5 / 2), QStringLiteral("2.5 GB"));
}

void DataUsageModelTest::formatBytesStopsAtGigabytes()
{
    constexpr qint64 gibibyte = 1024LL * 1024LL * 1024LL;

    QCOMPARE(DataUsageModel::formatBytes(gibibyte * 2048), QStringLiteral("2048.0 GB"));
}

void DataUsageModelTest::formatBytesRoundsToOneDecimal()
{
    // 1 KiB + 1 B is 1.0009 KiB, which still renders as a single decimal.
    QCOMPARE(DataUsageModel::formatBytes(1025), QStringLiteral("1.0 KB"));
    // 10 KiB + 512 B is 10.5 KiB exactly.
    QCOMPARE(DataUsageModel::formatBytes(1024 * 10 + 512), QStringLiteral("10.5 KB"));
}

void DataUsageModelTest::formatBytesHandlesNegative()
{
    QCOMPARE(DataUsageModel::formatBytes(-1), QStringLiteral("0 B"));
    QCOMPARE(DataUsageModel::formatBytes(-1024 * 1024), QStringLiteral("0 B"));
}

void DataUsageModelTest::formatBytesZero()
{
    QCOMPARE(DataUsageModel::formatBytes(0), QStringLiteral("0 B"));
}

QTEST_GUILESS_MAIN(DataUsageModelTest)
#include "tst_data_usage_model.moc"
