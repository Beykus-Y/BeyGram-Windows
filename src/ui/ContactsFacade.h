#pragma once

#include "telegram/ContactSortModel.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace beygram::ui {

/// QML-facing facade over the contact list.
///
/// It owns a ContactSortModel and keeps the last snapshot of rows the caller
/// handed over. names() is that snapshot already filtered by query() and
/// onlineOnly() and sorted by sortMode(), so a ListView can bind straight to it
/// and resolve a tapped row back to a user id with userIdAt(). The index strip
/// reads indexLetters(), which is derived from the same rows.
///
/// The facade holds no transport and performs no logging: it never calls
/// TDLib. A query that also has to reach the server is only announced through
/// searchRequested(); the caller answers it by calling setContacts() again with
/// whatever the search returned. Until then the list keeps showing the locally
/// filtered snapshot, so typing never blanks the panel.
///
/// Invariants:
///  - names() and indexLetters() describe exactly the same rows, so the index
///    never offers a letter the list cannot scroll to.
///  - names() is sorted by sortMode() and filtered by both query() and
///    onlineOnly().
///  - userIdAt(i) and activate(i) address the i-th row of names(); an index
///    outside that range yields an empty id and emits nothing.
///  - Every observable change emits changed() exactly once, and setters that
///    receive the value they already hold emit nothing.
class ContactsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList names READ names NOTIFY changed)
    Q_PROPERTY(QStringList indexLetters READ indexLetters NOTIFY changed)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY changed)
    Q_PROPERTY(bool onlineOnly READ onlineOnly WRITE setOnlineOnly NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)

public:
    explicit ContactsFacade(QObject* parent = nullptr);

    /// Display names of the visible rows, in sort order.
    [[nodiscard]] QStringList names() const;

    /// Uppercase initials present in the visible rows, deduplicated and sorted
    /// A-Z, for the fast-scroll strip.
    [[nodiscard]] QStringList indexLetters() const;

    /// Replaces the contact snapshot. The four lists are index-aligned: entry
    /// `i` of each one describes the same contact, so `ids[i]` belongs to
    /// `names[i]`, `online[i]` and `lastSeen[i]`.
    ///
    /// `ids` and `lastSeen` are strings because that is what QML hands over;
    /// `lastSeen` is a Unix timestamp in seconds. An id that is not a positive
    /// integer is dropped (no TDLib user has one, and the row could never be
    /// activated). `online` accepts "true"/"1"/"yes"/"online"
    /// (case-insensitive) and treats every other value as offline. A `lastSeen`
    /// that does not parse, or is negative, becomes 0, which the sort model
    /// already documents as "unknown".
    ///
    /// Lists shorter than `ids` are tolerated: the missing entries default to
    /// an empty name, offline and an unknown timestamp. That keeps a presence
    /// update from failing just because it carried fewer columns.
    ///
    /// Always emits changed(), even when the new snapshot looks identical: the
    /// caller passes fresh server data, and the facade does not compare whole
    /// contact lists to decide whether the view is stale.
    Q_INVOKABLE void setContacts(const QStringList& ids, const QStringList& names,
                                 const QStringList& online, const QStringList& lastSeen);

    /// The order the list is shown in: "name" (default), "lastSeen" or
    /// "onlineFirst". Reading always returns the canonical camelCase token.
    /// Unknown tokens are ignored, so a stale saved setting cannot silently
    /// fall back to a different order than the one on screen.
    [[nodiscard]] QString sortMode() const;
    void setSortMode(const QString& mode);

    /// Hides offline contacts from names() and indexLetters(). Defaults to
    /// false, which keeps every contact.
    [[nodiscard]] bool onlineOnly() const;
    void setOnlineOnly(bool on);

    /// The trimmed filter text. Empty means "no filter". Filtering is local and
    /// case-insensitive on the display name.
    [[nodiscard]] QString query() const;
    void setQuery(const QString& query);

    /// True while the caller is still fetching contacts. The facade only
    /// forwards the flag; it never sets it itself.
    [[nodiscard]] bool loading() const;
    Q_INVOKABLE void setLoading(bool loading);

    /// The user id of the visible row at `index`, as a decimal string, or an
    /// empty string when the index is out of range.
    Q_INVOKABLE QString userIdAt(int index) const;

    /// Announces a tap on the visible row at `index` through
    /// contactActivated(). The panel's row click goes through here so the id
    /// and the row it came from can never disagree; out of range indices are
    /// ignored.
    Q_INVOKABLE void activate(int index);

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted with the user id of the row the user picked.
    void contactActivated(const QString& userId);

    /// Emitted with a non-empty trimmed query the caller may send to TDLib.
    /// Clearing the query emits changed() but no request: there is nothing to
    /// search for.
    void searchRequested(const QString& query);

private:
    [[nodiscard]] bool matchesQuery(const QString& name) const;

    /// The filtered, sorted rows, rebuilt lazily after every mutation.
    [[nodiscard]] const QVector<telegram::ContactRow>& visibleRows() const;

    void invalidate();

    telegram::ContactSortModel* m_model = nullptr;
    QVector<telegram::ContactRow> m_contacts;
    QString m_query;
    bool m_loading = false;

    // Cache, not state: it is only ever rebuilt from m_contacts, m_query and
    // the model's settings.
    mutable QVector<telegram::ContactRow> m_visible;
    mutable bool m_visibleValid = false;
};

} // namespace beygram::ui
