#include "telegram/PremiumRequests.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact))
        .object();
}

QJsonObject purposeOf(const QString& purpose)
{
    return serialized(canPurchasePremiumRequest(purpose))
        .value(QStringLiteral("purpose"))
        .toObject();
}

QString typeOf(const QJsonObject& object)
{
    return object.value(QStringLiteral("@type")).toString();
}

QStringList titlesOf(const QVector<PremiumFeature>& features)
{
    QStringList titles;
    titles.reserve(features.size());
    for (const PremiumFeature& feature : features)
        titles.append(feature.title);
    return titles;
}

} // namespace

class PremiumRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsPremiumPromoRequest();
    void buildsPremiumFeaturesRequest();
    void mapsSettingsPurpose();
    void mapsLinkPurpose();
    void mapsFeaturePurpose();
    void mapsLimitAndStoryPurpose();
    void mapsBusinessPurpose();
    void keepsUnknownPurposeAsReferrer();
    void parsesFeatureList();
    void parsesFeatureOverrides();
    void handlesMalformedFeatureLists();
};

void PremiumRequestsTest::buildsPremiumPromoRequest()
{
    const QJsonObject request = serialized(getPremiumPromoRequest());

    QCOMPARE(typeOf(request), QStringLiteral("getPremiumPromo"));
    QCOMPARE(request.size(), qsizetype(1));
}

void PremiumRequestsTest::buildsPremiumFeaturesRequest()
{
    const QJsonObject request = serialized(getPremiumFeaturesRequest());

    QCOMPARE(typeOf(request), QStringLiteral("getPremiumFeatures"));
    QCOMPARE(request.size(), qsizetype(2));

    // source is required by the schema, so the parameter-less builder always
    // sends the settings source.
    const QJsonValue sourceValue = request.value(QStringLiteral("source"));
    QVERIFY(sourceValue.isObject());
    const QJsonObject source = sourceValue.toObject();
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceSettings"));
    QCOMPARE(source.size(), qsizetype(1));
}

void PremiumRequestsTest::mapsSettingsPurpose()
{
    const QJsonObject request = serialized(canPurchasePremiumRequest(QString()));
    QCOMPARE(typeOf(request), QStringLiteral("canPurchasePremium"));
    QCOMPARE(request.size(), qsizetype(2));
    QVERIFY(request.value(QStringLiteral("purpose")).isObject());

    // A source that needs a feature or limit name cannot be expressed without
    // one, so those prefixes degrade to the settings source.
    const QStringList purposes{
        QString(),
        QStringLiteral("   "),
        QStringLiteral("settings"),
        QStringLiteral("  Settings  "),
        QStringLiteral("feature:"),
        QStringLiteral("limit:"),
        QStringLiteral("story:"),
    };

    for (const QString& purpose : purposes) {
        const QJsonObject source = purposeOf(purpose);
        QCOMPARE(typeOf(source), QStringLiteral("premiumSourceSettings"));
        QCOMPARE(source.size(), qsizetype(1));
    }
}

void PremiumRequestsTest::mapsLinkPurpose()
{
    QJsonObject source = purposeOf(QStringLiteral("link"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceLink"));
    QVERIFY(source.value(QStringLiteral("referrer")).isString());
    QVERIFY(source.value(QStringLiteral("referrer")).toString().isEmpty());

    source = purposeOf(QStringLiteral("link:tg_premium"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceLink"));
    QCOMPARE(source.value(QStringLiteral("referrer")).toString(),
             QStringLiteral("tg_premium"));

    // The keyword is case-insensitive; the referrer keeps its own spelling.
    source = purposeOf(QStringLiteral("LINK:https://t.me/premium"));
    QCOMPARE(source.value(QStringLiteral("referrer")).toString(),
             QStringLiteral("https://t.me/premium"));

    source = purposeOf(QStringLiteral("  link:  ref with spaces  "));
    QCOMPARE(source.value(QStringLiteral("referrer")).toString(),
             QStringLiteral("ref with spaces"));
}

void PremiumRequestsTest::mapsFeaturePurpose()
{
    QJsonObject source = purposeOf(QStringLiteral("feature:premiumFeatureAccentColor"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceFeature"));
    QCOMPARE(source.size(), qsizetype(2));
    QCOMPARE(typeOf(source.value(QStringLiteral("feature")).toObject()),
             QStringLiteral("premiumFeatureAccentColor"));

    // A bare class name is the feature itself.
    source = purposeOf(QStringLiteral("premiumFeatureDisabledAds"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceFeature"));
    QCOMPARE(typeOf(source.value(QStringLiteral("feature")).toObject()),
             QStringLiteral("premiumFeatureDisabledAds"));

    source = purposeOf(QStringLiteral("FEATURE:premiumFeatureBusiness"));
    QCOMPARE(typeOf(source.value(QStringLiteral("feature")).toObject()),
             QStringLiteral("premiumFeatureBusiness"));
}

void PremiumRequestsTest::mapsLimitAndStoryPurpose()
{
    QJsonObject source = purposeOf(QStringLiteral("limit:premiumLimitTypePinnedChatCount"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceLimitExceeded"));
    QCOMPARE(source.size(), qsizetype(2));
    QCOMPARE(typeOf(source.value(QStringLiteral("limit_type")).toObject()),
             QStringLiteral("premiumLimitTypePinnedChatCount"));
    QVERIFY(!source.contains(QStringLiteral("feature")));

    source = purposeOf(QStringLiteral("story:premiumStoryFeatureStealthMode"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceStoryFeature"));
    QCOMPARE(typeOf(source.value(QStringLiteral("feature")).toObject()),
             QStringLiteral("premiumStoryFeatureStealthMode"));
}

void PremiumRequestsTest::mapsBusinessPurpose()
{
    // premiumSourceBusinessFeature is the only source that accepts a null
    // feature, for "the screen was opened without one specific feature".
    QJsonObject source = purposeOf(QStringLiteral("business"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceBusinessFeature"));
    QCOMPARE(source.size(), qsizetype(2));
    QVERIFY(source.contains(QStringLiteral("feature")));
    QVERIFY(source.value(QStringLiteral("feature")).isNull());

    source = purposeOf(QStringLiteral("business:businessFeatureLocation"));
    QCOMPARE(typeOf(source.value(QStringLiteral("feature")).toObject()),
             QStringLiteral("businessFeatureLocation"));
}

void PremiumRequestsTest::keepsUnknownPurposeAsReferrer()
{
    QJsonObject source = purposeOf(QStringLiteral("  profile_badge  "));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceLink"));
    QCOMPARE(source.value(QStringLiteral("referrer")).toString(),
             QStringLiteral("profile_badge"));

    // A URL is not a keyword, so it survives whole as the referrer.
    source = purposeOf(QStringLiteral("https://t.me/premium"));
    QCOMPARE(typeOf(source), QStringLiteral("premiumSourceLink"));
    QCOMPARE(source.value(QStringLiteral("referrer")).toString(),
             QStringLiteral("https://t.me/premium"));
}

void PremiumRequestsTest::parsesFeatureList()
{
    const QJsonObject response = QJsonDocument::fromJson(R"({
        "@type": "premiumFeatures",
        "features": [
            { "@type": "premiumFeatureIncreasedLimits" },
            { "@type": "premiumFeatureDisabledAds" },
            { "@type": "premiumFeatureRealTimeChatTranslation" },
            { "@type": "premiumFeatureBusiness" }
        ],
        "limits": [
            {
                "@type": "premiumLimit",
                "type": { "@type": "premiumLimitTypeSupergroupCount" },
                "default_value": 10,
                "premium_value": 20
            }
        ],
        "payment_link": {
            "@type": "internalLinkTypePremiumFeaturesPage",
            "referrer": ""
        }
    })")
                                        .object();

    const QVector<PremiumFeature> features = featuresFromResponse(response);
    QCOMPARE(features.size(), qsizetype(4));
    const QStringList expectedTitles{QStringLiteral("Increased Limits"),
                                     QStringLiteral("Disabled Ads"),
                                     QStringLiteral("Real Time Chat Translation"),
                                     QStringLiteral("Business")};
    QCOMPARE(titlesOf(features), expectedTitles);

    // Limits and the payment link are separate lists and stay out of features.
    for (const PremiumFeature& feature : features) {
        QVERIFY(feature.description.isEmpty());
        QVERIFY(feature.available);
    }
}

void PremiumRequestsTest::parsesFeatureOverrides()
{
    const QJsonObject response = QJsonDocument::fromJson(R"({
        "features": [
            {
                "@type": "premiumFeatureVoiceRecognition",
                "title": "  Voice to text  ",
                "description": " Convert voice notes ",
                "is_available": false
            },
            {
                "@type": "premiumFeatureCustomEmoji",
                "description": "Custom emoji",
                "available": true
            },
            { "@type": "premiumFeatureChecklists", "available": "yes" },
            {
                "@type": "premiumFeaturePaidMessages",
                "is_available": true,
                "available": false
            },
            { "@type": "premiumFeatureSavedMessagesTags", "is_available": 1 },
            { "title": "Server copy" }
        ]
    })")
                                        .object();

    const QVector<PremiumFeature> features = featuresFromResponse(response);
    QCOMPARE(features.size(), qsizetype(6));

    QCOMPARE(features.at(0).title, QStringLiteral("Voice to text"));
    QCOMPARE(features.at(0).description, QStringLiteral("Convert voice notes"));
    QVERIFY(!features.at(0).available);

    QCOMPARE(features.at(1).title, QStringLiteral("Custom Emoji"));
    QCOMPARE(features.at(1).description, QStringLiteral("Custom emoji"));
    QVERIFY(features.at(1).available);

    // A non-boolean availability is ignored, not read as false.
    QCOMPARE(features.at(2).title, QStringLiteral("Checklists"));
    QVERIFY(features.at(2).available);

    // is_available wins over the alias when both are present.
    QCOMPARE(features.at(3).title, QStringLiteral("Paid Messages"));
    QVERIFY(features.at(3).available);

    QCOMPARE(features.at(4).title, QStringLiteral("Saved Messages Tags"));
    QVERIFY(features.at(4).available);

    // An entry without a class name keeps the title the server sent.
    QCOMPARE(features.at(5).title, QStringLiteral("Server copy"));
    QVERIFY(features.at(5).description.isEmpty());
    QVERIFY(features.at(5).available);
}

void PremiumRequestsTest::handlesMalformedFeatureLists()
{
    QVERIFY(featuresFromResponse(QJsonObject{}).isEmpty());

    const auto parse = [](const char* json) {
        return featuresFromResponse(QJsonDocument::fromJson(json).object());
    };

    QVERIFY(parse(R"({ "features": null })").isEmpty());
    QVERIFY(parse(R"({ "features": "premiumFeatureDisabledAds" })").isEmpty());
    QVERIFY(parse(R"({ "features": {} })").isEmpty());
    QVERIFY(parse(R"({ "features": [ null, 5, true, {}, { "title": "   " },
                                   { "@type": "" }, { "description": "orphan" } ] })")
                .isEmpty());
    QVERIFY(parse(R"({ "features": [ { "@type": "premiumFeature" } ] })").isEmpty());

    // A bare type name still renders; an empty one has nothing to show.
    const QVector<PremiumFeature> legacy =
        parse(R"({ "features": [ "premiumFeatureEmojiStatus", "" ] })");
    QCOMPARE(legacy.size(), qsizetype(1));
    QCOMPARE(legacy.at(0).title, QStringLiteral("Emoji Status"));
    QVERIFY(legacy.at(0).available);

    // Order and repeats are TDLib's; the parser keeps both.
    const QVector<PremiumFeature> repeated =
        parse(R"({ "features": [ { "@type": "premiumFeatureAccentColor" },
                                 { "@type": "premiumFeatureAccentColor" } ] })");
    QCOMPARE(repeated.size(), qsizetype(2));
    QCOMPARE(repeated.at(0).title, QStringLiteral("Accent Color"));
    QCOMPARE(repeated.at(1).title, QStringLiteral("Accent Color"));
}

QTEST_GUILESS_MAIN(PremiumRequestsTest)
#include "tst_premium_requests.moc"
