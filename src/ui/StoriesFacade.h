#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {
class StoryProgressModel;
class StoryReactionModel;
class StoryViewerController;
} // namespace beygram::telegram

namespace beygram::ui {

/// QML-facing facade over the story viewer session, its playback clock and the
/// reactions the account left on stories.
///
/// The facade translates between the string ids QML speaks and the qint64 ids
/// the Telegram-side models use, and exposes one coherent state so bindings do
/// not have to watch three objects. It owns its models, holds no transport and
/// performs no logging: sending a reaction is only announced through
/// reactionRequested() for the caller to turn into a TDLib request.
///
/// Playback contract:
///  - open() starts the first story with StoryDurationMs and starts a session.
///  - next() marks the current story seen (once), then starts the next story.
///    Advancing past the last story closes the viewer.
///  - previous() stays on the first story and restarts the landed story's
///    progress; it never marks anything seen.
///  - tick() takes the absolute playback offset, mirroring the progress model,
///    and advances automatically when the offset reaches the duration.
///  - close() clears the session and the progress; stored reactions are kept so
///    reopening a story still shows the emoji the account left on it.
///
/// changed() is emitted once per observable transition: the facade compares the
/// exposed values before emitting, so a progress restart or a reaction stored
/// for another story does not repaint the view for nothing.
class StoriesFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString currentStoryId READ currentStoryId NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(QString reaction READ reaction NOTIFY changed)

public:
    /// Playback duration the facade gives every story it starts.
    static constexpr int StoryDurationMs = 5000;

    explicit StoriesFacade(QObject* parent = nullptr);

    /// True while a chat with at least one story is being viewed.
    [[nodiscard]] bool active() const;

    /// Story on screen as a string, empty when the viewer is closed.
    [[nodiscard]] QString currentStoryId() const;

    /// Zero based position of the story on screen, 0 when the viewer is closed.
    [[nodiscard]] int index() const;

    /// Number of stories in the open session, 0 when the viewer is closed.
    [[nodiscard]] int count() const;

    /// Playback position as 0.0..1.0, 0.0 when the viewer is closed.
    [[nodiscard]] double progress() const;

    /// True while playback is paused.
    [[nodiscard]] bool paused() const;

    /// Emoji the account left on the story on screen, empty when there is none.
    [[nodiscard]] QString reaction() const;

    /// Starts a viewing session for chatId showing storyIds in order. Ids that
    /// are not positive integers are dropped, and the request is ignored when
    /// the chat id is invalid or no story id survives. Reopening the same chat
    /// with the same stories keeps the running session, including the current
    /// story, the seen set and the playback position.
    Q_INVOKABLE void open(const QString& chatId, const QStringList& storyIds);

    /// Dismisses the viewer and stops playback. No-op when already closed.
    Q_INVOKABLE void close();

    /// Marks the story on screen seen, then advances to the next one. Advancing
    /// past the last story closes the viewer.
    Q_INVOKABLE void next();

    /// Moves to the previous story and restarts its progress. Stays on the
    /// first story instead of wrapping.
    Q_INVOKABLE void previous();

    /// Pauses or resumes playback. Ignored when the viewer is closed.
    Q_INVOKABLE void setPaused(bool value);

    /// Sets the absolute playback offset of the story on screen. The viewer
    /// advances to the next story once the offset reaches the duration.
    Q_INVOKABLE void tick(int elapsedMs);

    /// Stores emoji for the story on screen and announces it through
    /// reactionRequested(). Repeating the stored emoji is a no-op, and an empty
    /// emoji or a closed viewer is ignored.
    Q_INVOKABLE void react(const QString& emoji);

    /// Drops the reaction of the story on screen and announces the removal
    /// through reactionRequested() with an empty emoji. No-op when the story has
    /// no stored reaction.
    Q_INVOKABLE void clearReaction();

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted when advancing marked storyId as seen for the first time.
    void seen(const QString& storyId);

    /// Emitted when the user set or removed a reaction. An empty emoji means
    /// the reaction should be removed on the server.
    void reactionRequested(const QString& storyId, const QString& emoji);

private:
    struct Snapshot {
        bool active = false;
        QString storyId;
        int index = 0;
        int count = 0;
        double progress = 0.0;
        bool paused = false;
        QString reaction;

        bool operator==(const Snapshot&) const = default;
    };

    [[nodiscard]] Snapshot snapshot() const;
    void syncChanged();
    void beginCurrentProgress();
    void markCurrentSeen();

    telegram::StoryViewerController* m_controller = nullptr;
    telegram::StoryProgressModel* m_progress = nullptr;
    telegram::StoryReactionModel* m_reactions = nullptr;
    Snapshot m_last;
};

} // namespace beygram::ui
