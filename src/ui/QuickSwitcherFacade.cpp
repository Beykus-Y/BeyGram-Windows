#include "QuickSwitcherFacade.h"

#include <QSet>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace beygram::ui {

QuickSwitcherFacade::QuickSwitcherFacade(QObject* parent) : QObject(parent)
{
}

bool QuickSwitcherFacade::open() const
{
    return m_open;
}

QString QuickSwitcherFacade::query() const
{
    return m_query;
}

QStringList QuickSwitcherFacade::titles() const
{
    QStringList result;
    result.reserve(m_matches.size());

    for (const int index : m_matches)
        result.append(m_chats.at(index).title);

    return result;
}

QStringList QuickSwitcherFacade::ids() const
{
    QStringList result;
    result.reserve(m_matches.size());

    for (const int index : m_matches)
        result.append(m_chats.at(index).id);

    return result;
}

int QuickSwitcherFacade::selectedIndex() const
{
    return m_selectedIndex;
}

QString QuickSwitcherFacade::normalized(const QString& text)
{
    return text.trimmed().toLower();
}

int QuickSwitcherFacade::scoreFor(const Entry& entry, const QString& key)
{
    // Same order as ChatSearchIndex: a title hit always outranks a preview hit,
    // and a prefix always outranks a later occurrence within the same field.
    if (entry.titleKey.startsWith(key))
        return 4;
    if (entry.titleKey.contains(key))
        return 3;
    if (entry.previewKey.startsWith(key))
        return 2;
    if (entry.previewKey.contains(key))
        return 1;
    return -1;
}

void QuickSwitcherFacade::setChats(const QStringList& ids, const QStringList& titles,
                                   const QStringList& previews)
{
    QVector<Entry> chats;
    chats.reserve(ids.size());

    QSet<QString> seen;
    seen.reserve(ids.size());

    for (qsizetype i = 0; i < ids.size(); ++i) {
        const QString id = ids.at(i).trimmed();
        if (id.isEmpty() || seen.contains(id))
            continue;

        seen.insert(id);

        Entry entry;
        entry.id = id;
        entry.title = i < titles.size() ? titles.at(i) : QString();
        entry.titleKey = normalized(entry.title);
        entry.previewKey = normalized(i < previews.size() ? previews.at(i) : QString());
        chats.append(std::move(entry));
    }

    // A reload with the same catalog is not a change. The selection and the
    // visible rows cannot differ either, so there is nothing to reconcile.
    if (sameChats(chats))
        return;

    // Resolve the selection by chat id, not by index: a reload can reorder the
    // catalog or insert a chat above the selected one.
    const QString selectedId = selectedChatId();

    m_chats = std::move(chats);
    rebuildMatches();
    selectChatOrFirst(selectedId);

    emit changed();
}

void QuickSwitcherFacade::show()
{
    if (m_open)
        return;

    m_open = true;
    m_query.clear();
    rebuildMatches();
    m_selectedIndex = m_matches.isEmpty() ? -1 : 0;

    emit changed();
}

void QuickSwitcherFacade::hide()
{
    if (!m_open)
        return;

    m_open = false;

    emit changed();
}

void QuickSwitcherFacade::setQuery(const QString& query)
{
    if (m_query == query)
        return;

    m_query = query;
    rebuildMatches();
    m_selectedIndex = m_matches.isEmpty() ? -1 : 0;

    // The query itself is observable, so this fires even when the filter leaves
    // exactly the same rows visible.
    emit changed();
}

void QuickSwitcherFacade::moveSelection(int delta)
{
    const int count = static_cast<int>(m_matches.size());
    if (count == 0 || delta == 0)
        return;

    int next = m_selectedIndex;
    if (next < 0) {
        // No row is selected: the first move enters the list from the matching
        // end. The invariant keeps this branch unreachable while rows exist,
        // but it makes a stale selection harmless.
        next = delta > 0 ? 0 : count - 1;
    } else {
        // C++ remainder keeps the sign of the dividend, so a negative delta can
        // land below zero; the second modulo brings it back into the list.
        next = (next + delta % count) % count;
        if (next < 0)
            next += count;
    }

    if (next == m_selectedIndex)
        return;

    m_selectedIndex = next;

    emit changed();
}

void QuickSwitcherFacade::activateCurrent()
{
    // Snapshot the id before announcing anything: a changed() slot may re-enter
    // the facade and replace the catalog while the signal is being delivered.
    const QString chatId = selectedChatId();
    if (chatId.isEmpty())
        return;

    if (m_open) {
        m_open = false;
        emit changed();
    }

    emit chatActivated(chatId);
}

void QuickSwitcherFacade::rebuildMatches()
{
    m_matches.clear();
    m_matches.reserve(m_chats.size());

    const QString key = normalized(m_query);
    if (key.isEmpty()) {
        for (int i = 0; i < m_chats.size(); ++i)
            m_matches.append(i);
        return;
    }

    struct Scored {
        int index;
        int score;
    };

    QVector<Scored> scored;
    scored.reserve(m_chats.size());

    for (int i = 0; i < m_chats.size(); ++i) {
        const int score = scoreFor(m_chats.at(i), key);
        if (score >= 0)
            scored.append(Scored{i, score});
    }

    // stable_sort keeps the catalog order within equal scores, which is the
    // documented tie-breaker.
    std::stable_sort(scored.begin(), scored.end(),
                     [](const Scored& left, const Scored& right) {
                         return left.score > right.score;
                     });

    m_matches.reserve(scored.size());
    for (const Scored& match : scored)
        m_matches.append(match.index);
}

bool QuickSwitcherFacade::sameChats(const QVector<Entry>& chats) const
{
    if (chats.size() != m_chats.size())
        return false;

    for (qsizetype i = 0; i < chats.size(); ++i) {
        const Entry& left = chats.at(i);
        const Entry& right = m_chats.at(i);

        // Previews are match-only, so their normalized form is what matters; a
        // raw case or whitespace difference cannot change a visible row.
        if (left.id != right.id || left.title != right.title
            || left.previewKey != right.previewKey) {
            return false;
        }
    }

    return true;
}

QString QuickSwitcherFacade::selectedChatId() const
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_matches.size())
        return QString();

    return m_chats.at(m_matches.at(m_selectedIndex)).id;
}

void QuickSwitcherFacade::selectChatOrFirst(const QString& chatId)
{
    if (!chatId.isEmpty()) {
        for (int i = 0; i < m_matches.size(); ++i) {
            if (m_chats.at(m_matches.at(i)).id == chatId) {
                m_selectedIndex = i;
                return;
            }
        }
    }

    m_selectedIndex = m_matches.isEmpty() ? -1 : 0;
}

} // namespace beygram::ui
