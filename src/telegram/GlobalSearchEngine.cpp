#include "GlobalSearchEngine.h"

#include <QHash>
#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram::globalsearch {
namespace {

constexpr int kMinLimit = 1;
constexpr int kMaxLimit = 100;

int clampedLimit(int limit)
{
    return std::clamp(limit, kMinLimit, kMaxLimit);
}

/// The identity of one row inside its source. chatId and the source-specific
/// identity are kept separately so a message id and a user id can never
/// collide.
struct HitKey {
    SearchSource source = SearchSource::Messages;
    qint64 chatId = 0;
    qint64 identity = 0;

    bool operator==(const HitKey& other) const = default;
};

size_t qHash(const HitKey& key, size_t seed = 0) noexcept
{
    return qHashMulti(seed, static_cast<int>(key.source), key.chatId, key.identity);
}

HitKey keyFor(const SearchHit& hit)
{
    switch (hit.source) {
    case SearchSource::Chats:
        // A chat row is one chat; messageId/userId are not part of its key.
        return HitKey{hit.source, hit.chatId, 0};
    case SearchSource::Messages:
        // The same message id can exist in several chats.
        return HitKey{hit.source, hit.chatId, hit.messageId};
    case SearchSource::People:
        // A person is one user regardless of which chat surfaced them.
        return HitKey{hit.source, 0, hit.userId};
    }
    return HitKey{hit.source, hit.chatId, 0};
}

/// De-duplicates, ranks and caps one source. The result is sorted by score
/// descending, then date descending, then source order.
QVector<SearchHit> rankedSource(const QVector<SearchHit>& hits, int maxPerSource)
{
    QVector<SearchHit> kept;
    kept.reserve(hits.size());

    QHash<HitKey, qsizetype> positions;
    positions.reserve(hits.size());

    for (const SearchHit& hit : hits) {
        const HitKey key = keyFor(hit);
        const auto found = positions.constFind(key);
        if (found == positions.cend()) {
            positions.insert(key, kept.size());
            kept.append(hit);
            continue;
        }

        // The best-scoring copy wins. A tie keeps the first occurrence, which
        // also keeps the caller's order stable for equal ranks.
        SearchHit& current = kept[found.value()];
        if (hit.score > current.score)
            current = hit;
    }

    std::stable_sort(kept.begin(), kept.end(),
                     [](const SearchHit& left, const SearchHit& right) {
                         if (left.score != right.score)
                             return left.score > right.score;
                         if (left.date != right.date)
                             return left.date > right.date;
                         return static_cast<int>(left.source) < static_cast<int>(right.source);
                     });

    if (kept.size() > maxPerSource)
        kept.resize(maxPerSource);
    return kept;
}

} // namespace

QJsonObject searchChatsOnServerRequest(const QString& query, int limit)
{
    const QString text = query.trimmed();
    if (text.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchChatsOnServer")},
        {QStringLiteral("chat_list"), QJsonValue::Null},
        {QStringLiteral("query"), text},
        {QStringLiteral("limit"), clampedLimit(limit)},
    };
}

QJsonObject searchMessagesOnServerRequest(const QString& query, int limit)
{
    const QString text = query.trimmed();
    if (text.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchMessages")},
        {QStringLiteral("chat_list"), QJsonValue::Null},
        {QStringLiteral("query"), text},
        {QStringLiteral("offset"), QString()},
        {QStringLiteral("limit"), clampedLimit(limit)},
        {QStringLiteral("filter"), QJsonValue::Null},
        {QStringLiteral("chat_type_filter"), QJsonValue::Null},
        {QStringLiteral("min_date"), 0},
        {QStringLiteral("max_date"), 0},
    };
}

QJsonObject searchContactsRequest(const QString& query, int limit)
{
    const QString text = query.trimmed();
    if (text.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchContacts")},
        {QStringLiteral("query"), text},
        {QStringLiteral("limit"), clampedLimit(limit)},
    };
}

QVector<SearchHit> mergeSearchHits(const QVector<SearchHit>& chats,
                                   const QVector<SearchHit>& messages,
                                   const QVector<SearchHit>& people,
                                   int maxPerSource)
{
    if (maxPerSource <= 0)
        return {};

    const QVector<SearchHit> rankedChats = rankedSource(chats, maxPerSource);
    const QVector<SearchHit> rankedMessages = rankedSource(messages, maxPerSource);
    const QVector<SearchHit> rankedPeople = rankedSource(people, maxPerSource);

    QVector<SearchHit> merged;
    merged.reserve(rankedChats.size() + rankedMessages.size() + rankedPeople.size());

    // Round-robin in SearchSource order: chat, message, person, chat, ... A
    // source that has no row left for this round is skipped, so a long message
    // list still alternates with the shorter chat and people lists.
    const qsizetype rounds =
        std::max({rankedChats.size(), rankedMessages.size(), rankedPeople.size()});
    for (qsizetype round = 0; round < rounds; ++round) {
        if (round < rankedChats.size())
            merged.append(rankedChats.at(round));
        if (round < rankedMessages.size())
            merged.append(rankedMessages.at(round));
        if (round < rankedPeople.size())
            merged.append(rankedPeople.at(round));
    }

    return merged;
}

} // namespace beygram::telegram::globalsearch
