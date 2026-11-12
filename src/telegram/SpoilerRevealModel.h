#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks which messages currently have their spoiler content revealed.
///
/// Revealing a spoiler is a transient, per-view UI state: the text stays
/// covered until the user taps it, and nothing about the reveal is persisted
/// or sent to the server. The model keeps the revealed ids in reveal order so
/// a long chat session cannot grow the set without bound; once the cap is
/// reached the oldest reveal is dropped and the message is covered again.
class SpoilerRevealModel final : public QObject {
    Q_OBJECT

public:
    explicit SpoilerRevealModel(QObject* parent = nullptr);

    /// Reveals messageId. Non-positive ids are ignored, and revealing a
    /// message that is already revealed changes nothing (it keeps its place in
    /// the eviction order).
    void reveal(qint64 messageId);
    [[nodiscard]] bool isRevealed(qint64 messageId) const;

    /// Covers a single message again. Unknown ids are a silent no-op.
    void hide(qint64 messageId);

    /// Covers every message again, e.g. when the chat is closed.
    void hideAll();

    [[nodiscard]] int revealedCount() const;

    /// Caps how many messages may stay revealed at once, clamped to
    /// 1..10000. Lowering the cap drops the oldest reveals immediately.
    void setMaxRevealed(int max);
    [[nodiscard]] int maxRevealed() const;

signals:
    /// Emitted whenever the revealed set or the cap actually changed.
    void changed();

private:
    QVector<qint64> m_ids;
    int m_max = 200;
};

} // namespace beygram::telegram
