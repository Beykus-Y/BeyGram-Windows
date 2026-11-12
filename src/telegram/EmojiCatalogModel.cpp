#include "EmojiCatalogModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool storesEmoji(const QVector<EmojiEntry>& entries, const QString& emoji)
{
    return std::any_of(entries.cbegin(), entries.cend(),
                       [&emoji](const EmojiEntry& entry) {
                           return entry.emoji == emoji;
                       });
}

// EmojiEntry carries no operator==, so the change check compares the three
// fields explicitly. Keeps setEntries() from notifying on a no-op assignment.
bool sameEntries(const QVector<EmojiEntry>& lhs, const QVector<EmojiEntry>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                      [](const EmojiEntry& left, const EmojiEntry& right) {
                          return left.emoji == right.emoji
                              && left.name == right.name
                              && left.category == right.category;
                      });
}

} // namespace

EmojiCatalogModel::EmojiCatalogModel(QObject* parent)
    : QObject(parent)
{
}

void EmojiCatalogModel::setEntries(QVector<EmojiEntry> entries)
{
    QVector<EmojiEntry> next;
    next.reserve(entries.size());

    for (EmojiEntry& entry : entries) {
        // An entry without an emoji cannot be inserted into a message.
        if (entry.emoji.isEmpty())
            continue;
        // Telegram emoji are unique per character sequence; the first spelling
        // and its name/category win over later repeats.
        if (storesEmoji(next, entry.emoji))
            continue;
        next.append(std::move(entry));
    }

    if (sameEntries(next, m_entries))
        return;

    m_entries = std::move(next);
    emit changed();
}

int EmojiCatalogModel::count() const
{
    return static_cast<int>(m_entries.size());
}

QStringList EmojiCatalogModel::categories() const
{
    QStringList names;

    for (const EmojiEntry& entry : m_entries) {
        if (entry.category.isEmpty())
            continue;
        // byCategory() ignores case, so two spellings of one category would
        // list the same entries twice; the first spelling is kept.
        if (names.contains(entry.category, Qt::CaseInsensitive))
            continue;
        names.append(entry.category);
    }

    return names;
}

QVector<EmojiEntry> EmojiCatalogModel::byCategory(const QString& category) const
{
    QVector<EmojiEntry> matches;
    if (category.isEmpty())
        return matches;

    matches.reserve(m_entries.size());
    for (const EmojiEntry& entry : m_entries) {
        if (entry.category.compare(category, Qt::CaseInsensitive) == 0)
            matches.append(entry);
    }

    return matches;
}

QVector<EmojiEntry> EmojiCatalogModel::search(const QString& query, int limit) const
{
    QVector<EmojiEntry> matches;
    if (limit == 0)
        return matches;

    if (limit > 0) {
        matches.reserve(qMin<qsizetype>(m_entries.size(),
                                        static_cast<qsizetype>(limit)));
    }

    for (const EmojiEntry& entry : m_entries) {
        if (!entry.emoji.contains(query, Qt::CaseInsensitive)
            && !entry.name.contains(query, Qt::CaseInsensitive)) {
            continue;
        }

        matches.append(entry);
        if (limit > 0 && matches.size() >= limit)
            break;
    }

    return matches;
}

void EmojiCatalogModel::addRecent(const QString& emoji, int maxRecent)
{
    if (emoji.isEmpty() || maxRecent <= 0)
        return;

    QStringList next;
    next.reserve(qMin<qsizetype>(m_recent.size() + 1,
                                 static_cast<qsizetype>(maxRecent)));
    next.append(emoji);

    for (const QString& existing : std::as_const(m_recent)) {
        if (existing == emoji)
            continue;
        if (next.size() >= maxRecent)
            break;
        next.append(existing);
    }

    if (next == m_recent)
        return;

    m_recent = std::move(next);
    emit changed();
}

QStringList EmojiCatalogModel::recent() const
{
    return m_recent;
}

void EmojiCatalogModel::clear()
{
    if (m_entries.isEmpty() && m_recent.isEmpty())
        return;

    m_entries.clear();
    m_recent.clear();
    emit changed();
}

} // namespace beygram::telegram
