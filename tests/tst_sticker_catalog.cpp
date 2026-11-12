#include "telegram/StickerCatalog.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QString stickerTypeName(const QJsonObject& request)
{
    return request.value(QStringLiteral("sticker_type"))
        .toObject()
        .value(QStringLiteral("@type"))
        .toString();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class StickerCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsInstalledStickerSetsRequest();
    void buildsArchivedStickerSetsRequest();
    void buildsFeaturedStickerSetsRequest();
    void buildsChangeStickerSetRequest();
    void requestsSurviveSerialization();

    void parsesInstalledStickerSets();
    void keepsArchivedFlagPerSet();
    void parsesFeaturedStickerSets();
    void parsesRecentStickerIds();

    void fallsBackToCoverPreview();
    void readsStickerCountFromEitherField();
    void ignoresMalformedResponses();
    void keepsLargeIdsAndRejectsOversizedFileIds();
};

void StickerCatalogTest::buildsInstalledStickerSetsRequest()
{
    const QJsonObject request = getInstalledStickerSetsRequest();

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getInstalledStickerSets"));
    QCOMPARE(stickerTypeName(request), QStringLiteral("stickerTypeRegular"));
    QCOMPARE(request.value(QStringLiteral("sticker_type")).toObject().size(), 1);
    // getInstalledStickerSets has no archive switch in the pinned schema.
    QVERIFY(!request.contains(QStringLiteral("is_archived")));
    QCOMPARE(request.size(), 2);

    // Passing false explicitly must produce the same request as the default.
    QCOMPARE(QJsonDocument(getInstalledStickerSetsRequest(false)).toJson(QJsonDocument::Compact),
             QJsonDocument(request).toJson(QJsonDocument::Compact));
}

void StickerCatalogTest::buildsArchivedStickerSetsRequest()
{
    const QJsonObject request = getInstalledStickerSetsRequest(true);

    // Archived sets come from their own function; getInstalledStickerSets has
    // no is_archived field to toggle.
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getArchivedStickerSets"));
    QCOMPARE(stickerTypeName(request), QStringLiteral("stickerTypeRegular"));
    QCOMPARE(numberAt(request, QStringLiteral("offset_sticker_set_id")), qint64(0));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(request.size(), 4);
}

void StickerCatalogTest::buildsFeaturedStickerSetsRequest()
{
    const QJsonObject request = getFeaturedStickerSetsRequest();

    // The pinned schema replaced featured sets with trending sets; the request
    // must use the type TDLib actually knows.
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getTrendingStickerSets"));
    QVERIFY(request.value(QStringLiteral("@type")).toString()
            != QLatin1String("getFeaturedStickerSets"));
    QCOMPARE(stickerTypeName(request), QStringLiteral("stickerTypeRegular"));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(request.size(), 4);
}

void StickerCatalogTest::buildsChangeStickerSetRequest()
{
    const QJsonObject installed = changeStickerSetRequest(12345, true);

    QCOMPARE(installed.value(QStringLiteral("@type")).toString(),
             QStringLiteral("changeStickerSet"));
    QCOMPARE(numberAt(installed, QStringLiteral("set_id")), qint64(12345));
    QCOMPARE(installed.value(QStringLiteral("is_installed")).toBool(), true);
    // TDLib requires is_archived on every call, including a plain install.
    QCOMPARE(installed.value(QStringLiteral("is_archived")).toBool(), false);
    QCOMPARE(installed.size(), 4);

    const QJsonObject removed = changeStickerSetRequest(7, false, false);
    QCOMPARE(removed.value(QStringLiteral("is_installed")).toBool(), false);
    QCOMPARE(removed.value(QStringLiteral("is_archived")).toBool(), false);

    const QJsonObject archived = changeStickerSetRequest(7, false, true);
    QCOMPARE(archived.value(QStringLiteral("is_installed")).toBool(), false);
    QCOMPARE(archived.value(QStringLiteral("is_archived")).toBool(), true);

    const QJsonObject both = changeStickerSetRequest(7, true, true);
    QCOMPARE(both.value(QStringLiteral("is_installed")).toBool(), true);
    QCOMPARE(both.value(QStringLiteral("is_archived")).toBool(), true);
}

void StickerCatalogTest::requestsSurviveSerialization()
{
    const QJsonObject installed = serialized(getInstalledStickerSetsRequest());
    QCOMPARE(installed.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getInstalledStickerSets"));
    QCOMPARE(stickerTypeName(installed), QStringLiteral("stickerTypeRegular"));

    const QJsonObject archived = serialized(getInstalledStickerSetsRequest(true));
    QCOMPARE(archived.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getArchivedStickerSets"));
    QCOMPARE(numberAt(archived, QStringLiteral("offset_sticker_set_id")), qint64(0));
    QCOMPARE(archived.value(QStringLiteral("limit")).toInt(), 100);

    const QJsonObject featured = serialized(getFeaturedStickerSetsRequest());
    QCOMPARE(featured.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getTrendingStickerSets"));

    const QJsonObject change = serialized(changeStickerSetRequest(4242424242LL, true, true));
    QCOMPARE(change.value(QStringLiteral("@type")).toString(),
             QStringLiteral("changeStickerSet"));
    QCOMPARE(numberAt(change, QStringLiteral("set_id")), qint64(4242424242LL));
    QCOMPARE(change.value(QStringLiteral("is_installed")).toBool(), true);
    QCOMPARE(change.value(QStringLiteral("is_archived")).toBool(), true);
}

void StickerCatalogTest::parsesInstalledStickerSets()
{
    const StickerCatalogState state = stickerCatalogFromResponse(parseObject(R"({
        "@type": "stickerSets",
        "total_count": 2,
        "sets": [
            {
                "@type": "stickerSetInfo",
                "id": 111,
                "title": "Cats",
                "name": "cats",
                "size": 24,
                "thumbnail": {"width": 128, "height": 128, "file": {"id": 777}},
                "is_installed": true,
                "is_archived": false,
                "covers": []
            },
            {
                "@type": "stickerSetInfo",
                "id": 222,
                "title": "Dogs",
                "name": "dogs",
                "size": 5,
                "is_installed": true,
                "is_archived": false
            }
        ]
    })"));

    QCOMPARE(state.installedSets.size(), 2);
    QVERIFY(state.featuredSets.isEmpty());
    QVERIFY(state.recentStickerIds.isEmpty());

    const StickerSetInfo& cats = state.installedSets.at(0);
    QCOMPARE(cats.id, qint64(111));
    QCOMPARE(cats.title, QStringLiteral("Cats"));
    QCOMPARE(cats.name, QStringLiteral("cats"));
    QCOMPARE(cats.stickerCount, 24);
    QCOMPARE(cats.thumbnailFileId, 777);
    QCOMPARE(cats.installed, true);
    QCOMPARE(cats.archived, false);

    const StickerSetInfo& dogs = state.installedSets.at(1);
    QCOMPARE(dogs.id, qint64(222));
    QCOMPARE(dogs.stickerCount, 5);
    // No thumbnail and no covers leaves the preview unset instead of guessing.
    QCOMPARE(dogs.thumbnailFileId, 0);
    QCOMPARE(dogs.installed, true);
}

void StickerCatalogTest::keepsArchivedFlagPerSet()
{
    const StickerCatalogState state = stickerCatalogFromResponse(parseObject(R"({
        "@type": "stickerSets",
        "sets": [
            {"id": 1, "is_installed": true, "is_archived": false},
            {"id": 2, "is_installed": true, "is_archived": true},
            {"id": 3, "is_installed": false, "is_archived": true}
        ]
    })"));

    QCOMPARE(state.installedSets.size(), 3);
    QCOMPARE(state.installedSets.at(0).archived, false);
    QCOMPARE(state.installedSets.at(1).archived, true);
    QCOMPARE(state.installedSets.at(2).archived, true);
    QCOMPARE(state.installedSets.at(2).installed, false);
}

void StickerCatalogTest::parsesFeaturedStickerSets()
{
    const StickerCatalogState state = stickerCatalogFromResponse(parseObject(R"({
        "@type": "trendingStickerSets",
        "total_count": 1,
        "sets": [
            {
                "id": 333,
                "title": "Trend",
                "name": "trend",
                "size": 9,
                "thumbnail": {"file": {"id": 900}},
                "is_installed": false,
                "is_archived": false
            }
        ]
    })"));

    QCOMPARE(state.featuredSets.size(), 1);
    QVERIFY(state.installedSets.isEmpty());
    QVERIFY(state.recentStickerIds.isEmpty());

    const StickerSetInfo& trend = state.featuredSets.at(0);
    QCOMPARE(trend.id, qint64(333));
    QCOMPARE(trend.title, QStringLiteral("Trend"));
    QCOMPARE(trend.stickerCount, 9);
    QCOMPARE(trend.thumbnailFileId, 900);
    QCOMPARE(trend.installed, false);
}

void StickerCatalogTest::parsesRecentStickerIds()
{
    const StickerCatalogState state = stickerCatalogFromResponse(parseObject(R"({
        "@type": "stickers",
        "stickers": [
            {"id": 10},
            {"id": 20},
            {"id": 10},
            {"id": 0},
            {"id": -5},
            {"id": "abc"},
            null,
            "nope",
            {"id": 4242424242}
        ]
    })"));

    // Order is kept, repeats and non-positive ids are dropped, and a large
    // sticker id must not be truncated.
    QCOMPARE(state.recentStickerIds.size(), 3);
    QCOMPARE(state.recentStickerIds.at(0), qint64(10));
    QCOMPARE(state.recentStickerIds.at(1), qint64(20));
    QCOMPARE(state.recentStickerIds.at(2), qint64(4242424242LL));
    QVERIFY(state.installedSets.isEmpty());
    QVERIFY(state.featuredSets.isEmpty());
}

void StickerCatalogTest::fallsBackToCoverPreview()
{
    const StickerCatalogState state = stickerCatalogFromResponse(parseObject(R"({
        "@type": "stickerSets",
        "sets": [
            {
                "id": 1,
                "thumbnail": {"file": {"id": 0}},
                "covers": [
                    {"id": 10, "thumbnail": {"file": {"id": 55}}, "sticker": {"id": 66}}
                ]
            },
            {
                "id": 2,
                "covers": [
                    {"id": 11, "sticker": {"id": 77}}
                ]
            },
            {
                "id": 3,
                "covers": [null, 5, {"id": 12}]
            }
        ]
    })"));

    QCOMPARE(state.installedSets.size(), 3);
    QCOMPARE(state.installedSets.at(0).thumbnailFileId, 55);
    QCOMPARE(state.installedSets.at(1).thumbnailFileId, 77);
    QCOMPARE(state.installedSets.at(2).thumbnailFileId, 0);
}

void StickerCatalogTest::readsStickerCountFromEitherField()
{
    const StickerCatalogState state = stickerCatalogFromResponse(parseObject(R"({
        "@type": "stickerSets",
        "sets": [
            {"id": 1, "sticker_count": 12, "size": 99},
            {"id": 2, "size": 8},
            {"id": 3, "size": -4},
            {"id": 4}
        ]
    })"));

    QCOMPARE(state.installedSets.size(), 4);
    QCOMPARE(state.installedSets.at(0).stickerCount, 12);
    QCOMPARE(state.installedSets.at(1).stickerCount, 8);
    QCOMPARE(state.installedSets.at(2).stickerCount, 0);
    QCOMPARE(state.installedSets.at(3).stickerCount, 0);
}

void StickerCatalogTest::ignoresMalformedResponses()
{
    const QVector<QJsonObject> responses = {
        QJsonObject{},
        parseObject(R"({"@type": "ok"})"),
        parseObject(R"({"@type": "error", "code": 400, "message": "bad"})"),
        parseObject(R"({"@type": "stickerSet"})"),
        parseObject(R"({"@type": "stickerSets"})"),
        parseObject(R"({"@type": "stickerSets", "sets": {}})"),
        parseObject(R"({"@type": "trendingStickerSets", "sets": null})"),
        parseObject(R"({"@type": "stickers"})"),
        parseObject(R"({"@type": "stickers", "stickers": {}})"),
        parseObject(R"({"@type": "stickerSets", "stickers": [{"id": 1}]})"),
        parseObject(R"({"@type": "stickers", "sets": [{"id": 1}]})"),
    };

    for (const QJsonObject& response : responses) {
        const StickerCatalogState state = stickerCatalogFromResponse(response);
        QVERIFY(state.installedSets.isEmpty());
        QVERIFY(state.featuredSets.isEmpty());
        QVERIFY(state.recentStickerIds.isEmpty());
    }

    // Sets without a positive id are not addressable and are dropped.
    const StickerCatalogState filtered = stickerCatalogFromResponse(parseObject(R"({
        "@type": "stickerSets",
        "sets": [
            {"id": 0, "title": "zero"},
            {"id": -1, "title": "negative"},
            {"id": "abc", "title": "text"},
            {"title": "missing"},
            null,
            7
        ]
    })"));
    QVERIFY(filtered.installedSets.isEmpty());
}

void StickerCatalogTest::keepsLargeIdsAndRejectsOversizedFileIds()
{
    const StickerCatalogState state = stickerCatalogFromResponse(parseObject(R"({
        "@type": "stickerSets",
        "sets": [
            {
                "id": 4242424242,
                "title": "Big",
                "size": 4294967296,
                "thumbnail": {"file": {"id": 4294967296}}
            }
        ]
    })"));

    QCOMPARE(state.installedSets.size(), 1);
    const StickerSetInfo& set = state.installedSets.at(0);
    QCOMPARE(set.id, qint64(4242424242LL));
    // File ids are int32 in TDLib, so an out-of-range id means "no preview".
    QCOMPARE(set.thumbnailFileId, 0);
    // Counts saturate at INT_MAX instead of wrapping negative.
    QCOMPARE(set.stickerCount, std::numeric_limits<int>::max());
}

QTEST_GUILESS_MAIN(StickerCatalogTest)
#include "tst_sticker_catalog.moc"
