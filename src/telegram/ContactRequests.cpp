#include "ContactRequests.h"

#include <QJsonArray>
#include <QSet>

namespace beygram::telegram {
namespace {

// importedContact limits both name parts to 64 characters.
constexpr int kMaxNameLength = 64;

/// Cuts text to at most maxLength Unicode code points without splitting a
/// surrogate pair, which would leave half an emoji in the payload.
QString limitedToCodePoints(const QString& text, int maxLength)
{
    int codePoints = 0;
    int index = 0;
    while (index < text.size()) {
        if (codePoints == maxLength)
            return text.left(index);

        const QChar character = text.at(index);
        const bool surrogatePair = character.isHighSurrogate() && index + 1 < text.size()
            && text.at(index + 1).isLowSurrogate();
        index += surrogatePair ? 2 : 1;
        ++codePoints;
    }
    return text;
}

QString contactName(const QString& name)
{
    return limitedToCodePoints(name.trimmed(), kMaxNameLength);
}

/// The importedContact object shared by addContact and importContacts.
///
/// `note` is omitted on purpose: TDLib edits a note only when the field is
/// present, so leaving it out keeps whatever note the contact already has.
QJsonObject importedContact(const QString& phone, const QString& firstName,
                            const QString& lastName)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("importedContact")},
        {QStringLiteral("phone_number"), phone.trimmed()},
        {QStringLiteral("first_name"), contactName(firstName)},
        {QStringLiteral("last_name"), contactName(lastName)},
    };
}

} // namespace

QJsonObject getContactsRequest(int offset, int limit)
{
    // No paging exists in this schema, so the window stays out of the payload.
    Q_UNUSED(offset);
    Q_UNUSED(limit);

    return QJsonObject{{QStringLiteral("@type"), QStringLiteral("getContacts")}};
}

QJsonObject addContactRequest(qint64 userId, const QString& firstName,
                              const QString& lastName, const QString& phone)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("addContact")},
        {QStringLiteral("user_id"), userId},
        {QStringLiteral("contact"), importedContact(phone, firstName, lastName)},
        {QStringLiteral("share_phone_number"), false},
    };
}

QJsonObject removeContactsRequest(const QVector<qint64>& userIds)
{
    QJsonArray ids;
    QSet<qint64> seen;
    seen.reserve(userIds.size());

    for (const qint64 userId : userIds) {
        if (userId <= 0 || seen.contains(userId))
            continue;
        seen.insert(userId);
        ids.append(userId);
    }

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("removeContacts")},
        {QStringLiteral("user_ids"), ids},
    };
}

QJsonObject importContactsRequest(const QVector<QPair<QString, QString>>& phoneAndName)
{
    QJsonArray contacts;
    for (const QPair<QString, QString>& entry : phoneAndName) {
        // Positional: one entry per pair, even when the number is unknown.
        contacts.append(importedContact(entry.first, entry.second, QString()));
    }

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("importContacts")},
        {QStringLiteral("contacts"), contacts},
    };
}

} // namespace beygram::telegram
