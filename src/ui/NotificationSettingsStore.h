#pragma once

#include <QObject>
#include <QString>

namespace beygram::ui {

/// The persisted notification preferences.
///
/// These are the client-wide switches that decide whether a message may surface
/// a desktop notification at all, and how much of the chat may appear in it.
/// They are deliberately independent of per-chat mute state: muting a chat is
/// stored with the chat, `notifyMutedChats` is the global override.
struct NotificationSettings {
    bool enabled = true;
    bool showPreview = true;
    bool sound = true;
    bool notifyMutedChats = false;
};

/// JSON-backed notification settings.
///
/// The file is `{"version":1,...}`. Missing, damaged or wrongly typed fields
/// fall back to the defaults above, so a corrupt file can never make the client
/// unusable, and only real JSON booleans are accepted for the switches.
/// Setters only write (and emit changed()) when a value actually changes. Writes
/// go through QSaveFile, so a crash mid-write keeps the previous file intact.
/// Nothing about the file contents is logged.
class NotificationSettingsStore final : public QObject {
    Q_OBJECT

public:
    explicit NotificationSettingsStore(QString filePath, QObject* parent = nullptr);

    /// Replaces the in-memory settings with the file contents. Emits changed()
    /// once if that actually altered anything; never writes the file.
    void load();
    [[nodiscard]] NotificationSettings settings() const;
    [[nodiscard]] QString filePath() const;

    void setEnabled(bool value);
    void setShowPreview(bool value);
    void setSound(bool value);
    void setNotifyMutedChats(bool value);

signals:
    void changed();

private:
    void persist();
    QString m_filePath;
    NotificationSettings m_settings;
};

} // namespace beygram::ui
