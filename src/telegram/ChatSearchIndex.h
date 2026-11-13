#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// An in-memory index of chats for the chat-list search box.
///
/// A chat is identified by its chatId. upsert() stores the title and preview
/// trimmed and lowercased, so matching is case-insensitive and ignores
/// surrounding whitespace; the caller's strings are not modified. An upsert
/// with a chatId of zero or less is ignored.
///
/// Recency is the order in which upserts were accepted: every accepted upsert
/// makes that chat the most recent one, and search() with an empty (or
/// whitespace-only) query returns the chats newest first. Re-upserting the
/// chat that is already the most recent one with unchanged text does nothing.
///
/// A non-empty query is matched as a substring of the normalized title or
/// preview. Ranking is by score, then by recency:
///  - title prefix       4
///  - title substring    3
///  - preview prefix     2
///  - preview substring  1
/// A title hit therefore always outranks a preview hit, and a prefix outranks
/// a later occurrence within the same field. Equal scores read newest first.
///
/// search() clamps limit to 1..100.
///
/// remove() and clear() do nothing when there is nothing to remove.
/// changed() is emitted only when the stored set or its searchable order really
/// changes: an ignored chatId, an absent remove(), a clear() of an empty index
/// and a re-upsert of the newest chat with identical text are all silent.
///
/// The index is not persisted and is not thread-safe; its owner keeps it on the
/// thread that draws the chat list.
class ChatSearchIndex final : public QObject {
    Q_OBJECT

public:
    explicit ChatSearchIndex(QObject* parent = nullptr);

    /// Inserts or updates the chat and makes it the most recent one.
    ///
    /// title and preview are trimmed and lowercased before they are stored.
    /// A chatId of zero or less is ignored.
    void upsert(qint64 chatId, const QString& title, const QString& preview);

    /// Drops the chat. Removing a chat that is not indexed does nothing.
    void remove(qint64 chatId);

    /// Chat ids matching query, best first, at most `limit` of them.
    ///
    /// An empty or whitespace-only query matches every chat, newest first.
    /// `limit` is clamped to 1..100.
    [[nodiscard]] QVector<qint64> search(const QString& query, int limit = 30) const;

    [[nodiscard]] int count() const;

    /// Drops every chat. Clearing an empty index does nothing.
    void clear();

signals:
    void changed();

private:
    struct Entry {
        qint64 chatId = 0;
        QString title;
        QString preview;
        quint64 recency = 0;
    };

    /// Index of `chatId` in m_entries, or -1.
    [[nodiscard]] int indexOf(qint64 chatId) const;

    /// Whether the entry at `index` has the highest recency in the index.
    [[nodiscard]] bool isNewest(int index) const;

    /// Ranking score of one entry for a non-empty, normalized query, or -1
    /// when the query occurs in neither field.
    [[nodiscard]] static int scoreFor(const Entry& entry, const QString& query);

    QVector<Entry> m_entries;
    quint64 m_nextRecency = 0;
};

} // namespace beygram::telegram
