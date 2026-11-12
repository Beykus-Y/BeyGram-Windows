#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks the client-side state of the story viewer: which chat's stories are
/// being shown, the order they are shown in, which one is on screen, and which
/// ones have already been marked as seen.
///
/// A viewing session starts with open() and ends either with close() (the user
/// dismissed the viewer) or with next() past the last story. Both reset every
/// field, so no story, seen id or paused flag from a previous session can leak
/// into the next one.
///
/// Only positive chat and story ids are accepted, the incoming story order is
/// preserved, and every mutator is a no-op when it would not change the stored
/// state, so changed() is emitted once per real transition and views never
/// repaint for nothing.
class StoryViewerController final : public QObject {
    Q_OBJECT

public:
    explicit StoryViewerController(QObject* parent = nullptr);

    /// Opens the viewer on chatId showing storyIds. Non-positive ids are
    /// dropped, repeated ids keep their first position, and the request is
    /// ignored when the chat id is not positive or no story id survives. Index,
    /// seen ids and paused are reset. Reopening the same chat with the same ids
    /// while the viewer is open is a no-op, so the current story and the seen
    /// set are kept.
    void open(qint64 chatId, QVector<qint64> storyIds);

    /// Dismisses the viewer and clears all state. No-op when already closed.
    void close();

    /// True while a chat with at least one story is being viewed.
    [[nodiscard]] bool active() const;

    /// Chat owning the open session, or 0 when the viewer is closed.
    [[nodiscard]] qint64 chatId() const;

    /// Stories in playback order, empty when the viewer is closed.
    [[nodiscard]] QVector<qint64> storyIds() const;

    /// Zero based position of the story on screen, 0 when the viewer is closed.
    [[nodiscard]] int currentIndex() const;

    /// Story on screen, or 0 when the viewer is closed.
    [[nodiscard]] qint64 currentStoryId() const;

    /// Moves to the next story. Advancing past the last story closes the
    /// viewer. No-op when the viewer is closed.
    void next();

    /// Moves to the previous story. Stays on the first story instead of
    /// wrapping. No-op when the viewer is closed.
    void previous();

    /// Records storyId as seen. Unknown ids, non-positive ids and ids that are
    /// already seen are ignored.
    void markSeen(qint64 storyId);

    /// Stories of the open session that have not been marked seen, in playback
    /// order. Empty when the viewer is closed.
    [[nodiscard]] QVector<qint64> unseen() const;

    /// Pauses or resumes playback. Ignored when the viewer is closed.
    void setPaused(bool paused);

    /// True while the open session is paused.
    [[nodiscard]] bool paused() const;

signals:
    /// Emitted whenever the viewer state actually changed.
    void changed();

    /// Emitted when the viewer closed, before the matching changed().
    void closed();

private:
    qint64 m_chatId = 0;
    QVector<qint64> m_ids;
    int m_index = 0;
    QVector<qint64> m_seen;
    bool m_paused = false;
};

} // namespace beygram::telegram
