#include "PremiumRequests.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

/// Every PremiumSource subtype TDLib accepts, built from its @type name.
QJsonObject settingsSource()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("premiumSourceSettings")},
    };
}

QJsonObject linkSource(const QString& referrer)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("premiumSourceLink")},
        {QStringLiteral("referrer"), referrer},
    };
}

QJsonObject featureSource(const QString& featureType)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("premiumSourceFeature")},
        {QStringLiteral("feature"),
         QJsonObject{{QStringLiteral("@type"), featureType}}},
    };
}

QJsonObject limitSource(const QString& limitType)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("premiumSourceLimitExceeded")},
        {QStringLiteral("limit_type"),
         QJsonObject{{QStringLiteral("@type"), limitType}}},
    };
}

/// premiumSourceBusinessFeature accepts a null feature when no single Business
/// feature triggered the screen; premiumSourceFeature does not, which is why
/// only this one takes a nullable value.
QJsonObject businessSource(const QJsonValue& feature)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("premiumSourceBusinessFeature")},
        {QStringLiteral("feature"), feature},
    };
}

QJsonObject storySource(const QString& featureType)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("premiumSourceStoryFeature")},
        {QStringLiteral("feature"),
         QJsonObject{{QStringLiteral("@type"), featureType}}},
    };
}

QJsonObject premiumSourceFor(const QString& purpose)
{
    const QString text = purpose.trimmed();

    // The token is "keyword" or "keyword:value". A purpose that carries no colon
    // is a keyword, which is also how a bare premiumFeature* type arrives.
    const qsizetype colon = text.indexOf(QLatin1Char(':'));
    const QString keyword = (colon < 0 ? text : text.left(colon)).trimmed().toLower();
    const QString value = colon < 0 ? QString() : text.mid(colon + 1).trimmed();

    if (keyword.isEmpty() || keyword == QLatin1String("settings"))
        return settingsSource();
    if (keyword == QLatin1String("link"))
        return linkSource(value);
    if (keyword == QLatin1String("feature"))
        return value.isEmpty() ? settingsSource() : featureSource(value);
    if (keyword == QLatin1String("limit"))
        return value.isEmpty() ? settingsSource() : limitSource(value);
    if (keyword == QLatin1String("story"))
        return value.isEmpty() ? settingsSource() : storySource(value);
    if (keyword == QLatin1String("business")) {
        if (value.isEmpty())
            return businessSource(QJsonValue(QJsonValue::Null));
        return businessSource(QJsonValue(QJsonObject{
            {QStringLiteral("@type"), value}}));
    }
    if (text.startsWith(QLatin1String("premiumFeature"), Qt::CaseInsensitive))
        return featureSource(text);

    // Anything else is kept as a referrer: TDLib stores it as a free-form
    // string, so the caller's token survives instead of being replaced.
    return linkSource(text);
}

/// Turns a TDLib class name into a label: "premiumFeatureDisabledAds" becomes
/// "Disabled Ads". An empty result means the name carried nothing to show.
QString labelFromTypeName(const QString& type)
{
    QString name = type;
    const QString prefix = QStringLiteral("premiumFeature");
    if (name.startsWith(prefix, Qt::CaseInsensitive))
        name.remove(0, prefix.size());
    if (name.isEmpty())
        return {};

    QString label;
    label.reserve(name.size() + 8);
    for (qsizetype index = 0; index < name.size(); ++index) {
        const QChar character = name.at(index);
        if (character.isUpper() && !label.isEmpty())
            label.append(QLatin1Char(' '));
        label.append(character);
    }
    label[0] = label.at(0).toUpper();
    return label;
}

/// The schema has no availability flag, so only an explicit boolean can deny a
/// feature; a string, a number or a missing field keeps the entry available.
bool availabilityOf(const QJsonObject& feature)
{
    const QJsonValue isAvailable = feature.value(QStringLiteral("is_available"));
    if (isAvailable.isBool())
        return isAvailable.toBool();

    const QJsonValue available = feature.value(QStringLiteral("available"));
    if (available.isBool())
        return available.toBool();

    return true;
}

} // namespace

QJsonObject getPremiumPromoRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getPremiumPromo")},
    };
}

QJsonObject getPremiumFeaturesRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getPremiumFeatures")},
        {QStringLiteral("source"), settingsSource()},
    };
}

QJsonObject canPurchasePremiumRequest(const QString& purpose)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("canPurchasePremium")},
        {QStringLiteral("purpose"), premiumSourceFor(purpose)},
    };
}

QVector<PremiumFeature> featuresFromResponse(const QJsonObject& response)
{
    QVector<PremiumFeature> features;

    const QJsonArray entries = response.value(QStringLiteral("features")).toArray();
    features.reserve(entries.size());

    for (const QJsonValue& entry : entries) {
        // A bare type name is accepted too, so a payload that lists the class
        // names instead of objects still renders.
        if (entry.isString()) {
            const QString title = labelFromTypeName(entry.toString());
            if (title.isEmpty())
                continue;
            PremiumFeature feature;
            feature.title = title;
            features.append(feature);
            continue;
        }

        if (!entry.isObject())
            continue;

        const QJsonObject object = entry.toObject();
        const QString type = object.value(QStringLiteral("@type")).toString();

        PremiumFeature feature;
        feature.title = object.value(QStringLiteral("title")).toString().trimmed();
        if (feature.title.isEmpty())
            feature.title = labelFromTypeName(type);
        if (feature.title.isEmpty())
            continue;

        feature.description =
            object.value(QStringLiteral("description")).toString().trimmed();
        feature.available = availabilityOf(object);
        features.append(feature);
    }

    return features;
}

} // namespace beygram::telegram
