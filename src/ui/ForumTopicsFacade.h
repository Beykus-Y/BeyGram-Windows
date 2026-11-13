#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

namespace beygram::ui {

/// QML-facing view model for the topic list of a forum supergroup.
///
/// The facade owns no transport and performs no I/O, logging or clock reads.
/// The owner publishes what getForumTopics reported through setTopics(), QML
/// renders ids()/titles(), and a tap comes back out as topicActivated() or
/// pinRequested() for the owner to map onto the requests in
/// telegram/ForumTopics.h. setTopics() is also the refresh path after a pin
/// toggle, so the optimistic flip cannot outlive the server's answer.
///
/// `ids` and `titles` are parallel and cut to the shorter one; a missing
/// `unread` entry means 0 and a missing `pinned` entry means false, so a caller
/// may omit either auxiliary list without losing topics. A row whose id is not
/// a positive decimal integer or whose title is blank is dropped, mirroring
/// forumTopicsFromResponse(): a row that cannot be addressed or shown must not
/// reach QML. Ids are stored as canonical decimal strings ("007" becomes "7"),
/// so openTopic() and togglePinned() accept the same value QML received.
///
/// unreadCounts() and pinnedFlags() are additions to the three properties the
/// panel contract requires; a QML delegate cannot render the unread badge or
/// the pin state without the per-row values. They stay parallel to ids().
///
/// Every mutator emits changed() at most once and only when the exposed state
/// really moved, so a refresh that reports the same topics stays silent.
class ForumTopicsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList titles READ titles NOTIFY changed)
    Q_PROPERTY(QStringList ids READ ids NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QVariantList unreadCounts READ unreadCounts NOTIFY changed)
    Q_PROPERTY(QVariantList pinnedFlags READ pinnedFlags NOTIFY changed)

public:
    explicit ForumTopicsFacade(QObject* parent = nullptr);

    /// Row titles in server order, parallel to ids().
    [[nodiscard]] QStringList titles() const;

    /// Canonical decimal topic ids in server order.
    [[nodiscard]] QStringList ids() const;

    /// Number of rows that survived validation.
    [[nodiscard]] int count() const;

    /// Unread counts parallel to ids(), as integers.
    [[nodiscard]] QVariantList unreadCounts() const;

    /// Pinned flags parallel to ids(), as booleans.
    [[nodiscard]] QVariantList pinnedFlags() const;

    /// Replaces the topic list with one refresh.
    ///
    /// `ids` and `titles` are parallel and truncated to the shorter one, so a
    /// missing title is never invented. `unread` holds decimal counts; anything
    /// unparsable or non-positive becomes 0, and an entry missing from a short
    /// list also becomes 0. `pinned` holds booleans as QML spells them
    /// ("true"/"1" for pinned, anything else for not pinned); an entry missing
    /// from a short list is not pinned. A row with an unparsable or non-positive
    /// id, or a blank title, is dropped. Silent when the surviving rows are
    /// identical to the current ones.
    Q_INVOKABLE void setTopics(const QStringList& ids, const QStringList& titles,
                               const QStringList& unread, const QStringList& pinned);

    /// Emits topicActivated() with the stored id when `topicId` addresses a
    /// loaded row. Unknown, malformed or empty ids are ignored, so a stale row
    /// cannot open a topic that has disappeared. No state changes.
    Q_INVOKABLE void openTopic(const QString& topicId);

    /// Flips the pinned flag of the row addressed by `topicId`, emits changed()
    /// once and then pinRequested() with the new value. Unknown ids are
    /// ignored. The flip is optimistic: the owner should send
    /// toggleForumTopicIsPinnedRequest() and call setTopics() with the server's
    /// list when it answers.
    Q_INVOKABLE void togglePinned(const QString& topicId);

    /// Drops every row. Silent when the list is already empty.
    Q_INVOKABLE void clear();

signals:
    /// Any exposed row state moved: the list itself or one row's pin flag.
    void changed();

    /// A loaded row was opened; topicId is its canonical decimal id.
    void topicActivated(const QString& topicId);

    /// togglePinned() flipped a row. The owner should send
    /// toggleForumTopicIsPinnedRequest(chatId, topicId.toLongLong(), pinned) and
    /// refresh with setTopics() when the response arrives.
    void pinRequested(const QString& topicId, bool pinned);

private:
    struct Row {
        QString id;
        QString title;
        int unread = 0;
        bool pinned = false;

        bool operator==(const Row&) const = default;
    };

    [[nodiscard]] int indexOf(const QString& topicId) const;

    QVector<Row> m_rows;
};

} // namespace beygram::ui
