#include "MessageEffects.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

namespace beygram::telegram {
namespace {

// Reads one catalog entry. An entry without a positive id is rejected: TDLib
// uses 0 for "no effect", so such an entry could never be sent back.
bool effectFromEntry(const QJsonValue& value, MessageEffect& effect)
{
    if (!value.isObject())
        return false;

    const QJsonObject entry = value.toObject();
    const qint64 id = jsonInt64(entry.value(QStringLiteral("id")));
    if (id <= 0)
        return false;

    effect.id = id;
    // TDLib may pad a catalog title or emoji with stray whitespace; the UI
    // renders the trimmed value, so the parser normalizes it once.
    effect.emoji = entry.value(QStringLiteral("emoji")).toString().trimmed();
    effect.title = entry.value(QStringLiteral("title")).toString().trimmed();
    // A missing or non-boolean is_premium leaves the effect usable; only an
    // explicit true marks it as Premium-only.
    effect.premiumOnly = entry.value(QStringLiteral("is_premium")).toBool();
    return true;
}

} // namespace

QJsonObject getMessageAvailableEffectsRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMessageAvailableEffects")},
    };
}

QJsonObject getMessageEffectRequest(qint64 effectId)
{
    if (effectId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMessageEffect")},
        {QStringLiteral("effect_id"), effectId},
    };
}

QVector<MessageEffect> messageEffectsFromResponse(const QJsonObject& response)
{
    QVector<MessageEffect> effects;

    const QJsonValue effectsValue = response.value(QStringLiteral("effects"));
    if (!effectsValue.isArray())
        return effects;

    const QJsonArray entries = effectsValue.toArray();
    effects.reserve(entries.size());

    // Duplicates keep the first entry and the catalog order; a set keeps the
    // lookup cheap for catalogs that repeat an id at the end of the list.
    QSet<qint64> seenIds;
    for (const QJsonValue& entry : entries) {
        MessageEffect effect;
        if (!effectFromEntry(entry, effect))
            continue;
        if (seenIds.contains(effect.id))
            continue;
        seenIds.insert(effect.id);
        effects.append(effect);
    }

    return effects;
}

QJsonObject messageEffectObject(qint64 effectId)
{
    if (effectId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageEffect")},
        {QStringLiteral("id"), effectId},
    };
}

} // namespace beygram::telegram
