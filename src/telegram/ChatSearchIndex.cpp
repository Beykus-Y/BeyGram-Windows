#include "ChatSearchIndex.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int kMinimumLimit = 1;
constexpr int kMaximumLimit = 100;

// The two ranking rules are kept independent: every title hit scores above
// every preview hit, and within one field a prefix scores above a later
// occurrence. The values are compared only against each other.
constexpr int kTitlePrefixScore = 4;
constexpr int kTitleSubstringScore = 3;
constexpr int kPreviewPrefixScore = 2;
constexpr int kPreviewSubstringScore = 1;

QString normalize(const QString& text)
{
    return text.trimmed().toLower();
}

} // namespace

ChatSearchIndex::ChatSearchIndex(QObject* parent)
    : QObject(parent)
{
}

void ChatSearchIndex::upsert(qint64 chatId, const QString& title, const QString& preview)
{
    // The chat id is the identity of the row; TDLib never sends zero or a
    // negative one, and a row without an identity cannot be searched or removed.
    if (chatId <= 0)
        return;

    const QString normalizedTitle = normalize(title);
    const QString normalizedPreview = normalize(preview);

    const int existing = indexOf(chatId);
    if (existing >= 0) {
        Entry& entry = m_entries[existing];
        const bool textChanged = entry.title != normalizedTitle
            || entry.preview != normalizedPreview;

        // Re-upserting the chat that is already at the front of the recency
        // order with the same text leaves both the rows and the result order
        // untouched, so there is nothing to announce.
        if (!textChanged && isNewest(existing))
            return;

        entry.title = normalizedTitle;
        entry.preview = normalizedPreview;
        entry.recency = ++m_nextRecency;
        emit changed();
        return;
    }

    Entry entry;
    entry.chatId = chatId;
    entry.title = normalizedTitle;
    entry.preview = normalizedPreview;
    entry.recency = ++m_nextRecency;
    m_entries.append(std::move(entry));
    emit changed();
}

void ChatSearchIndex::remove(qint64 chatId)
{
    const int index = indexOf(chatId);
    if (index < 0)
        return;
    m_entries.removeAt(index);
    emit changed();
}

QVector<qint64> ChatSearchIndex::search(const QString& query, int limit) const
{
    const int wanted = qBound(kMinimumLimit, limit, kMaximumLimit);
    const QString normalizedQuery = normalize(query);

    struct Hit {
        qint64 chatId = 0;
        int score = 0;
        quint64 recency = 0;
    };

    QVector<Hit> hits;
    hits.reserve(m_entries.size());
    for (const Entry& entry : m_entries) {
        // An empty query is the recency listing, so every row qualifies with
        // the same (zero) score and is ordered by recency alone.
        const int score = normalizedQuery.isEmpty()
            ? 0
            : scoreFor(entry, normalizedQuery);
        if (score < 0)
            continue;
        hits.append(Hit{entry.chatId, score, entry.recency});
    }

    std::sort(hits.begin(), hits.end(), [](const Hit& left, const Hit& right) {
        if (left.score != right.score)
            return left.score > right.score;
        // Recency values are unique, so equal scores always have a stable,
        // order-independent answer.
        return left.recency > right.recency;
    });

    if (hits.size() > wanted)
        hits.resize(wanted);

    QVector<qint64> ids;
    ids.reserve(hits.size());
    for (const Hit& hit : hits)
        ids.append(hit.chatId);
    return ids;
}

int ChatSearchIndex::count() const
{
    return int(m_entries.size());
}

void ChatSearchIndex::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    m_nextRecency = 0;
    emit changed();
}

int ChatSearchIndex::indexOf(qint64 chatId) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).chatId == chatId)
            return i;
    }
    return -1;
}

bool ChatSearchIndex::isNewest(int index) const
{
    const quint64 recency = m_entries.at(index).recency;
    for (const Entry& entry : m_entries) {
        if (entry.recency > recency)
            return false;
    }
    return true;
}

int ChatSearchIndex::scoreFor(const Entry& entry, const QString& query)
{
    // startsWith() is checked before contains(), because every prefix is also
    // a substring and the prefix rank is the better one.
    if (entry.title.startsWith(query))
        return kTitlePrefixScore;
    if (entry.title.contains(query))
        return kTitleSubstringScore;
    if (entry.preview.startsWith(query))
        return kPreviewPrefixScore;
    if (entry.preview.contains(query))
        return kPreviewSubstringScore;
    return -1;
}

} // namespace beygram::telegram
