#include "GroupCreationController.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

/// Limits from the pinned TDLib schema (docs/WINDOWS_TDLIB.md): a title is
/// 1-128 characters and a description 0-255. Both are counted in Unicode code
/// points so one emoji is one character and never split in half.
constexpr int kMaxTitleLength = 128;
constexpr int kMaxDescriptionLength = 255;

/// Telegram usernames are 5-32 characters from [a-z0-9_].
constexpr int kMinUsernameLength = 5;
constexpr int kMaxUsernameLength = 32;

/// A basic group accepts at most 200 members at creation in this client. The
/// controller enforces the same ceiling for every kind, so the member list has
/// one bound no matter which chat is being created.
constexpr int kMaxMembers = 200;

/// Keeps at most maxCodePoints characters. QString is UTF-16, so a surrogate
/// pair is advanced as one character instead of being cut in half.
QString truncateToCodePoints(const QString& text, int maxCodePoints)
{
    qsizetype index = 0;
    int codePoints = 0;
    while (index < text.size() && codePoints < maxCodePoints) {
        const bool surrogatePair = text.at(index).isHighSurrogate()
            && index + 1 < text.size() && text.at(index + 1).isLowSurrogate();
        index += surrogatePair ? 2 : 1;
        ++codePoints;
    }
    return text.left(index);
}

/// Reduces what the user typed to the form a username is stored in: surrounding
/// whitespace and leading '@' characters (as pasted from a t.me link) are
/// dropped and the rest is lower cased. Trimming runs again after the '@' so
/// " @ Alice " still ends up as "alice". An '@' inside the name is not removed:
/// it is not a username character and validUsername() rejects it rather than
/// silently inventing a different name.
QString normalizeUsername(const QString& raw)
{
    QString text = raw.trimmed();
    while (text.startsWith(QLatin1Char('@')))
        text.remove(0, 1);
    return text.trimmed().toLower();
}

/// True for 5 to 32 characters from [a-z0-9_]. The caller has already
/// normalised the text, so upper case and a leading '@' cannot reach this
/// check.
bool validUsername(const QString& username)
{
    if (username.size() < kMinUsernameLength || username.size() > kMaxUsernameLength)
        return false;

    for (const QChar character : username) {
        const bool letter = character >= QLatin1Char('a') && character <= QLatin1Char('z');
        const bool digit = character >= QLatin1Char('0') && character <= QLatin1Char('9');
        if (!letter && !digit && character != QLatin1Char('_'))
            return false;
    }
    return true;
}

QJsonObject titleRequest(const QString& title)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatTitle")},
        {QStringLiteral("chat_id"), 0},
        {QStringLiteral("title"), title},
    };
}

QJsonObject descriptionRequest(const QString& description)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatDescription")},
        {QStringLiteral("chat_id"), 0},
        {QStringLiteral("description"), description},
    };
}

QJsonObject usernameRequest(const QString& username)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatUsername")},
        {QStringLiteral("chat_id"), 0},
        {QStringLiteral("username"), username},
    };
}

QJsonObject addMemberRequest(qint64 userId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("addChatMember")},
        {QStringLiteral("chat_id"), 0},
        {QStringLiteral("user_id"), userId},
        {QStringLiteral("forward_limit"), 0},
    };
}

} // namespace

GroupCreationController::GroupCreationController(QObject* parent) : QObject(parent)
{
}

void GroupCreationController::begin(GroupKind kind)
{
    // begin() always means "start over". Re-entering an already empty draft of
    // the same kind changes nothing a view could observe, so it stays silent.
    const bool unchanged = m_active && m_kind == kind && m_title.isEmpty()
        && m_description.isEmpty() && m_username.isEmpty() && !m_public
        && m_members.isEmpty();
    if (unchanged)
        return;

    m_kind = kind;
    m_title.clear();
    m_description.clear();
    m_username.clear();
    m_public = false;
    m_members.clear();
    m_active = true;
    emit changed();
}

void GroupCreationController::reset()
{
    const bool unchanged = !m_active && m_kind == GroupKind::Group && m_title.isEmpty()
        && m_description.isEmpty() && m_username.isEmpty() && !m_public
        && m_members.isEmpty();
    if (unchanged)
        return;

    m_kind = GroupKind::Group;
    m_title.clear();
    m_description.clear();
    m_username.clear();
    m_public = false;
    m_members.clear();
    m_active = false;
    emit changed();
}

bool GroupCreationController::active() const
{
    return m_active;
}

GroupKind GroupCreationController::kind() const
{
    return m_kind;
}

void GroupCreationController::setTitle(const QString& title)
{
    if (!m_active)
        return;

    const QString value = truncateToCodePoints(title.trimmed(), kMaxTitleLength);
    if (value == m_title)
        return;

    m_title = value;
    emit changed();
}

QString GroupCreationController::title() const
{
    return m_title;
}

void GroupCreationController::setDescription(const QString& description)
{
    if (!m_active)
        return;

    const QString value = truncateToCodePoints(description, kMaxDescriptionLength);
    if (value == m_description)
        return;

    m_description = value;
    emit changed();
}

QString GroupCreationController::description() const
{
    return m_description;
}

void GroupCreationController::setPublic(bool isPublic, const QString& username)
{
    if (!m_active)
        return;

    const QString value = normalizeUsername(username);
    if (m_public == isPublic && m_username == value)
        return;

    m_public = isPublic;
    m_username = value;
    emit changed();
}

bool GroupCreationController::isPublic() const
{
    return m_public;
}

QString GroupCreationController::username() const
{
    return m_username;
}

bool GroupCreationController::addMember(qint64 userId)
{
    if (!m_active || userId <= 0 || m_members.contains(userId)
        || m_members.size() >= kMaxMembers) {
        return false;
    }

    m_members.append(userId);
    emit changed();
    return true;
}

bool GroupCreationController::removeMember(qint64 userId)
{
    if (!m_active)
        return false;

    const qsizetype index = m_members.indexOf(userId);
    if (index < 0)
        return false;

    m_members.removeAt(index);
    emit changed();
    return true;
}

QVector<qint64> GroupCreationController::members() const
{
    return m_members;
}

bool GroupCreationController::valid() const
{
    return m_active && !m_title.isEmpty() && (!m_public || validUsername(m_username));
}

QVector<QJsonObject> GroupCreationController::requests() const
{
    if (!valid())
        return {};

    QVector<QJsonObject> requests;
    requests.reserve(3 + (m_public ? 1 : 0) + m_members.size());

    // The create call is sent with an empty member list: every member is added
    // by a following addChatMember, so all three kinds share one member path
    // and a failed invitation can be reported per user.
    if (m_kind == GroupKind::Group) {
        requests.append(QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("createNewBasicGroupChat")},
            {QStringLiteral("user_ids"), QJsonArray()},
            {QStringLiteral("title"), m_title},
            {QStringLiteral("message_auto_delete_time"), 0},
        });
    } else {
        requests.append(QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("createNewSupergroupChat")},
            {QStringLiteral("title"), m_title},
            {QStringLiteral("is_forum"), false},
            {QStringLiteral("is_channel"), m_kind == GroupKind::Channel},
            {QStringLiteral("description"), m_description},
            {QStringLiteral("location"), QJsonValue::Null},
            {QStringLiteral("message_auto_delete_time"), 0},
            {QStringLiteral("for_import"), false},
        });
    }

    // The title and description are also part of the create payload because
    // the schema requires them there. The setters make the post-create part of
    // the plan uniform across kinds; a caller that trusts the create answer may
    // drop the two no-op setters before sending.
    requests.append(titleRequest(m_title));
    requests.append(descriptionRequest(m_description));

    if (m_public)
        requests.append(usernameRequest(m_username));

    for (const qint64 userId : m_members)
        requests.append(addMemberRequest(userId));

    return requests;
}

} // namespace beygram::telegram
