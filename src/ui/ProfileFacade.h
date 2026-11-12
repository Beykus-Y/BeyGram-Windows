#pragma once

#include "telegram/ProfileEditController.h"
#include "telegram/UserStatusModel.h"
#include "telegram/UsernameCheckController.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::ui {

/// The "edit my profile" form as one QML-facing object.
///
/// This is a presentation facade, not a second source of truth. The four
/// fields, the dirty comparison, the username availability state and the
/// presence label all live in the telegram objects this class owns, and every
/// property reads straight from them:
///
///   - dirty() is ProfileEditController::dirty(), so it is true exactly when a
///     request would really change something on the server. Whitespace-only
///     edits, a leading '@' on the username or a line feed in the bio do not
///     count, because the request builders normalise them away.
///   - usernameStatus() is UsernameCheckController::status() as text:
///     "idle", "invalid", "checking", "available" or "taken". It stays "idle"
///     until checkUsername() is asked; editing the username to a different
///     name drops a pending or answered check, because that answer belongs to
///     the old text.
///   - statusText() is UserStatusModel::label(), i.e. the presence line:
///     "online", "last seen recently", "last seen within a week",
///     "last seen within a month" or "offline". It is empty for Unknown and
///     Hidden, where there is nothing to show. The username's own status text
///     is deliberately not folded in here: usernameStatus() is the machine
///     readable code and the view words it.
///
/// The facade never touches TDLib, the network or storage. save() and
/// checkUsername() only announce what the caller should do; sending the
/// requests is the caller's job.
///
/// Invariants:
///  - dirty() is true exactly when the draft differs from the last loaded or
///    last saved profile.
///  - canSave() is true exactly when the first name is non-empty after
///    trimming, because TDLib rejects an empty first name.
///  - save() emits saveRequested() if and only if it returns true.
///  - usernameCheckRequested() is emitted only for text that passes
///    UsernameCheckController::validFormat(), and at most once per check.
class ProfileFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString firstName READ firstName WRITE setFirstName NOTIFY changed)
    Q_PROPERTY(QString lastName READ lastName WRITE setLastName NOTIFY changed)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY changed)
    Q_PROPERTY(QString bio READ bio WRITE setBio NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(bool canSave READ canSave NOTIFY changed)
    Q_PROPERTY(QString usernameStatus READ usernameStatus NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)

public:
    explicit ProfileFacade(QObject* parent = nullptr);

    /// The draft as the form shows it, i.e. what the user typed, not the
    /// normalised value a request would carry. The values are trimmed when
    /// they are sent, never when they are stored.
    [[nodiscard]] QString firstName() const;
    void setFirstName(const QString& firstName);

    [[nodiscard]] QString lastName() const;
    void setLastName(const QString& lastName);

    [[nodiscard]] QString username() const;
    void setUsername(const QString& username);

    [[nodiscard]] QString bio() const;
    void setBio(const QString& bio);

    /// True when the draft would change the stored profile.
    [[nodiscard]] bool dirty() const;

    /// True when the first name is not empty after trimming, i.e. when save()
    /// would emit. It does not require dirty(): submitting an unchanged form
    /// is allowed and produces no request, because the controller leaves
    /// unchanged fields out of requests().
    [[nodiscard]] bool canSave() const;

    /// Replaces the whole form with the profile the server reported. The
    /// profile becomes the new clean baseline and any username check is
    /// dropped, since it belonged to the previous account state. The presence
    /// is left alone: it is about the user, not about the form.
    Q_INVOKABLE void load(const QString& first, const QString& last, const QString& username,
                          const QString& bio);

    /// Emits saveRequested() with the trimmed draft when canSave() is true and
    /// returns whether it did. It does not clear dirty() or the form: the
    /// caller reports success by calling markSaved(), so a failed request
    /// leaves the user's text in place.
    Q_INVOKABLE bool save();

    /// Accepts the draft as the new stored profile after the requests
    /// succeeded, which clears dirty(). The text stays as typed, so a further
    /// edit is measured against the freshly saved values.
    Q_INVOKABLE void markSaved();

    /// "idle", "invalid", "checking", "available" or "taken".
    [[nodiscard]] QString usernameStatus() const;

    /// The presence line, e.g. "online" or "last seen recently"; empty when
    /// there is nothing to show.
    [[nodiscard]] QString statusText() const;

    /// Asks whether the username is free. Malformed text is recorded as
    /// "invalid" and never announced; a well-formed name moves to "checking"
    /// and emits usernameCheckRequested() with the canonical form (trimmed,
    /// '@' stripped, lower case). Asking again while a check is pending, or
    /// after an answer, is silent, because the answer already belongs to that
    /// name.
    Q_INVOKABLE void checkUsername();

    /// Records the answer to a pending check. Ignored unless the status is
    /// "checking", so a late reply for text the user has edited away cannot
    /// label the new text.
    Q_INVOKABLE void setUsernameAvailable(bool available);

    /// Sets the presence line from a status name and a last-seen timestamp.
    /// Both the short names ("online", "offline", "recently", "lastWeek",
    /// "lastMonth", "hidden") and the TDLib spellings ("userStatusOnline",
    /// ...) are accepted, case-insensitively; anything else clears the line.
    /// The model keeps the timestamp only for "offline", but the label is
    /// "offline" either way, so a timestamp-only change is not reported.
    Q_INVOKABLE void setPresence(const QString& kind, qint64 lastSeen);

signals:
    /// Emitted whenever any exposed value actually changed. A setter that
    /// stores the same text stays silent, so a binding does not repaint for a
    /// redundant update.
    void changed();

    /// Emitted by save() with the trimmed draft, for the caller to send.
    void saveRequested(const QString& first, const QString& last, const QString& username,
                       const QString& bio);

    /// Emitted by checkUsername() with the canonical username, for the caller
    /// to look up.
    void usernameCheckRequested(const QString& username);

private:
    void syncDraft();

    telegram::ProfileEditController m_edit;
    telegram::UsernameCheckController m_check;
    telegram::UserStatusModel m_presence;

    QString m_first;
    QString m_last;
    QString m_username;
    QString m_bio;
};

} // namespace beygram::ui
