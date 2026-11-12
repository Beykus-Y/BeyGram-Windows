#include "ContactSortModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

// Locale-aware so accented and non-Latin names land where the user's language
// expects them, not where their code points happen to fall.
int compareByName(const ContactRow& left, const ContactRow& right)
{
    return QString::localeAwareCompare(left.name, right.name);
}

} // namespace

ContactSortModel::ContactSortModel(QObject* parent)
    : QObject(parent)
{
}

void ContactSortModel::setSort(ContactSort sort)
{
    if (m_sort == sort)
        return;
    m_sort = sort;
    emit changed();
}

void ContactSortModel::setOnlineOnly(bool onlineOnly)
{
    if (m_onlineOnly == onlineOnly)
        return;
    m_onlineOnly = onlineOnly;
    emit changed();
}

QVector<ContactRow> ContactSortModel::sorted(QVector<ContactRow> contacts) const
{
    // The filter runs first so the sort only ever sees the rows that will be
    // displayed, and so it cannot change the relative order of the survivors.
    if (m_onlineOnly) {
        QVector<ContactRow> online;
        online.reserve(contacts.size());
        for (const ContactRow& contact : contacts) {
            if (contact.online)
                online.append(contact);
        }
        contacts = std::move(online);
    }

    const ContactSort sort = m_sort;
    std::stable_sort(contacts.begin(), contacts.end(),
                     [sort](const ContactRow& left, const ContactRow& right) {
                         switch (sort) {
                         case ContactSort::Name:
                             return compareByName(left, right) < 0;
                         case ContactSort::LastSeen:
                             // Newest first; a tie falls through to false so
                             // the stable sort keeps the input order.
                             return left.lastSeen > right.lastSeen;
                         case ContactSort::OnlineFirst:
                             if (left.online != right.online)
                                 return left.online;
                             return compareByName(left, right) < 0;
                         }
                         return false;
                     });

    return contacts;
}

QStringList ContactSortModel::indexLetters(const QVector<ContactRow>& contacts) const
{
    QStringList letters;
    for (const ContactRow& contact : contacts) {
        if (m_onlineOnly && !contact.online)
            continue;

        const QString name = contact.name.trimmed();
        if (name.isEmpty())
            continue;

        const QChar first = name.at(0);
        if (!first.isLetter())
            continue;

        const QString letter = QString(first).toUpper();
        // A letter whose uppercase form needs several code points (the German
        // sharp s) cannot label a single section.
        if (letter.size() != 1)
            continue;
        if (letters.contains(letter))
            continue;

        letters.append(letter);
    }

    std::sort(letters.begin(), letters.end());
    return letters;
}

} // namespace beygram::telegram
