#pragma once

#include "TelegramTypes.h"

#include <QAbstractListModel>
#include <QVector>

namespace beygram::telegram {

class MessageListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        MessageIdRole = Qt::UserRole + 1,
        TextRole,
        TextHtmlRole,
        OutgoingRole,
        DateRole,
        SenderNameRole,
        PendingRole,
        FailedRole,
        EditedRole,
        ContentTypeRole,
        SenderUserIdRole,
        MediaFileIdRole,
        MediaFullFileIdRole,
        MediaWidthRole,
        MediaHeightRole,
        MediaReadyRole,
        MediaRevisionRole,
        ReplyToMessageIdRole,
        ReplyPreviewRole,
        ReactionsRole,
    };
    Q_ENUM(Role)

    explicit MessageListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const { return m_messages.size(); }
    [[nodiscard]] qint64 lastMessageId() const;
    [[nodiscard]] qint64 firstMessageId() const;

    void clear();
    void setMessages(QVector<MessageData> messages);
    void prependMessages(QVector<MessageData> messages);
    void addOrUpdate(const MessageData& message);
    bool replaceId(qint64 oldId, const MessageData& message);
    bool markFailed(qint64 messageId);
    bool markEdited(qint64 messageId);
    bool updateContent(qint64 messageId, const QJsonObject& content, bool edited);
    bool updateSenderName(qint64 userId, const QString& name);
    void markMediaReady(int fileId);
    void resolveReplyPreviews();
    void removeIds(const QVector<qint64>& ids);
    [[nodiscard]] bool contains(qint64 messageId) const;

signals:
    void countChanged();

private:
    [[nodiscard]] int indexForId(qint64 messageId) const;
    static void sortMessages(QVector<MessageData>& messages);

    QVector<MessageData> m_messages;
};

} // namespace beygram::telegram
