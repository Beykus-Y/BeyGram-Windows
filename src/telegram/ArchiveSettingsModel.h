#pragma once

#include <QObject>

namespace beygram::telegram {

/// The archive section of the settings screen: four switches that decide how
/// chats, folders and stories behave once they are archived.
///
/// The model is a plain value holder. It never talks to TDLib, never reads the
/// clock and performs no I/O, so a view binds its toggles to the getters, calls
/// the setters from its controls and repaints on changed(). Logging and
/// persistence are the caller's job.
///
/// Invariants:
///
///  - Every switch is off by default, so a freshly constructed model matches a
///    user who never touched the archive settings.
///  - The four switches are independent: writing one never touches another and
///    never turns the others off, which is how the settings screen treats them.
///  - changed() is emitted only when a value really differs. Writing the value
///    already stored is silent, so a refresh that brings the same settings does
///    not repaint the screen or re-trigger a binding loop.
///  - reset() is silent while the model already is at its defaults and emits
///    changed() exactly once when it actually restores something.
///  - isDefault() is true exactly when all four switches are off. It is the
///    same condition reset() moves the model to.
class ArchiveSettingsModel final : public QObject {
    Q_OBJECT

public:
    explicit ArchiveSettingsModel(QObject* parent = nullptr);

    /// Whether newly created chats are archived and muted automatically. Off by
    /// default.
    void setArchiveAndMuteNewChats(bool on);
    [[nodiscard]] bool archiveAndMuteNewChats() const;

    /// Whether a chat stays in the archive when it becomes unmuted, instead of
    /// jumping back to the main list. Off by default.
    void setKeepArchivedUnmuted(bool on);
    [[nodiscard]] bool keepArchivedUnmuted() const;

    /// Whether archived chats stay in their folders instead of leaving them
    /// when they are archived. Off by default.
    void setKeepArchivedFolders(bool on);
    [[nodiscard]] bool keepArchivedFolders() const;

    /// Whether stories are archived together with their chat. Off by default.
    void setArchiveStories(bool on);
    [[nodiscard]] bool archiveStories() const;

    /// Restores the defaults: all four switches off. Emits changed() once if
    /// anything actually moved, and stays silent when the model is already at
    /// its defaults.
    void reset();

    /// True while every switch is off, which is the state reset() installs.
    [[nodiscard]] bool isDefault() const;

signals:
    /// Emitted once per real change to any of the switches above, and once for
    /// a reset() that actually restored something.
    void changed();

private:
    bool m_newChats = false;
    bool m_keepUnmuted = false;
    bool m_keepFolders = false;
    bool m_stories = false;
};

} // namespace beygram::telegram
