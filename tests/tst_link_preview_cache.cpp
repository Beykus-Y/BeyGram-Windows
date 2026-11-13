#include "telegram/LinkPreviewCache.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::CachedLinkPreview;
using beygram::telegram::LinkPreviewCache;

namespace {

CachedLinkPreview makePreview(const QString& url)
{
    CachedLinkPreview preview;
    preview.url = url;
    preview.title = QStringLiteral("Title for %1").arg(url);
    preview.description = QStringLiteral("Description for %1").arg(url);
    preview.siteName = QStringLiteral("example.com");
    preview.imageFileId = 42;
    preview.isVideo = false;
    preview.fetchedAt = 0;
    return preview;
}

QString urlFor(int index)
{
    return QStringLiteral("https://example.com/%1").arg(index);
}

} // namespace

class LinkPreviewCacheTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void defaultCapacity();
    void setCapacityClampsToBounds();
    void setCapacityToCurrentValueDoesNotSignal();
    void setCapacityShrinkEvictsLeastRecentlyFetched();
    void setCapacityBelowMinimumKeepsEight();
    void setCapacityIncreaseKeepsEntries();
    void putTrimsUrl();
    void putIgnoresEmptyUrl();
    void putStampsFetchedAt();
    void putKeepsFields();
    void putReplacesPreviewWithSameUrl();
    void putSamePreviewAndNowIsNoOp();
    void putSameContentWithNewerNowSignals();
    void putAtCapacityEvictsLeastRecentlyFetched();
    void putNewcomerOlderThanAllChangesNothing();
    void evictionTieBreaksOnUrl();
    void getReturnsStoredCopy();
    void getOfMissingUrlIsDefault();
    void getDoesNotTouchRecency();
    void containsTracksPutAndRemove();
    void removeDropsOnlyThatUrl();
    void removeMissingOrEmptyIsNoOp();
    void removeTrimsUrl();
    void clearEmptiesStore();
    void clearOnEmptyDoesNotSignal();
    void putAfterRemoveAddsAgain();
    void replacementAtCapacityDoesNotEvict();
    void replacementOlderThanAllEvictsItself();
    void changedOnlyOnRealChanges();
};

void LinkPreviewCacheTest::startsEmpty()
{
    LinkPreviewCache cache;

    QCOMPARE(cache.count(), 0);
    QVERIFY(!cache.contains(urlFor(1)));
    QVERIFY(!cache.contains(QString()));
    QVERIFY(cache.get(urlFor(1)).url.isEmpty());
}

void LinkPreviewCacheTest::defaultCapacity()
{
    LinkPreviewCache cache;

    QCOMPARE(cache.capacity(), 100);
}

void LinkPreviewCacheTest::setCapacityClampsToBounds()
{
    LinkPreviewCache cache;
    QSignalSpy spy(&cache, &LinkPreviewCache::changed);

    cache.setCapacity(0);
    QCOMPARE(cache.capacity(), 8);

    cache.setCapacity(7);
    QCOMPARE(cache.capacity(), 8);

    cache.setCapacity(-100);
    QCOMPARE(cache.capacity(), 8);

    cache.setCapacity(501);
    QCOMPARE(cache.capacity(), 500);

    cache.setCapacity(1000000);
    QCOMPARE(cache.capacity(), 500);

    cache.setCapacity(250);
    QCOMPARE(cache.capacity(), 250);

    // 100 -> 8, 8 -> 500, 500 -> 250; the clamped no-ops did not signal.
    QCOMPARE(spy.count(), 3);
}

void LinkPreviewCacheTest::setCapacityToCurrentValueDoesNotSignal()
{
    LinkPreviewCache cache;
    QSignalSpy spy(&cache, &LinkPreviewCache::changed);

    cache.setCapacity(100);
    QCOMPARE(spy.count(), 0);

    cache.setCapacity(50); // real change, even with an empty store
    QCOMPARE(cache.capacity(), 50);
    QCOMPARE(spy.count(), 1);

    cache.setCapacity(50);
    QCOMPARE(spy.count(), 1);

    cache.setCapacity(1); // clamps to 8
    QCOMPARE(cache.capacity(), 8);
    QCOMPARE(spy.count(), 2);

    cache.setCapacity(8);
    QCOMPARE(spy.count(), 2);
}

void LinkPreviewCacheTest::setCapacityShrinkEvictsLeastRecentlyFetched()
{
    LinkPreviewCache cache;
    cache.setCapacity(20);
    for (int i = 1; i <= 20; ++i)
        cache.put(makePreview(urlFor(i)), i * 100);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.setCapacity(8);

    QCOMPARE(cache.capacity(), 8);
    QCOMPARE(cache.count(), 8);
    for (int i = 1; i <= 12; ++i)
        QVERIFY(!cache.contains(urlFor(i)));
    for (int i = 13; i <= 20; ++i)
        QVERIFY(cache.contains(urlFor(i)));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::setCapacityBelowMinimumKeepsEight()
{
    LinkPreviewCache cache;
    cache.setCapacity(20);
    for (int i = 1; i <= 10; ++i)
        cache.put(makePreview(urlFor(i)), i * 10);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.setCapacity(0);

    QCOMPARE(cache.capacity(), 8);
    QCOMPARE(cache.count(), 8);
    QVERIFY(!cache.contains(urlFor(1)));
    QVERIFY(!cache.contains(urlFor(2)));
    QVERIFY(cache.contains(urlFor(3)));
    QVERIFY(cache.contains(urlFor(10)));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::setCapacityIncreaseKeepsEntries()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    for (int i = 1; i <= 8; ++i)
        cache.put(makePreview(urlFor(i)), i);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.setCapacity(500);

    QCOMPARE(cache.capacity(), 500);
    QCOMPARE(cache.count(), 8);
    for (int i = 1; i <= 8; ++i)
        QVERIFY(cache.contains(urlFor(i)));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::putTrimsUrl()
{
    LinkPreviewCache cache;
    CachedLinkPreview preview = makePreview(QStringLiteral("  https://example.com/a  "));
    preview.title = QStringLiteral("Trimmed");

    cache.put(preview, 100);

    QCOMPARE(cache.count(), 1);
    QVERIFY(cache.contains(QStringLiteral("https://example.com/a")));
    QVERIFY(cache.contains(QStringLiteral("  https://example.com/a\n")));
    QCOMPARE(cache.get(QStringLiteral(" https://example.com/a ")).url,
             QStringLiteral("https://example.com/a"));
    QCOMPARE(cache.get(QStringLiteral("https://example.com/a")).title,
             QStringLiteral("Trimmed"));
}

void LinkPreviewCacheTest::putIgnoresEmptyUrl()
{
    LinkPreviewCache cache;
    QSignalSpy spy(&cache, &LinkPreviewCache::changed);

    cache.put(makePreview(QString()), 1);
    cache.put(makePreview(QStringLiteral("   ")), 2);
    cache.put(makePreview(QStringLiteral("\t\n ")), 3);

    QCOMPARE(cache.count(), 0);
    QCOMPARE(spy.count(), 0);

    cache.put(makePreview(urlFor(1)), 10);
    QCOMPARE(spy.count(), 1);

    cache.put(makePreview(QStringLiteral("  ")), 20);
    QCOMPARE(cache.count(), 1);
    QCOMPARE(spy.count(), 1);
    QVERIFY(cache.contains(urlFor(1)));
}

void LinkPreviewCacheTest::putStampsFetchedAt()
{
    LinkPreviewCache cache;

    CachedLinkPreview preview = makePreview(urlFor(1));
    preview.fetchedAt = 999999;
    cache.put(preview, 1234);
    QCOMPARE(cache.get(urlFor(1)).fetchedAt, qint64(1234));

    cache.put(makePreview(urlFor(2)), 0);
    QCOMPARE(cache.get(urlFor(2)).fetchedAt, qint64(0));
}

void LinkPreviewCacheTest::putKeepsFields()
{
    LinkPreviewCache cache;
    CachedLinkPreview preview;
    preview.url = QStringLiteral("https://example.com/story");
    preview.title = QStringLiteral("A story worth reading");
    preview.description = QStringLiteral("Short summary of the story.");
    preview.siteName = QStringLiteral("Example");
    preview.imageFileId = 77;
    preview.isVideo = true;
    preview.fetchedAt = 5;

    cache.put(preview, 1700000000);

    const CachedLinkPreview stored = cache.get(preview.url);
    QCOMPARE(stored.url, preview.url);
    QCOMPARE(stored.title, preview.title);
    QCOMPARE(stored.description, preview.description);
    QCOMPARE(stored.siteName, preview.siteName);
    QCOMPARE(stored.imageFileId, 77);
    QVERIFY(stored.isVideo);
    QCOMPARE(stored.fetchedAt, qint64(1700000000));
}

void LinkPreviewCacheTest::putReplacesPreviewWithSameUrl()
{
    LinkPreviewCache cache;
    cache.put(makePreview(urlFor(1)), 100);

    CachedLinkPreview updated = makePreview(urlFor(1));
    updated.title = QStringLiteral("Updated");
    updated.imageFileId = 9;
    updated.isVideo = true;
    cache.put(updated, 200);

    QCOMPARE(cache.count(), 1);
    QCOMPARE(cache.get(urlFor(1)).title, QStringLiteral("Updated"));
    QCOMPARE(cache.get(urlFor(1)).imageFileId, 9);
    QVERIFY(cache.get(urlFor(1)).isVideo);
    QCOMPARE(cache.get(urlFor(1)).fetchedAt, qint64(200));
}

void LinkPreviewCacheTest::putSamePreviewAndNowIsNoOp()
{
    LinkPreviewCache cache;
    const CachedLinkPreview preview = makePreview(urlFor(1));
    cache.put(preview, 100);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(preview, 100);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(cache.count(), 1);
}

void LinkPreviewCacheTest::putSameContentWithNewerNowSignals()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    for (int i = 1; i <= 8; ++i)
        cache.put(makePreview(urlFor(i)), i * 10);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(makePreview(urlFor(1)), 500);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(cache.get(urlFor(1)).fetchedAt, qint64(500));

    // The refreshed entry is now the newest, so url 2 is the oldest one left.
    cache.put(makePreview(urlFor(9)), 510);
    QCOMPARE(cache.count(), 8);
    QVERIFY(cache.contains(urlFor(1)));
    QVERIFY(!cache.contains(urlFor(2)));
    QVERIFY(cache.contains(urlFor(9)));
    QCOMPARE(spy.count(), 2);
}

void LinkPreviewCacheTest::putAtCapacityEvictsLeastRecentlyFetched()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    for (int i = 1; i <= 8; ++i)
        cache.put(makePreview(urlFor(i)), i * 10);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(makePreview(urlFor(9)), 90);

    QCOMPARE(cache.count(), 8);
    QVERIFY(!cache.contains(urlFor(1)));
    QVERIFY(cache.contains(urlFor(2)));
    QVERIFY(cache.contains(urlFor(9)));
    QCOMPARE(cache.get(urlFor(9)).fetchedAt, qint64(90));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::putNewcomerOlderThanAllChangesNothing()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    for (int i = 2; i <= 9; ++i)
        cache.put(makePreview(urlFor(i)), 1000);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(makePreview(urlFor(1)), 500);

    QCOMPARE(cache.count(), 8);
    QVERIFY(!cache.contains(urlFor(1)));
    QVERIFY(cache.contains(urlFor(2)));
    QVERIFY(cache.contains(urlFor(9)));
    QCOMPARE(spy.count(), 0);
}

void LinkPreviewCacheTest::evictionTieBreaksOnUrl()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    const QVector<int> insertionOrder{15, 11, 17, 10, 14, 12, 16, 13};
    for (int index : insertionOrder)
        cache.put(makePreview(urlFor(index)), 1000);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(makePreview(urlFor(18)), 1000);

    QCOMPARE(cache.count(), 8);
    QVERIFY(!cache.contains(urlFor(10)));
    for (int i = 11; i <= 18; ++i)
        QVERIFY(cache.contains(urlFor(i)));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::getReturnsStoredCopy()
{
    LinkPreviewCache cache;
    const CachedLinkPreview preview = makePreview(urlFor(1));
    cache.put(preview, 100);

    CachedLinkPreview copy = cache.get(urlFor(1));
    copy.title = QStringLiteral("mutated");

    QCOMPARE(cache.get(urlFor(1)).title, preview.title);
    QCOMPARE(cache.count(), 1);
}

void LinkPreviewCacheTest::getOfMissingUrlIsDefault()
{
    LinkPreviewCache cache;
    cache.put(makePreview(urlFor(1)), 100);

    const CachedLinkPreview missing = cache.get(urlFor(2));
    QVERIFY(missing.url.isEmpty());
    QVERIFY(missing.title.isEmpty());
    QVERIFY(missing.description.isEmpty());
    QVERIFY(missing.siteName.isEmpty());
    QCOMPARE(missing.imageFileId, 0);
    QVERIFY(!missing.isVideo);
    QCOMPARE(missing.fetchedAt, qint64(0));

    QVERIFY(cache.get(QString()).url.isEmpty());
    QVERIFY(cache.get(QStringLiteral("   ")).url.isEmpty());
}

void LinkPreviewCacheTest::getDoesNotTouchRecency()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    for (int i = 1; i <= 8; ++i)
        cache.put(makePreview(urlFor(i)), i * 10);

    QCOMPARE(cache.get(urlFor(1)).url, urlFor(1));
    QCOMPARE(cache.get(urlFor(1)).url, urlFor(1));
    QVERIFY(cache.contains(urlFor(1)));

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(makePreview(urlFor(9)), 90);

    // A pure read must not have refreshed url 1, so it is still the oldest.
    QVERIFY(!cache.contains(urlFor(1)));
    QVERIFY(cache.contains(urlFor(2)));
    QVERIFY(cache.contains(urlFor(9)));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::containsTracksPutAndRemove()
{
    LinkPreviewCache cache;

    QVERIFY(!cache.contains(urlFor(3)));
    cache.put(makePreview(urlFor(3)), 100);
    QVERIFY(cache.contains(urlFor(3)));
    cache.remove(urlFor(3));
    QVERIFY(!cache.contains(urlFor(3)));
}

void LinkPreviewCacheTest::removeDropsOnlyThatUrl()
{
    LinkPreviewCache cache;
    cache.put(makePreview(urlFor(1)), 10);
    cache.put(makePreview(urlFor(2)), 20);
    cache.put(makePreview(urlFor(3)), 30);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.remove(urlFor(2));

    QCOMPARE(cache.count(), 2);
    QVERIFY(cache.contains(urlFor(1)));
    QVERIFY(!cache.contains(urlFor(2)));
    QVERIFY(cache.contains(urlFor(3)));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::removeMissingOrEmptyIsNoOp()
{
    LinkPreviewCache cache;
    cache.put(makePreview(urlFor(1)), 100);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.remove(urlFor(2));
    cache.remove(QString());
    cache.remove(QStringLiteral("   "));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(cache.count(), 1);
}

void LinkPreviewCacheTest::removeTrimsUrl()
{
    LinkPreviewCache cache;
    cache.put(makePreview(QStringLiteral("  https://example.com/a  ")), 100);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.remove(QStringLiteral("https://example.com/a"));

    QCOMPARE(cache.count(), 0);
    QVERIFY(!cache.contains(QStringLiteral("https://example.com/a")));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::clearEmptiesStore()
{
    LinkPreviewCache cache;
    cache.put(makePreview(urlFor(1)), 100);
    cache.put(makePreview(urlFor(2)), 200);

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.clear();

    QCOMPARE(cache.count(), 0);
    QVERIFY(!cache.contains(urlFor(1)));
    QVERIFY(!cache.contains(urlFor(2)));
    QVERIFY(cache.get(urlFor(1)).url.isEmpty());
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::clearOnEmptyDoesNotSignal()
{
    LinkPreviewCache cache;
    QSignalSpy spy(&cache, &LinkPreviewCache::changed);

    cache.clear();
    cache.clear();

    QCOMPARE(spy.count(), 0);
}

void LinkPreviewCacheTest::putAfterRemoveAddsAgain()
{
    LinkPreviewCache cache;
    cache.put(makePreview(urlFor(1)), 100);
    cache.remove(urlFor(1));

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(makePreview(urlFor(1)), 200);

    QCOMPARE(cache.count(), 1);
    QCOMPARE(cache.get(urlFor(1)).fetchedAt, qint64(200));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::replacementAtCapacityDoesNotEvict()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    for (int i = 1; i <= 8; ++i)
        cache.put(makePreview(urlFor(i)), i * 10);

    CachedLinkPreview updated = makePreview(urlFor(1));
    updated.title = QStringLiteral("Updated");

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(updated, 999);

    QCOMPARE(cache.count(), 8);
    for (int i = 1; i <= 8; ++i)
        QVERIFY(cache.contains(urlFor(i)));
    QCOMPARE(cache.get(urlFor(1)).title, QStringLiteral("Updated"));
    QCOMPARE(cache.get(urlFor(1)).fetchedAt, qint64(999));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::replacementOlderThanAllEvictsItself()
{
    LinkPreviewCache cache;
    cache.setCapacity(8);
    for (int i = 1; i <= 8; ++i)
        cache.put(makePreview(urlFor(i)), i * 1000);

    CachedLinkPreview stale = makePreview(urlFor(1));
    stale.title = QStringLiteral("Stale");

    QSignalSpy spy(&cache, &LinkPreviewCache::changed);
    cache.put(stale, 0);

    // Replacing an existing entry never changes the count.
    QCOMPARE(cache.count(), 8);
    QVERIFY(cache.contains(urlFor(1)));
    QVERIFY(cache.contains(urlFor(2)));
    QVERIFY(cache.contains(urlFor(8)));
    QCOMPARE(spy.count(), 1);
}

void LinkPreviewCacheTest::changedOnlyOnRealChanges()
{
    LinkPreviewCache cache;
    QSignalSpy spy(&cache, &LinkPreviewCache::changed);

    cache.put(makePreview(urlFor(1)), 100);             // new entry
    cache.put(makePreview(urlFor(1)), 100);             // identical
    cache.put(makePreview(QStringLiteral("  ")), 100);  // empty url
    cache.put(makePreview(urlFor(2)), 200);             // new entry
    cache.put(makePreview(urlFor(1)), 300);             // recency moved
    cache.remove(urlFor(99));                           // absent
    cache.remove(urlFor(2));                            // present
    cache.clear();                                      // one left
    cache.clear();                                      // already empty

    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(LinkPreviewCacheTest)
#include "tst_link_preview_cache.moc"
