#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One line of synchronized lyrics: the moment it starts and its words.
///
/// startMs is an absolute offset from the start of the track in milliseconds,
/// already adjusted for any LRC [offset:] tag and never negative.
struct LyricLine {
    int startMs = 0;
    QString text;
};

/// Holds the synchronized lyrics of one track and resolves the line that is
/// active at a playback position.
///
/// setLyrics() accepts LRC text and is the only way to fill the model. It
/// parses every timestamp a line carries, so "[00:10.00][00:20.00]Chorus"
/// yields two lines, keeps the text after the last timestamp, drops lines
/// without text and lines whose timestamps are malformed, applies the global
/// [offset:] tag, sorts the result by start time and merges duplicate start
/// times into one line, keeping the first parsed text. Other LRC tags such as
/// [ti:] or [ar:] are ignored. Input larger than 256 KiB is rejected as a
/// whole. Parsing never throws.
///
/// The model is a plain value holder: it stores already-parsed lines and a
/// playback position, and never touches TDLib, the network or the UI.
class SyncedLyricsModel final : public QObject {
    Q_OBJECT

public:
    explicit SyncedLyricsModel(QObject* parent = nullptr);

    /// Replaces the stored lyrics with the lines parsed from lrc. Emits
    /// changed() only when the resulting line list differs from the stored
    /// one, so assigning an equivalent text does not notify. The playback
    /// position is kept.
    void setLyrics(const QString& lrc);

    /// Returns a copy of the stored lines, sorted by startMs ascending.
    [[nodiscard]] QVector<LyricLine> lines() const;

    /// Number of stored lines.
    [[nodiscard]] int count() const;

    /// Sets the absolute playback position in milliseconds. Negative values
    /// are clamped to 0. Emits changed() only when the stored position
    /// actually changes.
    void setPositionMs(int positionMs);

    /// Index of the line that should be highlighted at the stored position:
    /// the last line whose startMs is not after the position. Returns -1
    /// before the first line and when no lyrics are stored.
    [[nodiscard]] int currentIndex() const;

    /// Text of the current line, or an empty string before the first line.
    [[nodiscard]] QString currentText() const;

    /// True when at least one line is stored.
    [[nodiscard]] bool hasLyrics() const;

    /// Drops every line, emitting changed() once when there was anything to
    /// drop. The playback position is kept.
    void clear();

signals:
    /// Emitted whenever the stored lines or the playback position changed.
    void changed();

private:
    QVector<LyricLine> m_lines;
    int m_position = 0;
};

} // namespace beygram::telegram
