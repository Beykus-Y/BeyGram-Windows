#pragma once

#include "telegram/SyncedLyricsModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// QML-facing facade over SyncedLyricsModel: the text of the stored lines, the
/// line active at the last playback position and whether any lyrics exist.
///
/// The facade owns the model and does no parsing of its own. setLyrics() hands
/// the LRC text to the model, which sorts the lines, applies [offset:], drops
/// malformed and untimed lines and merges duplicate starts. The facade only
/// projects the result into the shape QML binds to: a QStringList of line
/// texts in start-time order.
///
/// Invariants:
///  - lines() holds one text per stored line, sorted by start time. Timestamps
///    are not exposed; the UI only needs the words and the active index.
///  - currentIndex() is the index of the line whose start time is not after
///    the last position, or -1 before the first line and when no lyrics are
///    stored.
///  - hasLyrics() is true exactly while at least one line is stored.
///  - setPosition() clamps negative input to 0. The position survives
///    setLyrics() and clear(), like the model it wraps.
///
/// changed() is emitted once per call that changed one of the observable
/// properties above. A position move that stays inside the same line, an
/// equivalent setLyrics(), or a clear() with nothing stored is silent.
class LyricsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList lines READ lines NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY changed)

public:
    explicit LyricsFacade(QObject* parent = nullptr);

    /// Text of every stored line, sorted by start time.
    [[nodiscard]] QStringList lines() const;

    /// Index of the line active at the stored position, or -1.
    [[nodiscard]] int currentIndex() const;

    /// Whether at least one line is stored.
    [[nodiscard]] bool hasLyrics() const;

    /// Replaces the stored lyrics with the lines parsed from lrc. Empty or
    /// untimed input clears the lines. The playback position is kept.
    Q_INVOKABLE void setLyrics(const QString& lrc);

    /// Sets the absolute playback position in milliseconds. Negative values
    /// are clamped to 0.
    Q_INVOKABLE void setPosition(int positionMs);

    /// Drops every stored line while keeping the playback position.
    Q_INVOKABLE void clear();

signals:
    /// Emitted whenever lines(), currentIndex() or hasLyrics() changed.
    void changed();

private:
    /// Every observable field, used to emit changed() exactly once per call
    /// that actually changed something.
    struct Snapshot {
        QStringList lines;
        int currentIndex = -1;
        bool hasLyrics = false;

        bool operator==(const Snapshot&) const = default;
    };

    [[nodiscard]] Snapshot snapshot() const;

    /// Emits changed() when the current snapshot differs from \a before.
    void commit(const Snapshot& before);

    telegram::SyncedLyricsModel m_model;
};

} // namespace beygram::ui
