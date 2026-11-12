#include "StoryReactionModel.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool sameReactions(const QVector<StoryReaction>& lhs, const QVector<StoryReaction>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype i = 0; i < lhs.size(); ++i) {
        if (lhs.at(i).storyId != rhs.at(i).storyId)
            return false;
        if (lhs.at(i).chatId != rhs.at(i).chatId)
            return false;
        if (lhs.at(i).emoji != rhs.at(i).emoji)
            return false;
    }

    return true;
}

} // namespace

StoryReactionModel::StoryReactionModel(QObject* parent) : QObject(parent)
{
}

void StoryReactionModel::setReactions(QVector<StoryReaction> reactions)
{
    QVector<StoryReaction> normalized;
    normalized.reserve(reactions.size());

    QSet<qint64> seen;
    for (const StoryReaction& reaction : reactions) {
        if (reaction.storyId <= 0)
            continue;

        // First entry for a story wins; later duplicates are ignored entirely.
        if (seen.contains(reaction.storyId))
            continue;
        seen.insert(reaction.storyId);

        StoryReaction entry = reaction;
        entry.emoji = reaction.emoji.trimmed();
        if (entry.emoji.isEmpty())
            continue;

        normalized.append(entry);
    }

    if (sameReactions(normalized, m_reactions))
        return;

    m_reactions = std::move(normalized);
    emit changed();
}

QVector<StoryReaction> StoryReactionModel::reactions() const
{
    return m_reactions;
}

int StoryReactionModel::count() const
{
    return static_cast<int>(m_reactions.size());
}

QString StoryReactionModel::reactionFor(qint64 storyId) const
{
    if (storyId <= 0)
        return {};

    const auto it = std::find_if(m_reactions.cbegin(), m_reactions.cend(),
                                 [storyId](const StoryReaction& reaction) {
                                     return reaction.storyId == storyId;
                                 });
    if (it == m_reactions.cend())
        return {};

    return it->emoji;
}

bool StoryReactionModel::setReaction(qint64 storyId, const QString& emoji)
{
    if (storyId <= 0)
        return false;

    const QString trimmed = emoji.trimmed();

    const auto it = std::find_if(m_reactions.begin(), m_reactions.end(),
                                 [storyId](const StoryReaction& reaction) {
                                     return reaction.storyId == storyId;
                                 });
    const bool present = it != m_reactions.end();

    if (trimmed.isEmpty()) {
        if (!present)
            return false;

        m_reactions.erase(it);
        emit changed();
        return true;
    }

    if (present) {
        if (it->emoji == trimmed)
            return false;

        it->emoji = trimmed;
        emit changed();
        return true;
    }

    m_reactions.append(StoryReaction{storyId, trimmed, 0});
    emit changed();
    return true;
}

QVector<StoryReaction> StoryReactionModel::forChat(qint64 chatId) const
{
    QVector<StoryReaction> result;
    if (chatId <= 0)
        return result;

    for (const StoryReaction& reaction : m_reactions) {
        if (reaction.chatId == chatId)
            result.append(reaction);
    }

    return result;
}

void StoryReactionModel::clear()
{
    if (m_reactions.isEmpty())
        return;

    m_reactions.clear();
    emit changed();
}

} // namespace beygram::telegram
