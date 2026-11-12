#include "ProfileFacade.h"

#include <QChar>

namespace beygram::ui {

namespace {

using Status = telegram::UsernameCheckController::Status;

/// Reduces what the user typed to the form the check controller stores: a
/// pasted leading '@' is dropped, the text is trimmed and lower cased. Mirrors
/// UsernameCheckController's own normalisation, which it does not expose.
QString canonicalUsername(const QString& raw)
{
    QString text = raw.trimmed();
    while (text.startsWith(QLatin1Char('@')))
        text.remove(0, 1);
    return text.trimmed().toLower();
}

/// The QML-facing spelling of a check status. These strings are the contract
/// ProfileEditor.qml switches on, so they stay lower case and stable.
QString statusName(Status status)
{
    switch (status) {
    case Status::Invalid:
        return QStringLiteral("invalid");
    case Status::Checking:
        return QStringLiteral("checking");
    case Status::Available:
        return QStringLiteral("available");
    case Status::Taken:
        return QStringLiteral("taken");
    case Status::Idle:
        break;
    }
    return QStringLiteral("idle");
}

/// Maps a presence name to the model's kind. Both the short names the facade
/// documents and the TDLib UserStatus spellings are accepted, because callers
/// receive one or the other depending on whether the value came from a payload
/// or from the UI. Comparison is case-insensitive and ignores surrounding
/// whitespace; an unknown name means "nothing was reported".
telegram::PresenceKind presenceKind(const QString& kind)
{
    const QString name = kind.trimmed().toLower();

    if (name == QLatin1String("online") || name == QLatin1String("userstatusonline"))
        return telegram::PresenceKind::Online;
    if (name == QLatin1String("offline") || name == QLatin1String("userstatusoffline"))
        return telegram::PresenceKind::Offline;
    if (name == QLatin1String("recently") || name == QLatin1String("userstatusrecently"))
        return telegram::PresenceKind::Recently;
    if (name == QLatin1String("lastweek") || name == QLatin1String("userstatuslastweek"))
        return telegram::PresenceKind::LastWeek;
    if (name == QLatin1String("lastmonth") || name == QLatin1String("userstatuslastmonth"))
        return telegram::PresenceKind::LastMonth;
    if (name == QLatin1String("hidden") || name == QLatin1String("userstatusempty"))
        return telegram::PresenceKind::Hidden;

    return telegram::PresenceKind::Unknown;
}

} // namespace

ProfileFacade::ProfileFacade(QObject* parent) : QObject(parent)
{
    // The facade emits changed() from its own mutators instead of forwarding
    // the models' changed() signals: every exposed value is written through a
    // mutator here, and doing both would report each edit twice.
}

QString ProfileFacade::firstName() const
{
    return m_first;
}

void ProfileFacade::setFirstName(const QString& firstName)
{
    if (m_first == firstName)
        return;

    m_first = firstName;
    syncDraft();
    emit changed();
}

QString ProfileFacade::lastName() const
{
    return m_last;
}

void ProfileFacade::setLastName(const QString& lastName)
{
    if (m_last == lastName)
        return;

    m_last = lastName;
    syncDraft();
    emit changed();
}

QString ProfileFacade::username() const
{
    return m_username;
}

void ProfileFacade::setUsername(const QString& username)
{
    if (m_username == username)
        return;

    m_username = username;

    // An answer belongs to the text it was asked for. Once the text is a
    // different username the old answer would label the wrong name, so the
    // check is dropped; the next checkUsername() reports the new state. Text
    // that only differs in decoration (" @Alice" for "alice") keeps its
    // answer, because it is the same username.
    if (canonicalUsername(m_username) != m_check.input())
        m_check.reset();

    syncDraft();
    emit changed();
}

QString ProfileFacade::bio() const
{
    return m_bio;
}

void ProfileFacade::setBio(const QString& bio)
{
    if (m_bio == bio)
        return;

    m_bio = bio;
    syncDraft();
    emit changed();
}

bool ProfileFacade::dirty() const
{
    return m_edit.dirty();
}

bool ProfileFacade::canSave() const
{
    return !m_first.trimmed().isEmpty();
}

void ProfileFacade::load(const QString& first, const QString& last, const QString& username,
                         const QString& bio)
{
    const QString previousFirst = m_first;
    const QString previousLast = m_last;
    const QString previousUsername = m_username;
    const QString previousBio = m_bio;
    const bool previousDirty = m_edit.dirty();
    const Status previousStatus = m_check.status();

    m_first = first;
    m_last = last;
    m_username = username;
    m_bio = bio;

    // setOriginal() also resets the draft onto the loaded values, so a fresh
    // load always starts clean; there is no partial merge to do here.
    m_edit.setOriginal(first, last, username, bio);
    m_check.reset();

    if (m_first != previousFirst || m_last != previousLast || m_username != previousUsername
        || m_bio != previousBio || m_edit.dirty() != previousDirty
        || m_check.status() != previousStatus) {
        emit changed();
    }
}

bool ProfileFacade::save()
{
    if (!canSave())
        return false;

    // The builders trim and fold anyway; sending the trimmed text keeps the
    // payload identical to what dirty() compared against.
    emit saveRequested(m_first.trimmed(), m_last.trimmed(), m_username.trimmed(),
                       m_bio.trimmed());
    return true;
}

void ProfileFacade::markSaved()
{
    const bool wasDirty = m_edit.dirty();
    m_edit.markSaved();

    // The draft is kept as typed, so only the clean/dirty answer can change.
    if (wasDirty != m_edit.dirty())
        emit changed();
}

QString ProfileFacade::usernameStatus() const
{
    return statusName(m_check.status());
}

QString ProfileFacade::statusText() const
{
    return m_presence.label();
}

void ProfileFacade::checkUsername()
{
    const Status previousStatus = m_check.status();

    // setInput() decides between idle, invalid and checking, and keeps an
    // existing answer for the same canonical text. Only a transition into
    // Checking means a request is now due.
    m_check.setInput(m_username);

    if (m_check.status() != previousStatus)
        emit changed();

    if (previousStatus != Status::Checking && m_check.status() == Status::Checking)
        emit usernameCheckRequested(m_check.input());
}

void ProfileFacade::setUsernameAvailable(bool available)
{
    const Status previousStatus = m_check.status();
    m_check.setResult(available);

    if (m_check.status() != previousStatus)
        emit changed();
}

void ProfileFacade::setPresence(const QString& kind, qint64 lastSeen)
{
    const QString previousText = m_presence.label();

    // The model owns the normalisation (a timestamp is kept only for offline,
    // negative values clamp to 0) and its label is the only thing exposed, so
    // a timestamp-only change is not reported.
    m_presence.setStatus(presenceKind(kind), lastSeen);

    if (m_presence.label() != previousText)
        emit changed();
}

void ProfileFacade::syncDraft()
{
    m_edit.setDraft(m_first, m_last, m_username, m_bio);
}

} // namespace beygram::ui
