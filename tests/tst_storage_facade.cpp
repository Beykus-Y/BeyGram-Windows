#include "ui/StorageFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

/// The row separator: space, middle dot, space.
QString dot()
{
    return QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ");
}

/// A three-kind breakdown: 12 photos worth 1.0 KB, 3 videos worth 2.0 KB and one
/// audio file worth 512 B. The total is 3.5 KB, which is also what ticking all
/// three selects.
void loadSampleStorage(StorageFacade& facade)
{
    facade.setStorage({QStringLiteral("photo"), QStringLiteral("video"), QStringLiteral("audio")},
                      {QStringLiteral("12"), QStringLiteral("3"), QStringLiteral("1")},
                      {QStringLiteral("1024"), QStringLiteral("2048"), QStringLiteral("512")});
}

} // namespace

class StorageFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void storageKindsKeepOrder();
    void storageSizesAreFormatted();
    void storageDropsBlankAndDuplicateKinds();
    void storageShortListsReadAsZero();
    void storageNegativeSizesReadAsZero();
    void totalTextSumsRows();
    void totalTextIgnoresDroppedRows();
    void selectionSetsSelectedText();
    void selectionIgnoresUnknownKindsAndSorts();
    void selectionSurvivesStorageRefresh();
    void selectionClearedWhenKindDisappears();
    void selectedTextIsZeroWithoutSelection();
    void canCleanRequiresSelectionWorthBytes();
    void cleanWithoutSelectionEmitsNothing();
    void cleanEmitsSelectionAndSetsRunning();
    void cleanIsIgnoredWhileRunning();
    void setRunningTogglesCanClean();
    void setRunningIsSilentWhenUnchanged();
    void autoDownloadDefaultsOn();
    void autoDownloadTogglesAndNotifies();
    void autoDownloadSameValueIsSilent();
    void usageFormatsSentAndReceived();
    void usageDropsBlankAndDuplicateCategories();
    void usageShortListsReadAsZero();
    void usageClearsWhenEmpty();
    void changedEmittedOncePerStorageUpdate();
    void changedCoversEveryUpdate();
};

void StorageFacadeTest::startsEmpty()
{
    StorageFacade facade;

    QVERIFY(facade.kinds().isEmpty());
    QVERIFY(facade.sizes().isEmpty());
    QCOMPARE(facade.totalText(), QStringLiteral("0 B"));
    QVERIFY(facade.selectedKinds().isEmpty());
    QCOMPARE(facade.selectedText(), QStringLiteral("0 B"));
    QVERIFY(!facade.canClean());
    QVERIFY(facade.autoDownload());
    QVERIFY(facade.usage().isEmpty());
}

void StorageFacadeTest::storageKindsKeepOrder()
{
    StorageFacade facade;
    loadSampleStorage(facade);

    const QStringList expected{
        QStringLiteral("photo"),
        QStringLiteral("video"),
        QStringLiteral("audio"),
    };
    QCOMPARE(facade.kinds(), expected);
}

void StorageFacadeTest::storageSizesAreFormatted()
{
    StorageFacade facade;
    loadSampleStorage(facade);

    const QStringList expected{
        QStringLiteral("1.0 KB"),
        QStringLiteral("2.0 KB"),
        QStringLiteral("512 B"),
    };
    QCOMPARE(facade.sizes(), expected);
}

void StorageFacadeTest::storageDropsBlankAndDuplicateKinds()
{
    StorageFacade facade;

    facade.setStorage({QStringLiteral("photo"), QString(), QStringLiteral("photo"),
                       QStringLiteral("video")},
                      {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("9"),
                       QStringLiteral("3")},
                      {QStringLiteral("1024"), QStringLiteral("2048"), QStringLiteral("4096"),
                       QStringLiteral("512")});

    const QStringList expectedKinds{QStringLiteral("photo"), QStringLiteral("video")};
    const QStringList expectedSizes{QStringLiteral("1.0 KB"), QStringLiteral("512 B")};
    QCOMPARE(facade.kinds(), expectedKinds);
    QCOMPARE(facade.sizes(), expectedSizes);

    // The surviving photo row is the first one, not the duplicate's 4096 B.
    facade.setSelection({QStringLiteral("photo")});
    QCOMPARE(facade.selectedText(), QStringLiteral("1.0 KB"));
}

void StorageFacadeTest::storageShortListsReadAsZero()
{
    StorageFacade facade;

    // One count and one size for two kinds, then a size that is not a number.
    facade.setStorage({QStringLiteral("photo"), QStringLiteral("video")},
                      {QStringLiteral("12")}, {QStringLiteral("1024")});
    QCOMPARE(facade.sizes(), QStringList({QStringLiteral("1.0 KB"), QStringLiteral("0 B")}));

    facade.setStorage({QStringLiteral("photo")}, {QStringLiteral("1")},
                      {QStringLiteral("not a size")});
    QCOMPARE(facade.sizes(), QStringList({QStringLiteral("0 B")}));
    QCOMPARE(facade.totalText(), QStringLiteral("0 B"));
}

void StorageFacadeTest::storageNegativeSizesReadAsZero()
{
    StorageFacade facade;

    facade.setStorage({QStringLiteral("photo")}, {QStringLiteral("-4")},
                      {QStringLiteral("-1024")});

    QCOMPARE(facade.sizes(), QStringList({QStringLiteral("0 B")}));
    QCOMPARE(facade.totalText(), QStringLiteral("0 B"));

    // A row that frees nothing cannot be cleaned either.
    facade.setSelection({QStringLiteral("photo")});
    QVERIFY(!facade.canClean());
}

void StorageFacadeTest::totalTextSumsRows()
{
    StorageFacade facade;
    loadSampleStorage(facade);

    QCOMPARE(facade.totalText(), QStringLiteral("3.5 KB"));
}

void StorageFacadeTest::totalTextIgnoresDroppedRows()
{
    StorageFacade facade;

    facade.setStorage({QString(), QStringLiteral("photo")}, {QStringLiteral("1"),
                                                             QStringLiteral("1")},
                      {QStringLiteral("999999"), QStringLiteral("1024")});

    QCOMPARE(facade.totalText(), QStringLiteral("1.0 KB"));
}

void StorageFacadeTest::selectionSetsSelectedText()
{
    StorageFacade facade;
    loadSampleStorage(facade);

    facade.setSelection({QStringLiteral("photo")});
    QCOMPARE(facade.selectedKinds(), QStringList({QStringLiteral("photo")}));
    QCOMPARE(facade.selectedText(), QStringLiteral("1.0 KB"));

    facade.setSelection({QStringLiteral("photo"), QStringLiteral("video")});
    QCOMPARE(facade.selectedText(), QStringLiteral("3.0 KB"));

    facade.setSelection({QStringLiteral("photo"), QStringLiteral("video"),
                         QStringLiteral("audio")});
    QCOMPARE(facade.selectedText(), QStringLiteral("3.5 KB"));
}

void StorageFacadeTest::selectionIgnoresUnknownKindsAndSorts()
{
    StorageFacade facade;
    loadSampleStorage(facade);

    facade.setSelection({QStringLiteral("video"), QStringLiteral("nope"),
                         QStringLiteral("photo"), QStringLiteral("video")});

    const QStringList expected{QStringLiteral("photo"), QStringLiteral("video")};
    QCOMPARE(facade.selectedKinds(), expected);
    QCOMPARE(facade.selectedText(), QStringLiteral("3.0 KB"));
}

void StorageFacadeTest::selectionSurvivesStorageRefresh()
{
    StorageFacade facade;
    loadSampleStorage(facade);
    facade.setSelection({QStringLiteral("video")});

    // The same kinds with new sizes: the tick must not be lost, and the text
    // must follow the refreshed size.
    facade.setStorage({QStringLiteral("photo"), QStringLiteral("video"),
                       QStringLiteral("audio")},
                      {QStringLiteral("12"), QStringLiteral("3"), QStringLiteral("1")},
                      {QStringLiteral("1024"), QStringLiteral("4096"), QStringLiteral("512")});

    QCOMPARE(facade.selectedKinds(), QStringList({QStringLiteral("video")}));
    QCOMPARE(facade.selectedText(), QStringLiteral("4.0 KB"));
    QVERIFY(facade.canClean());
}

void StorageFacadeTest::selectionClearedWhenKindDisappears()
{
    StorageFacade facade;
    loadSampleStorage(facade);
    facade.setSelection({QStringLiteral("photo"), QStringLiteral("video")});

    // Only audio is left, so neither ticked kind can stay selected.
    facade.setStorage({QStringLiteral("audio")}, {QStringLiteral("1")},
                      {QStringLiteral("512")});

    QVERIFY(facade.selectedKinds().isEmpty());
    QCOMPARE(facade.selectedText(), QStringLiteral("0 B"));
    QVERIFY(!facade.canClean());
}

void StorageFacadeTest::selectedTextIsZeroWithoutSelection()
{
    StorageFacade facade;
    loadSampleStorage(facade);

    QCOMPARE(facade.selectedText(), QStringLiteral("0 B"));

    facade.setSelection({QStringLiteral("photo")});
    facade.setSelection({});
    QCOMPARE(facade.selectedText(), QStringLiteral("0 B"));
}

void StorageFacadeTest::canCleanRequiresSelectionWorthBytes()
{
    StorageFacade facade;
    facade.setStorage({QStringLiteral("photo"), QStringLiteral("video")},
                      {QStringLiteral("1"), QStringLiteral("1")},
                      {QStringLiteral("0"), QStringLiteral("2048")});

    QVERIFY(!facade.canClean());

    // A ticked kind worth nothing is not a cleanup.
    facade.setSelection({QStringLiteral("photo")});
    QVERIFY(!facade.canClean());

    facade.setSelection({QStringLiteral("video")});
    QVERIFY(facade.canClean());

    facade.setSelection({QStringLiteral("photo"), QStringLiteral("video")});
    QVERIFY(facade.canClean());

    facade.setSelection({});
    QVERIFY(!facade.canClean());
}

void StorageFacadeTest::cleanWithoutSelectionEmitsNothing()
{
    StorageFacade facade;
    loadSampleStorage(facade);

    QSignalSpy spy(&facade, &StorageFacade::cleanRequested);
    QVERIFY(!facade.clean());
    QCOMPARE(spy.count(), 0);
}

void StorageFacadeTest::cleanEmitsSelectionAndSetsRunning()
{
    StorageFacade facade;
    loadSampleStorage(facade);
    facade.setSelection({QStringLiteral("video"), QStringLiteral("photo")});

    QSignalSpy spy(&facade, &StorageFacade::cleanRequested);
    QVERIFY(facade.clean());

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> arguments = spy.takeFirst();
    const QStringList expected{QStringLiteral("photo"), QStringLiteral("video")};
    QCOMPARE(arguments.at(0).toStringList(), expected);

    // The cleanup is in flight, so the button is disabled until released.
    QVERIFY(!facade.canClean());
    facade.setRunning(false);
    QVERIFY(facade.canClean());
}

void StorageFacadeTest::cleanIsIgnoredWhileRunning()
{
    StorageFacade facade;
    loadSampleStorage(facade);
    facade.setSelection({QStringLiteral("video")});
    facade.setRunning(true);

    QSignalSpy spy(&facade, &StorageFacade::cleanRequested);
    QVERIFY(!facade.clean());
    QCOMPARE(spy.count(), 0);
}

void StorageFacadeTest::setRunningTogglesCanClean()
{
    StorageFacade facade;
    loadSampleStorage(facade);
    facade.setSelection({QStringLiteral("video")});

    QVERIFY(facade.canClean());
    facade.setRunning(true);
    QVERIFY(!facade.canClean());
    facade.setRunning(false);
    QVERIFY(facade.canClean());
}

void StorageFacadeTest::setRunningIsSilentWhenUnchanged()
{
    StorageFacade facade;
    QSignalSpy spy(&facade, &StorageFacade::changed);

    facade.setRunning(false);
    QCOMPARE(spy.count(), 0);

    facade.setRunning(true);
    QCOMPARE(spy.count(), 1);

    facade.setRunning(true);
    QCOMPARE(spy.count(), 1);

    facade.setRunning(false);
    QCOMPARE(spy.count(), 2);
}

void StorageFacadeTest::autoDownloadDefaultsOn()
{
    StorageFacade facade;
    QVERIFY(facade.autoDownload());
}

void StorageFacadeTest::autoDownloadTogglesAndNotifies()
{
    StorageFacade facade;
    QSignalSpy spy(&facade, &StorageFacade::changed);

    facade.setAutoDownload(false);
    QVERIFY(!facade.autoDownload());
    QCOMPARE(spy.count(), 1);

    facade.setAutoDownload(true);
    QVERIFY(facade.autoDownload());
    QCOMPARE(spy.count(), 2);
}

void StorageFacadeTest::autoDownloadSameValueIsSilent()
{
    StorageFacade facade;
    facade.setAutoDownload(false);

    QSignalSpy spy(&facade, &StorageFacade::changed);
    facade.setAutoDownload(false);
    QCOMPARE(spy.count(), 0);
}

void StorageFacadeTest::usageFormatsSentAndReceived()
{
    StorageFacade facade;

    facade.setUsage({QStringLiteral("Photos"), QStringLiteral("Videos")},
                    {QStringLiteral("1024"), QStringLiteral("2048")},
                    {QStringLiteral("512"), QStringLiteral("0")});

    const QStringList expected{
        QStringLiteral("Photos") + dot() + QStringLiteral("1.0 KB sent") + dot()
            + QStringLiteral("512 B received"),
        QStringLiteral("Videos") + dot() + QStringLiteral("2.0 KB sent") + dot()
            + QStringLiteral("0 B received"),
    };
    QCOMPARE(facade.usage(), expected);
}

void StorageFacadeTest::usageDropsBlankAndDuplicateCategories()
{
    StorageFacade facade;

    facade.setUsage({QStringLiteral("Photos"), QString(), QStringLiteral("Photos"),
                     QStringLiteral("Videos")},
                    {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                     QStringLiteral("4")},
                    {QStringLiteral("10"), QStringLiteral("20"), QStringLiteral("30"),
                     QStringLiteral("40")});

    const QStringList expected{
        QStringLiteral("Photos") + dot() + QStringLiteral("1 B sent") + dot()
            + QStringLiteral("10 B received"),
        QStringLiteral("Videos") + dot() + QStringLiteral("4 B sent") + dot()
            + QStringLiteral("40 B received"),
    };
    QCOMPARE(facade.usage(), expected);
}

void StorageFacadeTest::usageShortListsReadAsZero()
{
    StorageFacade facade;

    facade.setUsage({QStringLiteral("Photos")}, {QStringLiteral("1024")}, {});

    QCOMPARE(facade.usage(),
             QStringList({QStringLiteral("Photos") + dot() + QStringLiteral("1.0 KB sent")
                          + dot() + QStringLiteral("0 B received")}));
}

void StorageFacadeTest::usageClearsWhenEmpty()
{
    StorageFacade facade;
    facade.setUsage({QStringLiteral("Photos")}, {QStringLiteral("1024")},
                    {QStringLiteral("512")});
    QCOMPARE(facade.usage().size(), 1);

    facade.setUsage({}, {}, {});
    QVERIFY(facade.usage().isEmpty());
}

void StorageFacadeTest::changedEmittedOncePerStorageUpdate()
{
    StorageFacade facade;
    QSignalSpy spy(&facade, &StorageFacade::changed);

    loadSampleStorage(facade);
    QCOMPARE(spy.count(), 1);

    // A redundant reload changes nothing on either model.
    loadSampleStorage(facade);
    QCOMPARE(spy.count(), 1);

    facade.setSelection({QStringLiteral("photo")});
    QCOMPARE(spy.count(), 2);

    // This update replaces the rows and prunes the selection at the same time;
    // it must still repaint once.
    facade.setStorage({QStringLiteral("video")}, {QStringLiteral("3")},
                      {QStringLiteral("2048")});
    QCOMPARE(spy.count(), 3);
    QVERIFY(facade.selectedKinds().isEmpty());
}

void StorageFacadeTest::changedCoversEveryUpdate()
{
    StorageFacade facade;
    QSignalSpy spy(&facade, &StorageFacade::changed);

    loadSampleStorage(facade);
    QCOMPARE(spy.count(), 1);

    facade.setSelection({QStringLiteral("video")});
    QCOMPARE(spy.count(), 2);

    facade.setRunning(true);
    QCOMPARE(spy.count(), 3);

    facade.setUsage({QStringLiteral("Photos")}, {QStringLiteral("1024")},
                    {QStringLiteral("512")});
    QCOMPARE(spy.count(), 4);

    facade.setAutoDownload(false);
    QCOMPARE(spy.count(), 5);

    // Repeating any of them is silent.
    loadSampleStorage(facade);
    facade.setSelection({QStringLiteral("video")});
    facade.setRunning(true);
    facade.setUsage({QStringLiteral("Photos")}, {QStringLiteral("1024")},
                    {QStringLiteral("512")});
    facade.setAutoDownload(false);
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(StorageFacadeTest)

#include "tst_storage_facade.moc"
