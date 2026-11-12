#pragma once

#include "WebAppInfo.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Owns the lifetime of one open Web App (Mini App) session.
///
/// The controller is pure state: it never talks to TDLib and never touches the
/// network, so a view can bind to it and a caller can decide what to do with the
/// stored ids. A session is active only after open() accepted a positive chat id
/// and a positive bot id; every mutator that needs a session is a no-op while it
/// is inactive, and changed() is emitted once per real change so no view
/// repaints for a write that stored the same value.
class WebAppSessionController final : public QObject {
    Q_OBJECT

public:
    explicit WebAppSessionController(QObject* parent = nullptr);

    /// Starts a session for the Web App that belongs to botUserId inside chatId.
    ///
    /// Ignored, leaving any current session untouched, unless both ids are
    /// positive. Opening while a session is already active replaces it and
    /// clears the previous loading flag and error, because they described the
    /// old session. Reopening the identical session with no flag set stores
    /// nothing new and does not emit changed().
    ///
    /// info is kept as given; botUserId() is the authoritative launch id, so a
    /// description that carries no bot id is still usable here.
    void open(qint64 chatId, qint64 botUserId, const WebAppInfo& info);

    /// Ends the active session and resets every field. A no-op while inactive.
    void close();

    /// True while a session is open, i.e. both stored ids are positive.
    [[nodiscard]] bool active() const;

    [[nodiscard]] qint64 chatId() const { return m_chatId; }
    [[nodiscard]] qint64 botUserId() const { return m_botUserId; }

    /// The description as passed to open(); default-constructed while inactive.
    [[nodiscard]] WebAppInfo info() const { return m_info; }

    /// Marks the session as loading or done loading. Ignored while inactive, and
    /// ignored when the flag already holds that value.
    void setLoading(bool loading);
    [[nodiscard]] bool loading() const { return m_loading; }

    /// Stores a user-facing error for the session; an empty string clears it.
    /// Ignored while inactive, and ignored when the text is unchanged.
    void setError(const QString& error);
    [[nodiscard]] QString error() const { return m_error; }

    /// Drops the session and every flag, like close(); quiet when already empty.
    void clear();

signals:
    /// Emitted whenever a stored value actually changed.
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_botUserId = 0;
    WebAppInfo m_info;
    bool m_loading = false;
    QString m_error;
};

} // namespace beygram::telegram
