#include "StoriesFacade.h"

#include "telegram/StoryProgressModel.h"
#include "telegram/StoryReactionModel.h"
#include "telegram/StoryViewerController.h"

#include <QVector>

#include <algorithm>
#include <utility>

namespace beygram::ui {

namespace {

/// Parses a QML-provided id. Anything that is not a positive integer yields 0,
/// which the controllers treat as "ignore".
qint64 parseId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

QString formatId(qint64 id)
{
    return id > 0 ? QString::number(id) : QString();
}

} // namespace

StoriesFacade::StoriesFacade(QObject* parent)
    : QObject(parent)
    , m_controller(new telegram::StoryViewerController(this))
    , m_progress(new telegram::StoryProgressModel(this))
    , m_reactions(new telegram::StoryReactionModel(this))
{
    connect(m_controller, &telegram::StoryViewerController::changed,
            this, &StoriesFacade::syncChanged);
    connect(m_progress, &telegram::StoryProgressModel::changed,
            this, &StoriesFacade::syncChanged);
    connect(m_reactions, &telegram::StoryReactionModel::changed,
            this, &StoriesFacade::syncChanged);

    // Reaching the duration ends the story, so the facade owns the advance and
    // the view never has to compare progress with a duration itself. begin()
    // emits finishedChanged() again when it restarts, but finished() is false
    // by then, so this cannot recurse.
    connect(m_progress, &telegram::StoryProgressModel::finishedChanged, this, [this] {
        if (m_progress->finished())
            next();
    });
}

bool StoriesFacade::active() const
{
    return m_controller->active();
}

QString StoriesFacade::currentStoryId() const
{
    return formatId(m_controller->currentStoryId());
}

QString StoriesFacade::chatId() const
{
    return formatId(m_controller->chatId());
}

int StoriesFacade::index() const
{
    return m_controller->currentIndex();
}

int StoriesFacade::count() const
{
    return static_cast<int>(m_controller->storyIds().size());
}

double StoriesFacade::progress() const
{
    // A closed viewer has no playback position even though the progress model
    // may still hold the last story until close() resets it.
    if (!m_controller->active())
        return 0.0;

    return m_progress->progress();
}

bool StoriesFacade::paused() const
{
    return m_controller->active() && m_controller->paused();
}

QString StoriesFacade::reaction() const
{
    return m_reactions->reactionFor(m_controller->currentStoryId());
}

void StoriesFacade::open(const QString& chatId, const QStringList& storyIds)
{
    const qint64 chat = parseId(chatId);
    if (chat <= 0)
        return;

    QVector<qint64> ids;
    ids.reserve(storyIds.size());
    for (const QString& value : storyIds) {
        const qint64 id = parseId(value);
        if (id > 0)
            ids.append(id);
    }
    if (ids.isEmpty())
        return;

    const bool wasActive = m_controller->active();
    const qint64 previousChat = m_controller->chatId();
    const QVector<qint64> previousIds = m_controller->storyIds();
    const int previousIndex = m_controller->currentIndex();

    m_controller->open(chat, ids);

    if (!m_controller->active())
        return;

    // Reopening the same chat with the same stories keeps the running session:
    // restarting the progress here would rewind a story the user is watching.
    if (wasActive && m_controller->chatId() == previousChat
        && m_controller->storyIds() == previousIds
        && m_controller->currentIndex() == previousIndex) {
        return;
    }

    beginCurrentProgress();
}

void StoriesFacade::close()
{
    // Close the controller first so the progress reset below is already seen as
    // "no session" by progress() and only one changed() reaches the view.
    m_controller->close();
    m_progress->reset();
}

void StoriesFacade::next()
{
    if (!m_controller->active())
        return;

    markCurrentSeen();
    m_controller->next();

    if (!m_controller->active()) {
        m_progress->reset();
        return;
    }

    beginCurrentProgress();
}

void StoriesFacade::previous()
{
    if (!m_controller->active())
        return;

    const int previousIndex = m_controller->currentIndex();
    m_controller->previous();

    // Already on the first story: keep the running progress untouched.
    if (m_controller->currentIndex() == previousIndex)
        return;

    beginCurrentProgress();
}

void StoriesFacade::setPaused(bool value)
{
    if (!m_controller->active())
        return;

    m_controller->setPaused(value);
    if (value)
        m_progress->pause();
    else
        m_progress->resume();
}

void StoriesFacade::tick(int elapsedMs)
{
    if (!m_controller->active())
        return;

    m_progress->tick(elapsedMs);
}

void StoriesFacade::react(const QString& emoji)
{
    const QString trimmed = emoji.trimmed();
    if (trimmed.isEmpty())
        return;

    const qint64 storyId = m_controller->currentStoryId();
    if (storyId <= 0)
        return;

    // StoryReactionModel::setReaction() stores chatId 0, but the reaction list
    // is also read per chat, so the entry is written through setReactions() with
    // the owning chat attached.
    QVector<telegram::StoryReaction> entries = m_reactions->reactions();
    const auto it = std::find_if(entries.begin(), entries.end(),
                                 [storyId](const telegram::StoryReaction& entry) {
                                     return entry.storyId == storyId;
                                 });
    if (it != entries.end()) {
        if (it->emoji == trimmed)
            return;

        it->emoji = trimmed;
        it->chatId = m_controller->chatId();
    } else {
        entries.append(telegram::StoryReaction{storyId, trimmed, m_controller->chatId()});
    }

    m_reactions->setReactions(std::move(entries));
    emit reactionRequested(QString::number(storyId), trimmed);
}

void StoriesFacade::clearReaction()
{
    const qint64 storyId = m_controller->currentStoryId();
    if (storyId <= 0)
        return;

    QVector<telegram::StoryReaction> entries = m_reactions->reactions();
    const qsizetype removed = entries.removeIf(
        [storyId](const telegram::StoryReaction& entry) { return entry.storyId == storyId; });
    if (removed == 0)
        return;

    m_reactions->setReactions(std::move(entries));
    emit reactionRequested(QString::number(storyId), QString());
}

StoriesFacade::Snapshot StoriesFacade::snapshot() const
{
    Snapshot result;
    result.active = active();
    result.storyId = currentStoryId();
    result.index = index();
    result.count = count();
    result.progress = progress();
    result.paused = paused();
    result.reaction = reaction();
    return result;
}

void StoriesFacade::syncChanged()
{
    const Snapshot current = snapshot();
    if (current == m_last)
        return;

    m_last = current;
    emit changed();
}

void StoriesFacade::beginCurrentProgress()
{
    if (!m_controller->active()) {
        m_progress->reset();
        return;
    }

    m_progress->begin(m_controller->currentStoryId(), StoryDurationMs);
    if (m_controller->paused())
        m_progress->pause();
}

void StoriesFacade::markCurrentSeen()
{
    const qint64 storyId = m_controller->currentStoryId();
    if (storyId <= 0)
        return;

    // Advancing over a story that is already seen must not report it twice.
    if (!m_controller->unseen().contains(storyId))
        return;

    m_controller->markSeen(storyId);
    emit seen(QString::number(storyId));
}

} // namespace beygram::ui
