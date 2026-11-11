#pragma once

#include <QObject>
#include <QString>

namespace beygram::ui {

/// The desktop window geometry remembered between runs.
///
/// `x`/`y` are screen coordinates and may legitimately be negative: a window on
/// a monitor left of or above the primary one lives in negative space, so they
/// are stored verbatim. `width`/`height` are clamped to [640, 10000] so a
/// damaged file can never produce a window too small to grab or larger than any
/// real display.
struct WindowState {
    int x = 0;
    int y = 0;
    int width = 1280;
    int height = 860;
    bool maximized = false;
};

/// JSON-backed window geometry.
///
/// The file is `{"version":1,"x":...,"y":...,"width":...,"height":...,"maximized":...}`.
/// Missing, damaged or unknown values fall back to the defaults above, so a
/// corrupt file can never make the client unusable. save() clamps the size and
/// only writes (and emits changed()) when a field actually differs. Writes go
/// through QSaveFile, so a crash mid-write keeps the previous file intact.
/// Nothing about the file contents is logged.
class WindowStateStore final : public QObject {
    Q_OBJECT

public:
    explicit WindowStateStore(QString filePath, QObject* parent = nullptr);

    /// Replaces the in-memory geometry with the file contents. Emits changed()
    /// once if that actually altered anything; never writes the file.
    void load();
    [[nodiscard]] WindowState state() const;
    [[nodiscard]] QString filePath() const;

    /// Stores the geometry, clamping width/height first. A no-op when the result
    /// matches what is already held.
    void save(const WindowState& state);

signals:
    void changed();

private:
    void persist();
    QString m_filePath;
    WindowState m_state;
};

} // namespace beygram::ui
