#include "ProfileEditController.h"

#include "ProfileEdit.h"

#include <utility>

namespace beygram::telegram {
namespace {

/// The four fields as they would travel to TDLib. Both the dirty check and the
/// request list are derived from this shape, so "is it dirty?" and "which
/// requests are built?" can never disagree.
struct ProfileFields {
    QString first;
    QString last;
    QString username;
    QString bio;
};

/// Asks the ProfileEdit builders to normalise the values: the names are trimmed,
/// the bio is folded onto one line and the username loses whitespace and leading
/// '@' characters. The builders are the single source of truth for those rules,
/// so the comparison here matches the payload byte for byte.
ProfileFields normalized(const QString& first, const QString& last, const QString& username,
                         const QString& bio)
{
    const QJsonObject name = setNameRequest(first, last);
    const QJsonObject usernameRequest = setUsernameRequest(username);
    const QJsonObject bioRequest = setBioRequest(bio);
    return ProfileFields{
        name.value(QStringLiteral("first_name")).toString(),
        name.value(QStringLiteral("last_name")).toString(),
        usernameRequest.value(QStringLiteral("username")).toString(),
        bioRequest.value(QStringLiteral("bio")).toString(),
    };
}

bool sameFields(const ProfileFields& left, const ProfileFields& right)
{
    return left.first == right.first && left.last == right.last
        && left.username == right.username && left.bio == right.bio;
}

} // namespace

ProfileEditController::ProfileEditController(QObject* parent)
    : QObject(parent)
{
}

void ProfileEditController::setOriginal(QString first, QString last, QString username, QString bio)
{
    const ProfileFields beforeDraft = normalized(m_first, m_last, m_username, m_bio);
    const ProfileFields beforeOriginal = normalized(o_first, o_last, o_username, o_bio);

    o_first = std::move(first);
    o_last = std::move(last);
    o_username = std::move(username);
    o_bio = std::move(bio);

    // A loaded profile replaces the form: the draft starts out identical to the
    // server's copy, so a fresh load never looks edited.
    m_first = o_first;
    m_last = o_last;
    m_username = o_username;
    m_bio = o_bio;

    if (!sameFields(beforeDraft, normalized(m_first, m_last, m_username, m_bio))
        || !sameFields(beforeOriginal, normalized(o_first, o_last, o_username, o_bio))) {
        emit changed();
    }
}

void ProfileEditController::setDraft(QString first, QString last, QString username, QString bio)
{
    const ProfileFields before = normalized(m_first, m_last, m_username, m_bio);

    m_first = std::move(first);
    m_last = std::move(last);
    m_username = std::move(username);
    m_bio = std::move(bio);

    // Whitespace-only typing keeps the same normalised value and stays silent,
    // because nothing the user could save has changed.
    if (!sameFields(before, normalized(m_first, m_last, m_username, m_bio)))
        emit changed();
}

bool ProfileEditController::dirty() const
{
    return !sameFields(normalized(m_first, m_last, m_username, m_bio),
                       normalized(o_first, o_last, o_username, o_bio));
}

QVector<QJsonObject> ProfileEditController::requests() const
{
    const ProfileFields draft = normalized(m_first, m_last, m_username, m_bio);
    const ProfileFields original = normalized(o_first, o_last, o_username, o_bio);

    QVector<QJsonObject> requests;
    if (draft.first != original.first || draft.last != original.last)
        requests.append(setNameRequest(m_first, m_last));
    if (draft.bio != original.bio)
        requests.append(setBioRequest(m_bio));
    if (draft.username != original.username)
        requests.append(setUsernameRequest(m_username));
    return requests;
}

void ProfileEditController::markSaved()
{
    const ProfileFields before = normalized(o_first, o_last, o_username, o_bio);

    o_first = m_first;
    o_last = m_last;
    o_username = m_username;
    o_bio = m_bio;

    // Saving an already clean profile changes nothing the caller can observe.
    if (!sameFields(before, normalized(o_first, o_last, o_username, o_bio)))
        emit changed();
}

} // namespace beygram::telegram
