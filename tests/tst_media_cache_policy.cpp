#include "telegram/MediaCachePolicy.h"

#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {
constexpr qint64 MiB = 1024LL * 1024LL;

QVector<MediaKind> allKinds()
{
    return {MediaKind::Photo, MediaKind::Video, MediaKind::Document, MediaKind::Audio,
            MediaKind::Other};
}
} // namespace

class MediaCachePolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreConservative();
    void photoWithinLimitDownloadsAutomatically();
    void knownSizeAtTheLimitIsAccepted();
    void oneByteOverTheLimitIsRefused();
    void disabledKindNeverDownloads();
    void unknownSizeFollowsTheEnabledFlag();
    void negativeSizeIsTreatedAsUnknown();
    void documentsCoverFilesAndAudio();
    void eachKindUsesItsOwnLimit();
    void nonPositiveLimitRefusesEveryKnownSize();
    void cacheLimitDoesNotGateASingleFile();
    void hugeSizeDoesNotOverflow();
    void visibleMediaOutranksBackgroundMedia();
    void photosOutrankOtherBackgroundKinds();
    void prioritiesStayInTdlibRange();
};

void MediaCachePolicyTest::defaultsAreConservative()
{
    const MediaCacheRules rules;
    QVERIFY(rules.photos);
    QVERIFY(!rules.videos);
    QVERIFY(!rules.documents);
    QCOMPARE(rules.maxPhotoBytes, 10 * MiB);
    QCOMPARE(rules.maxVideoBytes, 50 * MiB);
    QCOMPARE(rules.maxDocumentBytes, 20 * MiB);
    QCOMPARE(rules.cacheLimitBytes, 1024LL * MiB);
}

void MediaCachePolicyTest::photoWithinLimitDownloadsAutomatically()
{
    const MediaCacheRules rules;
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 1));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 3 * MiB));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 10 * MiB));
}

void MediaCachePolicyTest::knownSizeAtTheLimitIsAccepted()
{
    MediaCacheRules rules;
    rules.videos = true;
    rules.documents = true;
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, rules.maxPhotoBytes));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Video, rules.maxVideoBytes));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Document, rules.maxDocumentBytes));
}

void MediaCachePolicyTest::oneByteOverTheLimitIsRefused()
{
    MediaCacheRules rules;
    rules.videos = true;
    rules.documents = true;
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Photo, rules.maxPhotoBytes + 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Video, rules.maxVideoBytes + 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Document, rules.maxDocumentBytes + 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Audio, rules.maxDocumentBytes + 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Other, rules.maxDocumentBytes + 1));
}

void MediaCachePolicyTest::disabledKindNeverDownloads()
{
    MediaCacheRules rules;
    // Videos and files are off by default, whatever their size.
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Video, 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Document, 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Audio, 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Other, 1));

    rules.photos = false;
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Photo, 1));

    // A disabled kind also refuses an unknown size; there is nothing to fetch.
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Photo, 0));

    rules.photos = true;
    rules.videos = false;
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Video, 1));
}

void MediaCachePolicyTest::unknownSizeFollowsTheEnabledFlag()
{
    const MediaCacheRules rules;
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 0));

    MediaCacheRules enabled;
    enabled.videos = true;
    enabled.documents = true;
    QVERIFY(shouldAutoDownload(enabled, MediaKind::Video, 0));
    QVERIFY(shouldAutoDownload(enabled, MediaKind::Document, 0));
    QVERIFY(shouldAutoDownload(enabled, MediaKind::Audio, 0));
    QVERIFY(shouldAutoDownload(enabled, MediaKind::Other, 0));
}

void MediaCachePolicyTest::negativeSizeIsTreatedAsUnknown()
{
    MediaCacheRules rules;
    rules.videos = true;
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, -1));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Video, -1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Document, -1));
}

void MediaCachePolicyTest::documentsCoverFilesAndAudio()
{
    MediaCacheRules rules;
    rules.documents = true;
    for (const MediaKind kind : {MediaKind::Document, MediaKind::Audio, MediaKind::Other}) {
        QVERIFY2(shouldAutoDownload(rules, kind, 5 * MiB), "enabled file kind within its limit");
        QVERIFY2(shouldAutoDownload(rules, kind, rules.maxDocumentBytes),
                 "enabled file kind at its limit");
        QVERIFY2(!shouldAutoDownload(rules, kind, rules.maxDocumentBytes + 1),
                 "enabled file kind above its limit");
    }
    // Enabling files does not turn videos on.
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Video, 1));
}

void MediaCachePolicyTest::eachKindUsesItsOwnLimit()
{
    MediaCacheRules rules;
    rules.videos = true;
    rules.documents = true;
    rules.maxPhotoBytes = 1 * MiB;
    rules.maxVideoBytes = 4 * MiB;
    rules.maxDocumentBytes = 2 * MiB;

    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 1 * MiB));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Photo, 2 * MiB));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Video, 4 * MiB));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Video, 5 * MiB));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Document, 2 * MiB));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Document, 3 * MiB));
}

void MediaCachePolicyTest::nonPositiveLimitRefusesEveryKnownSize()
{
    MediaCacheRules rules;
    rules.videos = true;
    rules.documents = true;
    rules.maxPhotoBytes = 0;
    rules.maxVideoBytes = -1;
    rules.maxDocumentBytes = 0;

    QVERIFY(!shouldAutoDownload(rules, MediaKind::Photo, 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Video, 1));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Document, 1));

    // An unknown size is still allowed, because the limit cannot be compared yet.
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 0));
}

void MediaCachePolicyTest::cacheLimitDoesNotGateASingleFile()
{
    MediaCacheRules rules;
    rules.videos = true;
    rules.cacheLimitBytes = 1 * MiB;
    // The cache budget is enforced by the store, not by the per-file decision.
    QVERIFY(shouldAutoDownload(rules, MediaKind::Video, rules.maxVideoBytes));
    QVERIFY(shouldAutoDownload(rules, MediaKind::Photo, 10 * MiB));
}

void MediaCachePolicyTest::hugeSizeDoesNotOverflow()
{
    MediaCacheRules rules;
    rules.videos = true;
    rules.documents = true;
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Video, std::numeric_limits<qint64>::max()));
    QVERIFY(!shouldAutoDownload(rules, MediaKind::Document,
                                std::numeric_limits<qint64>::max()));

    rules.maxVideoBytes = std::numeric_limits<qint64>::max();
    QVERIFY(shouldAutoDownload(rules, MediaKind::Video, std::numeric_limits<qint64>::max()));
}

void MediaCachePolicyTest::visibleMediaOutranksBackgroundMedia()
{
    for (const MediaKind kind : allKinds()) {
        QCOMPARE(priorityFor(kind, true), 32);
        QVERIFY2(priorityFor(kind, true) > priorityFor(kind, false),
                 "a visible file is fetched before a background one");
    }
}

void MediaCachePolicyTest::photosOutrankOtherBackgroundKinds()
{
    QCOMPARE(priorityFor(MediaKind::Photo, false), 16);
    for (const MediaKind kind : {MediaKind::Video, MediaKind::Document, MediaKind::Audio,
                                 MediaKind::Other}) {
        QCOMPARE(priorityFor(kind, false), 8);
    }
}

void MediaCachePolicyTest::prioritiesStayInTdlibRange()
{
    for (const MediaKind kind : allKinds()) {
        for (const bool visible : {true, false}) {
            const int priority = priorityFor(kind, visible);
            QVERIFY2(priority >= 1 && priority <= 32, "TDLib accepts priorities 1..32");
        }
    }
}

QTEST_GUILESS_MAIN(MediaCachePolicyTest)
#include "tst_media_cache_policy.moc"
