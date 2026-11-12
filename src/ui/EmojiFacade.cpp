#include "EmojiFacade.h"

#include <QVector>

#include <utility>

namespace beygram::ui {
namespace {

// The lists are positional, so one emoji row takes the name and category at the
// same offset. The emoji list defines the rows: a name or category past its end
// belongs to no emoji and is dropped, while a missing name or category is left
// empty. EmojiCatalogModel then drops rows without an emoji and keeps the first
// row per repeated emoji.
QVector<telegram::EmojiEntry> buildEntries(const QStringList& emojis,
                                           const QStringList& names,
                                           const QStringList& categories)
{
    QVector<telegram::EmojiEntry> entries;
    entries.reserve(emojis.size());

    for (qsizetype i = 0; i < emojis.size(); ++i) {
        telegram::EmojiEntry entry;
        entry.emoji = emojis.at(i);
        if (i < names.size())
            entry.name = names.at(i);
        if (i < categories.size())
            entry.category = categories.at(i);
        entries.append(entry);
    }

    return entries;
}

} // namespace

EmojiFacade::EmojiFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList EmojiFacade::categories() const
{
    return m_catalog.categories();
}

QStringList EmojiFacade::emojis() const
{
    // search() treats an empty query as "matches everything" and a negative
    // limit as "no limit", so this one pass covers both "all" and "searching".
    const QVector<telegram::EmojiEntry> matches = m_catalog.search(m_query, -1);
    const QString category = selectedCategory();

    QStringList result;
    result.reserve(matches.size());
    for (const telegram::EmojiEntry& entry : matches) {
        // An empty category is "all"; otherwise the section narrows the
        // matches instead of being replaced by them.
        if (!category.isEmpty()
            && entry.category.compare(category, Qt::CaseInsensitive) != 0) {
            continue;
        }
        result.append(entry.emoji);
    }

    return result;
}

QString EmojiFacade::query() const
{
    return m_query;
}

void EmojiFacade::setQuery(const QString& query)
{
    const State before = state();
    // Trimming here is what lets the search field keep a trailing space the
    // user typed without turning it into a different search.
    m_query = query.trimmed();
    emitIfChanged(before);
}

void EmojiFacade::setCatalog(const QStringList& emojis, const QStringList& names,
                             const QStringList& categories)
{
    const State before = state();
    m_catalog.setEntries(buildEntries(emojis, names, categories));
    // The selection is kept by name and resolved on read, so a catalog that
    // still has the section leaves the user on it even when the sections moved,
    // and one that dropped it resolves to -1 and shows every emoji again.
    emitIfChanged(before);
}

int EmojiFacade::categoryIndex() const
{
    if (m_selectedCategory.isEmpty())
        return -1;

    // Case-insensitive, because that is how EmojiCatalogModel compares
    // sections: a re-spelled section is still the one the user picked.
    const QStringList names = m_catalog.categories();
    for (qsizetype i = 0; i < names.size(); ++i) {
        if (names.at(i).compare(m_selectedCategory, Qt::CaseInsensitive) == 0)
            return static_cast<int>(i);
    }

    return -1;
}

void EmojiFacade::selectCategory(int index)
{
    const QStringList names = m_catalog.categories();
    // Only a real section is a selection; -1 and anything out of range mean
    // "all", so a stale index can never leave the picker on nothing.
    const QString next = (index >= 0 && index < names.size())
                             ? names.at(index)
                             : QString();

    const State before = state();
    m_selectedCategory = next;
    emitIfChanged(before);
}

void EmojiFacade::activate(const QString& emoji)
{
    if (emoji.isEmpty())
        return;

    // The recent list is not exposed, so recording the use cannot move any
    // property and needs no changed(); emojiActivated() is the whole point.
    m_catalog.addRecent(emoji);
    emit emojiActivated(emoji);
}

void EmojiFacade::addReplacement(const QString& from, const QString& to)
{
    if (from.isEmpty())
        return;

    // Appending keeps every existing rule in place. The model keeps the first
    // rule per needle, so re-adding one is a silent no-op rather than a
    // second rule that could never match.
    QVector<telegram::ReplacementRule> rules = m_replacements.rules();
    rules.append(telegram::ReplacementRule{from, to});
    m_replacements.setRules(std::move(rules));
}

QString EmojiFacade::applyReplacements(const QString& text) const
{
    return m_replacements.apply(text);
}

EmojiFacade::State EmojiFacade::state() const
{
    State s;
    s.categories = categories();
    s.emojis = emojis();
    s.query = m_query;
    s.categoryIndex = categoryIndex();
    return s;
}

void EmojiFacade::emitIfChanged(const State& before)
{
    if (state() == before)
        return;
    emit changed();
}

QString EmojiFacade::selectedCategory() const
{
    if (m_selectedCategory.isEmpty())
        return QString();

    const QStringList names = m_catalog.categories();
    for (const QString& name : names) {
        if (name.compare(m_selectedCategory, Qt::CaseInsensitive) == 0)
            return name;
    }

    return QString();
}

} // namespace beygram::ui
