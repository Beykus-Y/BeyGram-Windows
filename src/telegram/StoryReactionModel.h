#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One emoji reaction the user left on a story.
///
/// TDLib keeps reactions in a map keyed by story id, but the story bar and the
/// viewer need the list in the order the stories are shown, so this model holds
/// a vector instead of a hash. chatId is the poster chat that owns the story; it
/// is carried so forChat() can answer "what did I react to in this chat".
struct StoryReaction {
    qint64 storyId = 0;
    QString emoji;
    qint64 chatId = 0;
};

/// The set of emoji reactions the current account left on stories.
///
/// The model is deliberately dumb: it stores what the caller hands it, keeps the
/// entries in insertion order and only emits changed() when the stored content
/// actually differs. Story ids are the identity of an entry, so a story can
/// carry at most one emoji.
///
/// Normalisation happens on the way in: non-positive story ids are dropped
/// (they cannot address a story), the emoji is trimmed, and an entry whose emoji
/// is empty after trimming is dropped as well, because "no emoji" and "no
/// reaction" are the same state. Duplicate story ids keep their first entry.
class StoryReactionModel final : public QObject {
    Q_OBJECT

public:
    explicit StoryReactionModel(QObject* parent = nullptr);

    /// Replaces the whole set. Entries are normalised as described above and
    /// the surviving order is preserved. Emits changed() only when the
    /// normalised result differs from the current set.
    void setReactions(QVector<StoryReaction> reactions);

    /// Copy of the stored entries, in display order.
    [[nodiscard]] QVector<StoryReaction> reactions() const;

    /// Number of stored reactions.
    [[nodiscard]] int count() const;

    /// Emoji stored for storyId, or an empty string when the story has none.
    [[nodiscard]] QString reactionFor(qint64 storyId) const;

    /// Adds, updates or removes the reaction for storyId.
    ///
    /// An empty emoji (after trimming) removes the entry. Non-positive story ids
    /// are rejected and leave the model untouched. Returns true when the stored
    /// content changed; repeating the current emoji, or removing a story that
    /// has no reaction, returns false without emitting changed().
    bool setReaction(qint64 storyId, const QString& emoji);

    /// Reactions whose chatId matches, in stored order. A non-positive chatId
    /// cannot match anything and yields an empty list.
    [[nodiscard]] QVector<StoryReaction> forChat(qint64 chatId) const;

    /// Drops every reaction. Emits changed() only when there was something to
    /// drop.
    void clear();

signals:
    /// Emitted whenever the stored reaction set changed.
    void changed();

private:
    QVector<StoryReaction> m_reactions;
};

} // namespace beygram::telegram
