#include "telegram/BackgroundCatalogModel.h"
#include "telegram/ChatBackgrounds.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to the transport.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

BackgroundInfo background(qint64 id, const QString& name = {},
                          bool isDefault = false, bool isDark = false,
                          int fileId = 0)
{
    return BackgroundInfo{id, name, isDefault, isDark, fileId};
}

QVector<qint64> idsOf(const QVector<BackgroundInfo>& backgrounds)
{
    QVector<qint64> ids;
    ids.reserve(backgrounds.size());
    for (const BackgroundInfo& item : backgrounds)
        ids.append(item.id);
    return ids;
}

} // namespace

class ChatBackgroundsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetBackgroundsRequest();
    void buildsBackgroundUrlRequest();
    void acceptsEveryBackgroundType();
    void canonicalizesTypeSpelling();
    void rejectsInvalidBackgroundUrlRequest();

    void buildsSetBackgroundRequest();
    void rejectsNonPositiveBackgroundId();
    void buildsSetBackgroundTypeRequest();
    void rejectsInvalidBackgroundType();
    void requestsSurviveSerialization();

    void parsesBackground();
    void parsesBackgroundWithoutDocument();
    void parsesStringIdsAndFileIds();
    void rejectsMalformedBackgrounds();
    void keepsLargeIdsAndRejectsOversizedFileIds();
    void ignoresWrongTypedOptionalFields();
    void trimsNameToEmptyWithoutDroppingId();

    void startsEmpty();

    void setBackgroundsKeepsIncomingOrder();
    void setBackgroundsDedupesByIdKeepingFirst();
    void setBackgroundsDropsMalformedIds();
    void setBackgroundsTrimsNames();
    void setBackgroundsReplacesPreviousCatalog();

    void defaultBackgroundReturnsFirstDefault();
    void defaultBackgroundWithoutDefaultIsEmpty();

    void findReturnsStoredBackground();
    void findOfMissingIdReturnsDefault();
    void findOfNonPositiveIdReturnsDefault();

    void clearDropsCatalog();
    void clearOnEmptyIsNoOp();

    void emitsChangedOnRealSetBackgrounds();
    void emitsChangedOnlyOncePerClear();
    void returnsIndependentCopies();
};

void ChatBackgroundsTest::buildsGetBackgroundsRequest()
{
    const QJsonObject dark = getBackgroundsRequest(true);
    QCOMPARE(dark.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getBackgrounds"));
    QCOMPARE(dark.value(QStringLiteral("for_dark_theme")).toBool(), true);
    QCOMPARE(dark.size(), 2);

    const QJsonObject light = getBackgroundsRequest(false);
    QCOMPARE(light.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getBackgrounds"));
    QCOMPARE(light.value(QStringLiteral("for_dark_theme")).toBool(), false);
    QCOMPARE(light.size(), 2);
}

void ChatBackgroundsTest::buildsBackgroundUrlRequest()
{
    const QJsonObject request =
        getBackgroundUrlRequest(QStringLiteral("  sunset  "),
                                QStringLiteral("Wallpaper"));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getBackgroundUrl"));
    // Names are addressable only in their trimmed form.
    QCOMPARE(request.value(QStringLiteral("name")).toString(),
             QStringLiteral("sunset"));
    QCOMPARE(request.value(QStringLiteral("type")).toString(),
             QStringLiteral("wallpaper"));
    QCOMPARE(request.size(), 3);
}

void ChatBackgroundsTest::acceptsEveryBackgroundType()
{
    const QVector<QString> types = {
        QStringLiteral("wallpaper"),
        QStringLiteral("pattern"),
        QStringLiteral("solid"),
        QStringLiteral("gradient"),
    };

    for (const QString& type : types) {
        const QJsonObject request =
            getBackgroundUrlRequest(QStringLiteral("bg"), type);
        QCOMPARE(request.value(QStringLiteral("type")).toString(), type);
        QVERIFY(!request.isEmpty());
    }
}

void ChatBackgroundsTest::canonicalizesTypeSpelling()
{
    // The renderer compares lower-case kind names, so a caller may pass the
    // user-visible spelling with any case and surrounding spaces.
    const QJsonObject padded =
        getBackgroundUrlRequest(QStringLiteral("bg"), QStringLiteral("  GRADIENT  "));
    QCOMPARE(padded.value(QStringLiteral("type")).toString(),
             QStringLiteral("gradient"));

    const QJsonObject mixed =
        getBackgroundUrlRequest(QStringLiteral("bg"), QStringLiteral("PaTtErN"));
    QCOMPARE(mixed.value(QStringLiteral("type")).toString(),
             QStringLiteral("pattern"));
}

void ChatBackgroundsTest::rejectsInvalidBackgroundUrlRequest()
{
    const QVector<QString> invalidTypes = {
        QString(),
        QStringLiteral("   "),
        QStringLiteral("fill"),
        QStringLiteral("chatTheme"),
        QStringLiteral("none"),
        QStringLiteral("photo"),
    };

    for (const QString& type : invalidTypes) {
        QVERIFY(getBackgroundUrlRequest(QStringLiteral("bg"), type).isEmpty());
    }

    // A blank name is not addressable, whatever the type.
    QVERIFY(getBackgroundUrlRequest(QString(), QStringLiteral("wallpaper")).isEmpty());
    QVERIFY(getBackgroundUrlRequest(QStringLiteral("   "),
                                    QStringLiteral("wallpaper")).isEmpty());
}

void ChatBackgroundsTest::buildsSetBackgroundRequest()
{
    const QJsonObject global = setBackgroundRequest(77, 0);
    QCOMPARE(global.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setBackground"));
    QCOMPARE(numberAt(global, QStringLiteral("background_id")), qint64(77));
    // 0 is the global scope, not a missing chat id.
    QCOMPARE(numberAt(global, QStringLiteral("chat_id")), qint64(0));
    QCOMPARE(global.size(), 3);

    // Group and channel ids are negative and must survive unchanged.
    const QJsonObject group = setBackgroundRequest(88, -1001234567890LL);
    QCOMPARE(numberAt(group, QStringLiteral("background_id")), qint64(88));
    QCOMPARE(numberAt(group, QStringLiteral("chat_id")), qint64(-1001234567890LL));

    const QJsonObject privateChat = setBackgroundRequest(99, 4242424242LL);
    QCOMPARE(numberAt(privateChat, QStringLiteral("chat_id")), qint64(4242424242LL));
}

void ChatBackgroundsTest::rejectsNonPositiveBackgroundId()
{
    // A background without a positive id cannot be selected, so no request is
    // produced for any chat scope.
    QVERIFY(setBackgroundRequest(0, 0).isEmpty());
    QVERIFY(setBackgroundRequest(-1, 0).isEmpty());
    QVERIFY(setBackgroundRequest(0, -1001234567890LL).isEmpty());
    QVERIFY(setBackgroundRequest(-42, 123).isEmpty());
}

void ChatBackgroundsTest::buildsSetBackgroundTypeRequest()
{
    const QJsonObject dark =
        setBackgroundTypeRequest(true, QStringLiteral("  SoLiD  "));
    QCOMPARE(dark.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setBackgroundType"));
    QCOMPARE(dark.value(QStringLiteral("for_dark_theme")).toBool(), true);
    QCOMPARE(dark.value(QStringLiteral("type")).toString(), QStringLiteral("solid"));
    QCOMPARE(dark.size(), 3);

    const QJsonObject light =
        setBackgroundTypeRequest(false, QStringLiteral("pattern"));
    QCOMPARE(light.value(QStringLiteral("for_dark_theme")).toBool(), false);
    QCOMPARE(light.value(QStringLiteral("type")).toString(), QStringLiteral("pattern"));
}

void ChatBackgroundsTest::rejectsInvalidBackgroundType()
{
    const QVector<QString> invalidTypes = {
        QString(),
        QStringLiteral("   "),
        QStringLiteral("fill"),
        QStringLiteral("none"),
        QStringLiteral("chatTheme"),
    };

    for (const QString& type : invalidTypes) {
        QVERIFY(setBackgroundTypeRequest(true, type).isEmpty());
        QVERIFY(setBackgroundTypeRequest(false, type).isEmpty());
    }
}

void ChatBackgroundsTest::requestsSurviveSerialization()
{
    const QJsonObject backgrounds = serialized(getBackgroundsRequest(true));
    QCOMPARE(backgrounds.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getBackgrounds"));
    QCOMPARE(backgrounds.value(QStringLiteral("for_dark_theme")).toBool(), true);

    const QJsonObject url =
        serialized(getBackgroundUrlRequest(QStringLiteral(" bg "),
                                           QStringLiteral("GRADIENT")));
    QCOMPARE(url.value(QStringLiteral("name")).toString(), QStringLiteral("bg"));
    QCOMPARE(url.value(QStringLiteral("type")).toString(), QStringLiteral("gradient"));

    const QJsonObject set = serialized(
        setBackgroundRequest(4242424242LL, -1001234567890LL));
    QCOMPARE(set.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setBackground"));
    QCOMPARE(numberAt(set, QStringLiteral("background_id")), qint64(4242424242LL));
    QCOMPARE(numberAt(set, QStringLiteral("chat_id")), qint64(-1001234567890LL));

    const QJsonObject type =
        serialized(setBackgroundTypeRequest(true, QStringLiteral("wallpaper")));
    QCOMPARE(type.value(QStringLiteral("type")).toString(), QStringLiteral("wallpaper"));
    QCOMPARE(type.value(QStringLiteral("for_dark_theme")).toBool(), true);
}

void ChatBackgroundsTest::parsesBackground()
{
    const BackgroundInfo info = backgroundFromJson(parseObject(R"({
        "@type": "background",
        "id": 1234567890123,
        "name": "  Sunset  ",
        "is_default": true,
        "is_dark": true,
        "document": {
            "file_name": "sunset.jpg",
            "file": {"id": 777}
        }
    })"));

    QCOMPARE(info.id, qint64(1234567890123LL));
    QCOMPARE(info.name, QStringLiteral("Sunset"));
    QCOMPARE(info.isDefault, true);
    QCOMPARE(info.isDark, true);
    QCOMPARE(info.fileId, 777);
}

void ChatBackgroundsTest::parsesBackgroundWithoutDocument()
{
    // Filled backgrounds have no document, so there is no file to download.
    const BackgroundInfo missing = backgroundFromJson(parseObject(R"({
        "id": 1,
        "name": "Plain"
    })"));
    QCOMPARE(missing.id, qint64(1));
    QCOMPARE(missing.fileId, 0);
    QCOMPARE(missing.isDefault, false);
    QCOMPARE(missing.isDark, false);

    const BackgroundInfo nullDocument = backgroundFromJson(parseObject(R"({
        "id": 2,
        "document": null
    })"));
    QCOMPARE(nullDocument.fileId, 0);

    const BackgroundInfo zeroFile = backgroundFromJson(parseObject(R"({
        "id": 3,
        "document": {"file": {"id": 0}}
    })"));
    QCOMPARE(zeroFile.fileId, 0);

    const BackgroundInfo negativeFile = backgroundFromJson(parseObject(R"({
        "id": 4,
        "document": {"file": {"id": -5}}
    })"));
    QCOMPARE(negativeFile.fileId, 0);
}

void ChatBackgroundsTest::parsesStringIdsAndFileIds()
{
    // The JSON interface sends int64 values as strings once they would lose
    // precision; the parser accepts both spellings.
    const BackgroundInfo info = backgroundFromJson(parseObject(R"({
        "id": "4242424242",
        "name": "Big",
        "document": {"file": {"id": "99"}}
    })"));

    QCOMPARE(info.id, qint64(4242424242LL));
    QCOMPARE(info.name, QStringLiteral("Big"));
    QCOMPARE(info.fileId, 99);
}

void ChatBackgroundsTest::rejectsMalformedBackgrounds()
{
    const QVector<QJsonObject> malformed = {
        QJsonObject{},
        parseObject(R"({"name": "No id"})"),
        parseObject(R"({"id": 0, "name": "Zero"})"),
        parseObject(R"({"id": -1, "name": "Negative"})"),
        parseObject(R"({"id": "abc", "name": "Text"})"),
        parseObject(R"({"id": null, "name": "Null"})"),
        parseObject(R"({"id": true, "name": "Bool"})"),
        parseObject(R"({"id": [], "name": "Array"})"),
    };

    for (const QJsonObject& object : malformed) {
        const BackgroundInfo info = backgroundFromJson(object);
        QCOMPARE(info.id, qint64(0));
        QVERIFY(info.name.isEmpty());
        QCOMPARE(info.fileId, 0);
    }

    // A valid id with a malformed document is still a usable background; only
    // the file is missing.
    const BackgroundInfo malformedDocument =
        backgroundFromJson(parseObject(R"({"id": 5, "document": []})"));
    QCOMPARE(malformedDocument.id, qint64(5));
    QCOMPARE(malformedDocument.fileId, 0);
}

void ChatBackgroundsTest::keepsLargeIdsAndRejectsOversizedFileIds()
{
    const BackgroundInfo info = backgroundFromJson(parseObject(R"({
        "id": 9007199254740991,
        "name": "Largest exact double",
        "document": {"file": {"id": 4294967296}}
    })"));

    QCOMPARE(info.id, qint64(9007199254740991LL));
    // File ids are int32 in the protocol, so an out-of-range id is not
    // truncated into a different file.
    QCOMPARE(info.fileId, 0);
}

void ChatBackgroundsTest::ignoresWrongTypedOptionalFields()
{
    const BackgroundInfo info = backgroundFromJson(parseObject(R"({
        "id": 7,
        "name": 42,
        "is_default": "true",
        "is_dark": 1,
        "document": "not an object"
    })"));

    QCOMPARE(info.id, qint64(7));
    QVERIFY(info.name.isEmpty());
    QCOMPARE(info.isDefault, false);
    QCOMPARE(info.isDark, false);
    QCOMPARE(info.fileId, 0);
}

void ChatBackgroundsTest::trimsNameToEmptyWithoutDroppingId()
{
    const BackgroundInfo info =
        backgroundFromJson(parseObject(R"({"id": 8, "name": "   "})"));

    // A blank name is still a usable entry; only the id is mandatory.
    QCOMPARE(info.id, qint64(8));
    QVERIFY(info.name.isEmpty());
}

void ChatBackgroundsTest::startsEmpty()
{
    BackgroundCatalogModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.backgrounds().isEmpty());
    QCOMPARE(model.defaultBackground().id, qint64(0));
    QCOMPARE(model.find(1).id, qint64(0));
}

void ChatBackgroundsTest::setBackgroundsKeepsIncomingOrder()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(30, QStringLiteral("Third")),
        background(10, QStringLiteral("First")),
        background(20, QStringLiteral("Second")),
    });

    QCOMPARE(model.count(), 3);
    QCOMPARE(idsOf(model.backgrounds()), QVector<qint64>({30, 10, 20}));
}

void ChatBackgroundsTest::setBackgroundsDedupesByIdKeepingFirst()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(1, QStringLiteral("First"), true, false, 11),
        background(2, QStringLiteral("Second"), false, true, 22),
        background(1, QStringLiteral("Duplicate"), false, true, 99),
        background(2, QStringLiteral("Third"), true, false, 33),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.backgrounds()), QVector<qint64>({1, 2}));

    const BackgroundInfo first = model.find(1);
    QCOMPARE(first.name, QStringLiteral("First"));
    QCOMPARE(first.isDefault, true);
    QCOMPARE(first.isDark, false);
    QCOMPARE(first.fileId, 11);

    const BackgroundInfo second = model.find(2);
    QCOMPARE(second.name, QStringLiteral("Second"));
    QCOMPARE(second.isDefault, false);
    QCOMPARE(second.isDark, true);
    QCOMPARE(second.fileId, 22);

    // The first row also wins when only the dropped duplicate was a default.
    BackgroundCatalogModel duplicateDefault;
    duplicateDefault.setBackgrounds({
        background(1, QStringLiteral("First")),
        background(1, QStringLiteral("Duplicate"), true),
    });
    QCOMPARE(duplicateDefault.count(), 1);
    QCOMPARE(duplicateDefault.defaultBackground().id, qint64(0));
}

void ChatBackgroundsTest::setBackgroundsDropsMalformedIds()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(0, QStringLiteral("Zero")),
        background(-3, QStringLiteral("Negative")),
        background(7, QStringLiteral("Kept")),
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.backgrounds()), QVector<qint64>({7}));
}

void ChatBackgroundsTest::setBackgroundsTrimsNames()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(1, QStringLiteral("  Bear  ")),
        background(2, QStringLiteral("   ")),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.find(1).name, QStringLiteral("Bear"));
    QVERIFY(model.find(2).name.isEmpty());
}

void ChatBackgroundsTest::setBackgroundsReplacesPreviousCatalog()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({background(1), background(2)});
    model.setBackgrounds({background(3)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.backgrounds()), QVector<qint64>({3}));
    QCOMPARE(model.find(1).id, qint64(0));
}

void ChatBackgroundsTest::defaultBackgroundReturnsFirstDefault()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(1, QStringLiteral("Regular")),
        background(2, QStringLiteral("Default"), true, false, 22),
        background(3, QStringLiteral("Another default"), true, true, 33),
    });

    const BackgroundInfo info = model.defaultBackground();
    QCOMPARE(info.id, qint64(2));
    QCOMPARE(info.name, QStringLiteral("Default"));
    QCOMPARE(info.isDefault, true);
    QCOMPARE(info.isDark, false);
    QCOMPARE(info.fileId, 22);
}

void ChatBackgroundsTest::defaultBackgroundWithoutDefaultIsEmpty()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({background(1), background(2)});

    const BackgroundInfo info = model.defaultBackground();
    QCOMPARE(info.id, qint64(0));
    QVERIFY(info.name.isEmpty());
    QCOMPARE(info.isDefault, false);
    QCOMPARE(info.isDark, false);
    QCOMPARE(info.fileId, 0);
}

void ChatBackgroundsTest::findReturnsStoredBackground()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(1, QStringLiteral("Bear"), false, true, 42),
        background(2, QStringLiteral("Panda"), true, false, 43),
    });

    const BackgroundInfo found = model.find(1);
    QCOMPARE(found.id, qint64(1));
    QCOMPARE(found.name, QStringLiteral("Bear"));
    QCOMPARE(found.isDefault, false);
    QCOMPARE(found.isDark, true);
    QCOMPARE(found.fileId, 42);
}

void ChatBackgroundsTest::findOfMissingIdReturnsDefault()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({background(1, QStringLiteral("Bear"))});

    const BackgroundInfo missing = model.find(99);
    QCOMPARE(missing.id, qint64(0));
    QVERIFY(missing.name.isEmpty());
    QCOMPARE(missing.fileId, 0);
}

void ChatBackgroundsTest::findOfNonPositiveIdReturnsDefault()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({background(1, QStringLiteral("Bear"))});

    // Filtered ids are not stored, so they read back as the default row.
    QCOMPARE(model.find(0).id, qint64(0));
    QCOMPARE(model.find(-1).id, qint64(0));
}

void ChatBackgroundsTest::clearDropsCatalog()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(1, QStringLiteral("Bear")),
        background(2, QStringLiteral("Panda"), true),
    });

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.backgrounds().isEmpty());
    QCOMPARE(model.defaultBackground().id, qint64(0));
    QCOMPARE(model.find(1).id, qint64(0));
}

void ChatBackgroundsTest::clearOnEmptyIsNoOp()
{
    BackgroundCatalogModel model;
    QSignalSpy spy(&model, &BackgroundCatalogModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void ChatBackgroundsTest::emitsChangedOnRealSetBackgrounds()
{
    BackgroundCatalogModel model;
    QSignalSpy spy(&model, &BackgroundCatalogModel::changed);

    // Already empty stays silent.
    model.setBackgrounds({});
    QCOMPARE(spy.count(), 0);

    model.setBackgrounds({background(1, QStringLiteral("Bear"), true, false, 11)});
    QCOMPARE(spy.count(), 1);

    // The same catalog again is not a change.
    model.setBackgrounds({background(1, QStringLiteral("Bear"), true, false, 11)});
    QCOMPARE(spy.count(), 1);

    // Normalization means an untrimmed name is still the same catalog.
    model.setBackgrounds({background(1, QStringLiteral("  Bear  "), true, false, 11)});
    QCOMPARE(spy.count(), 1);

    // A different name is a change.
    model.setBackgrounds({background(1, QStringLiteral("Panda"), true, false, 11)});
    QCOMPARE(spy.count(), 2);

    // Becoming non-default is a change.
    model.setBackgrounds({background(1, QStringLiteral("Panda"), false, false, 11)});
    QCOMPARE(spy.count(), 3);

    // Becoming dark is a change.
    model.setBackgrounds({background(1, QStringLiteral("Panda"), false, true, 11)});
    QCOMPARE(spy.count(), 4);

    // A different file is a change.
    model.setBackgrounds({background(1, QStringLiteral("Panda"), false, true, 12)});
    QCOMPARE(spy.count(), 5);

    // Adding a background is a change.
    model.setBackgrounds({background(1, QStringLiteral("Panda"), false, true, 12),
                          background(2, QStringLiteral("Fox"))});
    QCOMPARE(spy.count(), 6);

    // Reordering the same rows is a change: the picker shows a different order.
    model.setBackgrounds({background(2, QStringLiteral("Fox")),
                          background(1, QStringLiteral("Panda"), false, true, 12)});
    QCOMPARE(spy.count(), 7);

    // Dropping every background is a change.
    model.setBackgrounds({});
    QCOMPARE(spy.count(), 8);

    // Already empty stays silent.
    model.setBackgrounds({});
    QCOMPARE(spy.count(), 8);
}

void ChatBackgroundsTest::emitsChangedOnlyOncePerClear()
{
    BackgroundCatalogModel model;
    QSignalSpy spy(&model, &BackgroundCatalogModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setBackgrounds({background(1)});
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void ChatBackgroundsTest::returnsIndependentCopies()
{
    BackgroundCatalogModel model;
    model.setBackgrounds({
        background(1, QStringLiteral("Bear"), true, false, 11),
        background(2, QStringLiteral("Panda"), false, true, 22),
    });

    QVector<BackgroundInfo> snapshot = model.backgrounds();
    snapshot[0].name = QStringLiteral("mutated");
    snapshot.append(background(3));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.find(1).name, QStringLiteral("Bear"));

    BackgroundInfo defaultInfo = model.defaultBackground();
    defaultInfo.name = QStringLiteral("mutated");
    QCOMPARE(model.defaultBackground().name, QStringLiteral("Bear"));

    BackgroundInfo found = model.find(2);
    found.name = QStringLiteral("mutated");
    QCOMPARE(model.find(2).name, QStringLiteral("Panda"));
}

QTEST_GUILESS_MAIN(ChatBackgroundsTest)
#include "tst_chat_backgrounds.moc"
