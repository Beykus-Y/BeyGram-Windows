#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {

/// Tracks one QR-login attempt from the link the UI shows to the answer from
/// the phone.
///
/// begin() accepts only a link TDLib can turn into a QR code: either the
/// `tg://login?token=...` URI or an `https://` URL. The link is trimmed before
/// it is stored, so the controller always holds the exact value that was
/// accepted. An empty or foreign link is ignored and leaves the current
/// attempt untouched, which keeps a stray clipboard paste from ending a login
/// that is already in progress.
///
/// The marks follow the server's order: waiting -> scanned -> confirmed. A
/// confirmation can also arrive directly from waiting when the phone does not
/// report the scan first. Every mark that does not fit the current state is a
/// no-op, so late or duplicated TDLib updates cannot move the attempt
/// backwards. expired is terminal until begin() starts a new attempt or reset()
/// drops everything.
///
/// changed() is emitted once per real transition and linkChanged() only when
/// the stored link really differs, so views bound to them do not repaint for
/// nothing.
class QrLoginController final : public QObject {
    Q_OBJECT

public:
    explicit QrLoginController(QObject* parent = nullptr);

    /// Starts a new attempt from a QR-login link. The link is trimmed and must
    /// then be non-empty and start with "tg://login?token=" or "https://";
    /// anything else is ignored and the current state is kept. A valid link
    /// always replaces the previous attempt and moves the state to waiting.
    void begin(const QString& link);

    /// Records that the phone scanned the code. Accepted only while waiting.
    void markScanned();

    /// Records that the user confirmed the login on the phone. Accepted from
    /// scanned, or directly from waiting when the scan update was skipped.
    void markConfirmed();

    /// Records that the QR code timed out. Accepted only while the attempt is
    /// active, i.e. waiting or scanned; confirmed and expired are final.
    void markExpired();

    /// Drops the link and returns to idle. Silent when already idle.
    void reset();

    /// True while the attempt can still reach confirmed: waiting or scanned.
    [[nodiscard]] bool active() const;

    /// The accepted link, trimmed, or an empty string while idle. The link is
    /// kept after confirmation and expiry so the UI can still refer to the
    /// attempt; reset() and the next begin() replace it.
    [[nodiscard]] QString link() const;

    /// One of "idle", "waiting", "scanned", "confirmed" or "expired".
    [[nodiscard]] QString state() const;

    /// True only in the expired state.
    [[nodiscard]] bool expired() const;

signals:
    /// Emitted whenever the state actually changed.
    void changed();

    /// Emitted whenever the stored link actually changed. reset() reports an
    /// empty string when it drops a link.
    void linkChanged(const QString& link);

private:
    enum class State { Idle, Waiting, Scanned, Confirmed, Expired };

    QString m_link;
    State m_state = State::Idle;
};

} // namespace beygram::telegram
