#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One row of the contact list as the view needs it.
///
/// ContactSearch.h already has ContactInfo, but that struct describes what a
/// `searchContacts` reply carries (display name, username, photo, bot and
/// premium flags) and has no presence at all. Sorting by "who is online" or by
/// "when were they last seen" therefore needs a row of its own instead of
/// widening ContactInfo with fields TDLib never puts in that reply. Callers map
/// their ContactInfo (or user object) onto a ContactRow when they build the
/// list.
///
/// `lastSeen` is a Unix timestamp in seconds, taken from TDLib's
/// `user.status`; 0 means "unknown" (last seen recently, hidden by privacy
/// settings, or never observed) and sorts as the oldest value.
struct ContactRow {
    qint64 userId = 0;
    QString name;
    bool online = false;
    qint64 lastSeen = 0;
};

/// The orders the contact list can be shown in.
enum class ContactSort {
    Name,        ///< Alphabetical, compared with QString::localeAwareCompare.
    LastSeen,    ///< Most recently seen first.
    OnlineFirst, ///< Online contacts first, each group alphabetical by name.
};

/// Sorts and filters a copy of the contact list without touching the view.
///
/// sorted() takes the contacts by value, filters them and performs a stable
/// sort, so contacts that tie on every key of the current mode keep their input
/// order and the caller's vector is never mutated. onlineOnly() is a filter, not
/// an order: it drops offline contacts before the sort, so the remaining rows
/// are ordered exactly as they would be without it.
///
/// Every setter is a no-op for the value it already holds, so changed() fires
/// once per real transition and the view does not re-sort for nothing. Neither
/// sorted() nor indexLetters() emits anything.
class ContactSortModel final : public QObject {
    Q_OBJECT

public:
    explicit ContactSortModel(QObject* parent = nullptr);

    /// Selects the comparison key. Changing it re-emits changed().
    void setSort(ContactSort sort);
    [[nodiscard]] ContactSort sort() const { return m_sort; }

    /// Hides offline contacts from sorted() and indexLetters(). Defaults to
    /// false, which keeps every contact.
    void setOnlineOnly(bool onlineOnly);
    [[nodiscard]] bool onlineOnly() const { return m_onlineOnly; }

    /// Returns a stably sorted (and, with onlineOnly(), filtered) copy.
    ///
    /// Name: locale-aware alphabetical order. LastSeen: newest timestamp first,
    /// unknown (0) last. OnlineFirst: online contacts before offline ones, and
    /// each group alphabetical by name; lastSeen is not consulted.
    [[nodiscard]] QVector<ContactRow> sorted(QVector<ContactRow> contacts) const;

    /// Returns the section letters present in the contacts, for a fast-scroll
    /// index: the uppercase first letter of every name, deduplicated and sorted
    /// A-Z.
    ///
    /// Leading and trailing whitespace is ignored, so "  Ada" files under A.
    /// Names that are empty, start with a digit or punctuation, or whose first
    /// letter has no single uppercase form (such as the German sharp s) add no
    /// letter. The onlineOnly() filter applies here too, so the index always
    /// matches the rows sorted() would return.
    [[nodiscard]] QStringList indexLetters(const QVector<ContactRow>& contacts) const;

signals:
    /// Emitted whenever the sort order or the online filter really changed.
    void changed();

private:
    ContactSort m_sort = ContactSort::Name;
    bool m_onlineOnly = false;
};

} // namespace beygram::telegram
