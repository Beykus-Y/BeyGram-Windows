#include "telegram/MessageEffects.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject responseFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class MessageEffectsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsAvailableEffectsRequest();
    void buildsEffectRequest();
    void rejectsNonPositiveEffectIds();
    void buildsMessageEffectObject();
    void parsesEffectCatalog();
    void trimsEmojiAndTitle();
    void skipsMalformedEntries();
    void dedupesByIdKeepingFirst();
    void ignoresMissingEffectsArray();
    void readsStringEncodedIds();
    void preservesLargeIdsThroughJson();
};

void MessageEffectsTest::buildsAvailableEffectsRequest()
{
    const QJsonObject request = getMessageAvailableEffectsRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMessageAvailableEffects"));
    QCOMPARE(request.size(), 1);

    const QJsonObject encoded = serialized(request);
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMessageAvailableEffects"));
    QCOMPARE(encoded.size(), 1);
}

void MessageEffectsTest::buildsEffectRequest()
{
    const QJsonObject request = getMessageEffectRequest(42);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getMessageEffect"));
    QCOMPARE(numberAt(request, QStringLiteral("effect_id")), qint64(42));
    QCOMPARE(request.size(), 2);

    // The schema names the argument effect_id, never id.
    QVERIFY(!request.contains(QStringLiteral("id")));

    const QJsonObject encoded = serialized(request);
    QCOMPARE(numberAt(encoded, QStringLiteral("effect_id")), qint64(42));
}

void MessageEffectsTest::rejectsNonPositiveEffectIds()
{
    // 0 is TDLib's "no effect" value, so neither builder may name it.
    QVERIFY(getMessageEffectRequest(0).isEmpty());
    QVERIFY(getMessageEffectRequest(-1).isEmpty());
    QVERIFY(getMessageEffectRequest(std::numeric_limits<qint64>::min())
                .isEmpty());
    QVERIFY(messageEffectObject(0).isEmpty());
    QVERIFY(messageEffectObject(-7).isEmpty());
    QVERIFY(messageEffectObject(std::numeric_limits<qint64>::min())
                .isEmpty());
}

void MessageEffectsTest::buildsMessageEffectObject()
{
    const QJsonObject effect = messageEffectObject(555);
    QCOMPARE(effect.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageEffect"));
    QCOMPARE(numberAt(effect, QStringLiteral("id")), qint64(555));
    QCOMPARE(effect.size(), 2);

    const QJsonObject encoded = serialized(effect);
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageEffect"));
    QCOMPARE(numberAt(encoded, QStringLiteral("id")), qint64(555));
}

void MessageEffectsTest::parsesEffectCatalog()
{
    const QJsonObject response = responseFromJson(R"({
        "@type": "messageAvailableEffects",
        "effects": [
            {
                "@type": "availableEffect",
                "id": 5100000000001,
                "emoji": "🎉",
                "title": "Party",
                "is_premium": false
            },
            {
                "@type": "availableEffect",
                "id": 5100000000002,
                "emoji": "❤️",
                "title": "Heart",
                "is_premium": true
            },
            {
                "@type": "availableEffect",
                "id": 7,
                "emoji": "🔥",
                "title": "Fire"
            }
        ]
    })");

    const QVector<MessageEffect> effects = messageEffectsFromResponse(response);
    QCOMPARE(effects.size(), 3);

    QCOMPARE(effects.at(0).id, qint64(5100000000001LL));
    QCOMPARE(effects.at(0).emoji, QStringLiteral("🎉"));
    QCOMPARE(effects.at(0).title, QStringLiteral("Party"));
    QCOMPARE(effects.at(0).premiumOnly, false);

    QCOMPARE(effects.at(1).id, qint64(5100000000002LL));
    QCOMPARE(effects.at(1).emoji, QStringLiteral("❤️"));
    QCOMPARE(effects.at(1).title, QStringLiteral("Heart"));
    QCOMPARE(effects.at(1).premiumOnly, true);

    // A missing is_premium is not Premium-only.
    QCOMPARE(effects.at(2).id, qint64(7));
    QCOMPARE(effects.at(2).premiumOnly, false);
}

void MessageEffectsTest::trimsEmojiAndTitle()
{
    const QJsonObject response = responseFromJson(R"({
        "effects": [
            { "id": 1, "emoji": "  🎉 \n", "title": "\t Party \r\n" },
            { "id": 2, "emoji": "   ", "title": "" },
            { "id": 3, "title": "  " }
        ]
    })");

    const QVector<MessageEffect> effects = messageEffectsFromResponse(response);
    QCOMPARE(effects.size(), 3);

    QCOMPARE(effects.at(0).emoji, QStringLiteral("🎉"));
    QCOMPARE(effects.at(0).title, QStringLiteral("Party"));
    QVERIFY(effects.at(1).emoji.isEmpty());
    QVERIFY(effects.at(1).title.isEmpty());
    QVERIFY(effects.at(2).emoji.isEmpty());
    QVERIFY(effects.at(2).title.isEmpty());
}

void MessageEffectsTest::skipsMalformedEntries()
{
    const QJsonObject malformed = responseFromJson(R"({
        "effects": [
            null,
            5,
            "effect",
            [],
            {},
            { "id": 0 },
            { "id": -5 },
            { "id": "0" },
            { "id": null }
        ]
    })");
    QVERIFY(messageEffectsFromResponse(malformed).isEmpty());

    // A malformed neighbour never discards the rest of the catalog.
    const QJsonObject mixed = responseFromJson(R"({
        "effects": [
            { "id": 11, "title": "First" },
            null,
            { "id": 0 },
            { "id": 12, "title": "Second" }
        ]
    })");
    const QVector<MessageEffect> effects = messageEffectsFromResponse(mixed);
    QCOMPARE(effects.size(), 2);
    QCOMPARE(effects.at(0).id, qint64(11));
    QCOMPARE(effects.at(1).id, qint64(12));
}

void MessageEffectsTest::dedupesByIdKeepingFirst()
{
    const QJsonObject response = responseFromJson(R"({
        "effects": [
            { "id": 7, "emoji": "🎉", "title": "First", "is_premium": false },
            { "id": 7, "emoji": "❤️", "title": "Second", "is_premium": true },
            { "id": 8, "emoji": "🔥", "title": "Third" },
            { "id": 7, "emoji": "✨", "title": "Fourth" }
        ]
    })");

    const QVector<MessageEffect> effects = messageEffectsFromResponse(response);
    QCOMPARE(effects.size(), 2);

    // The first copy wins and the catalog order is kept.
    QCOMPARE(effects.at(0).id, qint64(7));
    QCOMPARE(effects.at(0).emoji, QStringLiteral("🎉"));
    QCOMPARE(effects.at(0).title, QStringLiteral("First"));
    QCOMPARE(effects.at(0).premiumOnly, false);
    QCOMPARE(effects.at(1).id, qint64(8));
    QCOMPARE(effects.at(1).title, QStringLiteral("Third"));
}

void MessageEffectsTest::ignoresMissingEffectsArray()
{
    QVERIFY(messageEffectsFromResponse(QJsonObject()).isEmpty());
    QVERIFY(messageEffectsFromResponse(responseFromJson(R"({ "effects": null })"))
                .isEmpty());
    QVERIFY(messageEffectsFromResponse(responseFromJson(R"({ "effects": {} })"))
                .isEmpty());
    QVERIFY(messageEffectsFromResponse(
                responseFromJson(R"({ "effects": "effect" })"))
                .isEmpty());
    QVERIFY(messageEffectsFromResponse(responseFromJson(R"({ "effects": [] })"))
                .isEmpty());
    QVERIFY(messageEffectsFromResponse(responseFromJson(R"({
        "@type": "error",
        "code": 404,
        "message": "Not Found"
    })")).isEmpty());
}

void MessageEffectsTest::readsStringEncodedIds()
{
    // tdjson can hand int64 values over as strings to preserve precision, so
    // the parser accepts both encodings.
    const QJsonObject response = responseFromJson(R"({
        "effects": [
            {
                "id": "1234567890123",
                "emoji": "🎉",
                "title": "Party",
                "is_premium": true
            }
        ]
    })");

    const QVector<MessageEffect> effects = messageEffectsFromResponse(response);
    QCOMPARE(effects.size(), 1);
    QCOMPARE(effects.at(0).id, qint64(1234567890123LL));
    QCOMPARE(effects.at(0).title, QStringLiteral("Party"));
    QCOMPARE(effects.at(0).premiumOnly, true);
}

void MessageEffectsTest::preservesLargeIdsThroughJson()
{
    const qint64 effectId = 1234567890124LL;

    const QJsonObject request = serialized(getMessageEffectRequest(effectId));
    QCOMPARE(numberAt(request, QStringLiteral("effect_id")), effectId);

    const QJsonObject sendEffect = serialized(messageEffectObject(effectId));
    QCOMPARE(numberAt(sendEffect, QStringLiteral("id")), effectId);

    const QJsonObject response = serialized(responseFromJson(R"({
        "effects": [
            { "id": 1234567890124, "emoji": "🎉", "title": "Party" }
        ]
    })"));
    const QVector<MessageEffect> effects = messageEffectsFromResponse(response);
    QCOMPARE(effects.size(), 1);
    QCOMPARE(effects.at(0).id, effectId);
}

QTEST_GUILESS_MAIN(MessageEffectsTest)
#include "tst_message_effects.moc"
