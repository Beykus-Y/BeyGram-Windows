#include "ReactionPickerModel.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

// AvailableReaction carries no operator==, so the catalog comparison spells out
// its fields. Keeps setCatalog() from notifying on an identical assignment.
bool sameReaction(const AvailableReaction& lhs, const AvailableReaction& rhs)
{
    return lhs.type == rhs.type
        && lhs.emoji == rhs.emoji
        && lhs.customEmojiId == rhs.customEmojiId
        && lhs.title == rhs.title
        && lhs.isActive == rhs.isActive
        && lhs.isPremium == rhs.isPremium;
}

bool sameReactions(const QVector<AvailableReaction>& lhs,
                   const QVector<AvailableReaction>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), sameReaction);
}

// A reaction is selectable when either list offers its emoji. Custom and paid
// entries have an empty emoji, so they never match and never become selectable.
bool offers(const QVector<AvailableReaction>& reactions, const QString& emoji)
{
    if (emoji.isEmpty())
        return false;

    return std::any_of(reactions.cbegin(), reactions.cend(),
                       [&emoji](const AvailableReaction& reaction) {
                           return reaction.emoji == emoji;
                       });
}

} // namespace

ReactionPickerModel::ReactionPickerModel(QObject* parent)
    : QObject(parent)
{
}

void ReactionPickerModel::setCatalog(const ReactionCatalogState& catalog)
{
    // A selection only survives when the new catalog still offers it. Keeping a
    // vanished emoji would let selected() name a reaction the picker cannot
    // send, so it is dropped together with the catalog it came from.
    const bool selectionSurvives = m_selected.isEmpty()
        || offers(catalog.topReactions, m_selected)
        || offers(catalog.recentReactions, m_selected);

    if (selectionSurvives
        && m_max == catalog.maxReactionCount
        && sameReactions(m_top, catalog.topReactions)
        && sameReactions(m_recent, catalog.recentReactions)) {
        return;
    }

    m_top = catalog.topReactions;
    m_recent = catalog.recentReactions;
    m_max = catalog.maxReactionCount;

    if (!selectionSurvives)
        m_selected.clear();

    emit changed();
}

QVector<AvailableReaction> ReactionPickerModel::top() const
{
    return m_top;
}

QVector<AvailableReaction> ReactionPickerModel::recent() const
{
    return m_recent;
}

int ReactionPickerModel::maxCount() const
{
    return m_max;
}

void ReactionPickerModel::setSelected(const QString& emoji)
{
    if (emoji.isEmpty()) {
        if (m_selected.isEmpty())
            return;

        m_selected.clear();
        emit changed();
        return;
    }

    if (!offers(m_top, emoji) && !offers(m_recent, emoji))
        return;

    if (m_selected == emoji)
        return;

    m_selected = emoji;
    emit changed();
}

QString ReactionPickerModel::selected() const
{
    return m_selected;
}

bool ReactionPickerModel::isSelected(const QString& emoji) const
{
    if (emoji.isEmpty())
        return false;

    return m_selected == emoji;
}

void ReactionPickerModel::toggle(const QString& emoji)
{
    if (emoji.isEmpty())
        return;

    if (m_selected == emoji) {
        m_selected.clear();
        emit changed();
        return;
    }

    // setSelected() applies the offered-emoji rule, so toggling an emoji the
    // catalog does not offer stays a no-op.
    setSelected(emoji);
}

bool ReactionPickerModel::expanded() const
{
    return m_expanded;
}

void ReactionPickerModel::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
        return;

    m_expanded = expanded;
    emit changed();
}

void ReactionPickerModel::clear()
{
    if (m_top.isEmpty() && m_recent.isEmpty() && m_max == 0
        && m_selected.isEmpty() && !m_expanded) {
        return;
    }

    m_top.clear();
    m_recent.clear();
    m_max = 0;
    m_selected.clear();
    m_expanded = false;

    emit changed();
}

} // namespace beygram::telegram
