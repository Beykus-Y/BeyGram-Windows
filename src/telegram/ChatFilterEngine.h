#pragma once

#include "TelegramTypes.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The criteria one chat-list view is filtered by.
///
/// An empty text and all-false flags accept every chat. A folderId of 0 means
/// the whole list; any other value restricts the result to the chats that were
/// registered for that folder with setFolderMembers().
struct ChatFilterQuery {
    QString text;
    bool unreadOnly = false;
    bool mutedOnly = false;
    bool pinnedOnly = false;
    qint64 folderId = 0;
};

/// Filters a copy of the chat list without touching the model or the source.
///
/// apply() returns the chats that satisfy every active criterion, in their
/// original order, so the view can keep TDLib's order (or the order chosen by
/// ChatListSortController). Text is matched case-insensitively against the
/// title and the preview; the three flags and the folder restriction are
/// ANDed. The engine owns no chats and no requests: it only holds the current
/// query and the folder membership it was last given. Every setter is a no-op
/// for the value it already holds, so changed() fires once per real transition.
class ChatFilterEngine final : public QObject {
    Q_OBJECT

public:
    explicit ChatFilterEngine(QObject* parent = nullptr);

    /// Replaces the criteria. A query equal to the current one is ignored.
    void setQuery(const ChatFilterQuery& query);
    [[nodiscard]] ChatFilterQuery query() const;

    /// Replaces the members of one folder. Duplicate chat ids are dropped and
    /// the first occurrence keeps the caller's order; an empty list forgets the
    /// folder, so querying it matches nothing. Changing the members of a folder
    /// that is not the current query is still a real state change.
    void setFolderMembers(qint64 folderId, const QVector<qint64>& chatIds);

    /// Returns a filtered copy of the given chats, preserving their order.
    [[nodiscard]] QVector<ChatData> apply(const QVector<ChatData>& chats) const;

    /// Counts the chats apply() would return, without copying them.
    [[nodiscard]] int matchCount(const QVector<ChatData>& chats) const;

    /// Drops the query and every cached folder membership.
    void clear();

signals:
    /// Emitted whenever the query or a folder membership really changed.
    void changed();

private:
    ChatFilterQuery m_query;
    QHash<qint64, QVector<qint64>> m_members;
};

} // namespace beygram::telegram
