#include "ReactionsFacade.h"

#include "telegram/ReactionPickerModel.h"

namespace beygram::ui {
namespace {

/// Converts the QML-facing emoji list into catalog entries. Empty strings are
/// skipped because they cannot be selected and would only add dead buttons.
QVector<telegram::AvailableReaction> toReactions(const QStringList& emoji)
{
    QVector<telegram::AvailableReaction> reactions;
    reactions.reserve(emoji.size());
    for (const QString& value : emoji) {
        if (value.isEmpty())
            continue;

        telegram::AvailableReaction reaction;
        reaction.type = QStringLiteral("emoji");
        reaction.emoji = value;
        reactions.append(reaction);
    }
    return reactions;
}

/// Returns the emoji of every entry the picker can render. Custom-emoji and
/// paid entries never come from toReactions(), but they are filtered anyway so
/// the properties stay plain emoji lists.
QStringList toEmoji(const QVector<telegram::AvailableReaction>& reactions)
{
    QStringList emoji;
    emoji.reserve(reactions.size());
    for (const telegram::AvailableReaction& reaction : reactions) {
        if (reaction.type == QLatin1String("emoji") && !reaction.emoji.isEmpty())
            emoji.append(reaction.emoji);
    }
    return emoji;
}

} // namespace

ReactionsFacade::ReactionsFacade(QObject* parent)
    : QObject(parent), m_model(new telegram::ReactionPickerModel(this))
{
    connect(m_model, &telegram::ReactionPickerModel::changed, this,
            &ReactionsFacade::changed);
}

QStringList ReactionsFacade::top() const
{
    return toEmoji(m_model->top());
}

QStringList ReactionsFacade::recent() const
{
    return toEmoji(m_model->recent());
}

int ReactionsFacade::maxCount() const
{
    return m_model->maxCount();
}

QString ReactionsFacade::selected() const
{
    return m_model->selected();
}

void ReactionsFacade::setCatalog(const QStringList& topEmoji,
                                 const QStringList& recentEmoji, int maxCount)
{
    telegram::ReactionCatalogState catalog;
    catalog.topReactions = toReactions(topEmoji);
    catalog.recentReactions = toReactions(recentEmoji);
    catalog.maxReactionCount = maxCount;
    m_model->setCatalog(catalog);
}

void ReactionsFacade::toggle(const QString& emoji)
{
    const QString before = m_model->selected();
    m_model->toggle(emoji);

    // An unavailable or empty emoji is ignored by the model, and a no-op toggle
    // must not reach the network layer.
    const QString after = m_model->selected();
    if (after == before || !hasTarget())
        return;

    emit reactionRequested(m_chatId, m_messageId, after);
}

void ReactionsFacade::clearSelection()
{
    m_model->setSelected(QString());
}

bool ReactionsFacade::expanded() const
{
    return m_model->expanded();
}

void ReactionsFacade::setExpanded(bool expanded)
{
    m_model->setExpanded(expanded);
}

void ReactionsFacade::openFor(const QString& chatId, const QString& messageId)
{
    m_chatId = chatId;
    m_messageId = messageId;

    // The previous message's choice says nothing about this one.
    m_model->setSelected(QString());
}

bool ReactionsFacade::hasTarget() const
{
    return !m_chatId.isEmpty() && !m_messageId.isEmpty();
}

} // namespace beygram::ui
