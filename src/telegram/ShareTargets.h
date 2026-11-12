#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One destination row of the "share to..." screen.
///
/// `kind` is a short label supplied by the caller (for example "group" or
/// "channel"); the model never interprets it. `subtitle` is a secondary line
/// such as a username or member count. Both are searched by filtered().
struct ShareTarget {
    qint64 chatId = 0;
    QString title;
    QString subtitle;
    QString kind;
    int unreadCount = 0;
};

/// State for the "share to..." picker: the chats the user may send to and the
/// destinations they used recently, shown above the full list.
///
/// The class holds no transport and no logging. It only keeps the picker state
/// consistent so the UI can bind to it and the send path can use a chat id.
///
/// Invariants:
///  - targets() only contains positive, unique chat ids in the order given.
///  - recent() only contains positive, unique ids, newest first, and never more
///    than the cap that was last applied (eight by default).
///  - recentTargets() is recent() resolved through targets(): an id whose chat
///    is not in targets() is skipped rather than shown with empty fields.
///  - changed() is emitted only when observable state actually changed.
class ShareTargetModel final : public QObject {
    Q_OBJECT

public:
    /// The number of recent destinations kept unless addRecent() asks for
    /// another cap.
    static constexpr int kDefaultMaxRecent = 8;

    explicit ShareTargetModel(QObject* parent = nullptr);

    /// Replaces the destinations. Non-positive chat ids are dropped and a
    /// repeated id keeps its first entry, fields included; the incoming order is
    /// preserved. Recents are not touched, because a chat list that reloads must
    /// not erase destinations the user actually used.
    void setTargets(QVector<ShareTarget> targets);
    [[nodiscard]] QVector<ShareTarget> targets() const;

    /// Case-insensitive substring match on the title or the subtitle, in stored
    /// order. The query is trimmed first, so an empty or blank query returns
    /// every target. `kind` is not searched: it is a label, not a name.
    [[nodiscard]] QVector<ShareTarget> filtered(const QString& query) const;

    [[nodiscard]] int count() const;

    /// Replaces the recent chat ids with `chatIds`, newest first.
    ///
    /// Non-positive ids are dropped, a repeated id keeps its first position and
    /// the list is capped at kDefaultMaxRecent. Ids without a matching target
    /// are kept: the chat list may simply not have loaded yet, and
    /// recentTargets() projects only the ids that are present.
    void setRecent(const QVector<qint64>& chatIds);
    [[nodiscard]] QVector<qint64> recent() const;

    /// The recent ids resolved through targets(), newest first. Ids that are no
    /// longer among the targets are skipped, so the result never carries an
    /// empty title.
    [[nodiscard]] QVector<ShareTarget> recentTargets() const;

    /// Records one used chat as the newest recent entry.
    ///
    /// A chat that is already in the list moves to the front instead of being
    /// duplicated, and the oldest entries beyond `maxRecent` are dropped.
    /// `maxRecent` is clamped to at least one, so the chat just used is never
    /// dropped immediately. A non-positive chat id is ignored. Emits changed()
    /// only when the resulting order really changed, so re-sending to the
    /// newest chat is silent.
    void addRecent(qint64 chatId, int maxRecent = kDefaultMaxRecent);

    /// Drops both the destinations and the recents.
    void clear();

signals:
    /// Emitted whenever the observable state actually changed.
    void changed();

private:
    QVector<ShareTarget> m_targets;
    QVector<qint64> m_recent;
};

} // namespace beygram::telegram
