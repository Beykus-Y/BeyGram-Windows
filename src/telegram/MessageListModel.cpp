#include "MessageListModel.h"

#include "TextFormatting.h"

#include <algorithm>

namespace beygram::telegram {

MessageListModel::MessageListModel(QObject* parent) : QAbstractListModel(parent)
{
}

int MessageListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_messages.size();
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size())
        return {};

    const MessageData& message = m_messages.at(index.row());
    switch (role) {
    case MessageIdRole: return message.id;
    case TextRole: return message.text;
    case TextHtmlRole: return message.textHtml;
    case OutgoingRole: return message.outgoing;
    case DateRole: return message.date;
    case SenderNameRole: return message.senderName;
    case PendingRole: return message.pending;
    case FailedRole: return message.failed;
    case EditedRole: return message.edited;
    case ContentTypeRole: return message.contentType;
    case SenderUserIdRole: return message.senderUserId;
    case MediaFileIdRole: return message.mediaFileId;
    case MediaFullFileIdRole: return message.mediaFullFileId;
    case MediaWidthRole: return message.mediaWidth;
    case MediaHeightRole: return message.mediaHeight;
    case MediaReadyRole: return message.mediaReady;
    case MediaRevisionRole: return message.mediaRevision;
    case ReplyToMessageIdRole: return message.replyToMessageId;
    case ReplyPreviewRole: return message.replyPreview;
    case ReactionsRole: return message.reactionsJson;
    default: return {};
    }
}

QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {
        {MessageIdRole, "messageId"},
        {TextRole, "text"},
        {TextHtmlRole, "textHtml"},
        {OutgoingRole, "outgoing"},
        {DateRole, "date"},
        {SenderNameRole, "senderName"},
        {PendingRole, "pending"},
        {FailedRole, "failed"},
        {EditedRole, "edited"},
        {ContentTypeRole, "contentType"},
        {SenderUserIdRole, "senderUserId"},
        {MediaFileIdRole, "mediaFileId"},
        {MediaFullFileIdRole, "mediaFullFileId"},
        {MediaWidthRole, "mediaWidth"},
        {MediaHeightRole, "mediaHeight"},
        {MediaReadyRole, "mediaReady"},
        {MediaRevisionRole, "mediaRevision"},
        {ReplyToMessageIdRole, "replyToMessageId"},
        {ReplyPreviewRole, "replyPreview"},
        {ReactionsRole, "reactionsJson"},
    };
}

qint64 MessageListModel::lastMessageId() const
{
    return m_messages.isEmpty() ? 0 : m_messages.last().id;
}

qint64 MessageListModel::firstMessageId() const
{
    return m_messages.isEmpty() ? 0 : m_messages.first().id;
}

void MessageListModel::clear()
{
    if (m_messages.isEmpty())
        return;
    beginResetModel();
    m_messages.clear();
    endResetModel();
    emit countChanged();
}

void MessageListModel::setMessages(QVector<MessageData> messages)
{
    beginResetModel();
    m_messages = std::move(messages);
    sortMessages(m_messages);
    endResetModel();
    emit countChanged();
}

void MessageListModel::prependMessages(QVector<MessageData> messages)
{
    if (messages.isEmpty())
        return;

    QVector<MessageData> merged = std::move(messages);
    merged += m_messages;
    setMessages(std::move(merged));
}

void MessageListModel::addOrUpdate(const MessageData& message)
{
    const int existing = indexForId(message.id);
    if (existing >= 0) {
        m_messages[existing] = message;
        const QModelIndex changed = index(existing);
        emit dataChanged(changed, changed);
        return;
    }

    int insertAt = m_messages.size();
    while (insertAt > 0 && m_messages.at(insertAt - 1).id > message.id)
        --insertAt;

    beginInsertRows({}, insertAt, insertAt);
    m_messages.insert(insertAt, message);
    endInsertRows();
    emit countChanged();
}

bool MessageListModel::replaceId(qint64 oldId, const MessageData& message)
{
    const int existing = indexForId(oldId);
    if (existing < 0) {
        addOrUpdate(message);
        return false;
    }

    // Replacing a temporary id can change its sort position. Remove the old
    // row first, then use addOrUpdate so a server message already added from
    // the sendMessage response is updated instead of duplicated.
    beginResetModel();
    m_messages.removeAt(existing);
    endResetModel();
    emit countChanged();
    addOrUpdate(message);
    return true;
}

bool MessageListModel::markFailed(qint64 messageId)
{
    const int existing = indexForId(messageId);
    if (existing < 0)
        return false;
    m_messages[existing].pending = false;
    m_messages[existing].failed = true;
    const QModelIndex changed = index(existing);
    emit dataChanged(changed, changed);
    return true;
}

bool MessageListModel::markEdited(qint64 messageId)
{
    const int existing = indexForId(messageId);
    if (existing < 0)
        return false;
    m_messages[existing].edited = true;
    const QModelIndex changed = index(existing);
    emit dataChanged(changed, changed, {EditedRole});
    return true;
}

bool MessageListModel::updateContent(qint64 messageId, const QJsonObject& content, bool edited)
{
    const int existing = indexForId(messageId);
    if (existing < 0)
        return false;

    MessageData& message = m_messages[existing];
    message.text = messageContentText(content);
    message.contentType = messageContentLabel(content);
    const QJsonObject formatted =
        content.contains(QStringLiteral("text"))
        ? content.value(QStringLiteral("text")).toObject()
        : content.value(QStringLiteral("caption")).toObject();
    message.textHtml = formatTdlibText(formatted).html;
    if (message.text.isEmpty())
        message.text = QStringLiteral("[%1]").arg(message.contentType);
    if (edited)
        message.edited = true;
    const QModelIndex changed = index(existing);
    emit dataChanged(changed, changed);
    return true;
}

bool MessageListModel::updateSenderName(qint64 userId, const QString& name)
{
    bool changedAny = false;
    for (int row = 0; row < m_messages.size(); ++row) {
        MessageData& message = m_messages[row];
        if (message.senderUserId != userId || message.senderName == name)
            continue;
        message.senderName = name;
        const QModelIndex changed = index(row);
        emit dataChanged(changed, changed, {SenderNameRole});
        changedAny = true;
    }
    return changedAny;
}

void MessageListModel::markMediaReady(int fileId)
{
    if (fileId == 0)
        return;
    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].mediaFileId != fileId)
            continue;
        if (m_messages[row].mediaReady)
            continue;
        m_messages[row].mediaReady = true;
        ++m_messages[row].mediaRevision;
        const QModelIndex changed = index(row);
        emit dataChanged(changed, changed, {MediaReadyRole, MediaRevisionRole});
    }
}

void MessageListModel::resolveReplyPreviews()
{
    for (int row = 0; row < m_messages.size(); ++row) {
        MessageData& message = m_messages[row];
        if (message.replyToMessageId == 0 || !message.replyPreview.isEmpty())
            continue;
        const int targetRow = indexForId(message.replyToMessageId);
        if (targetRow < 0 || targetRow == row)
            continue;
        const MessageData& target = m_messages.at(targetRow);
        QString preview = target.text;
        if (preview.isEmpty())
            preview = QStringLiteral("Message");
        if (!target.senderName.isEmpty())
            preview = target.senderName + QStringLiteral(": ") + preview;
        message.replyPreview = preview;
        const QModelIndex changed = index(row);
        emit dataChanged(changed, changed, {ReplyPreviewRole});
    }
}

void MessageListModel::removeIds(const QVector<qint64>& ids)
{
    if (ids.isEmpty())
        return;

    for (int row = m_messages.size() - 1; row >= 0; --row) {
        if (!ids.contains(m_messages.at(row).id))
            continue;
        beginRemoveRows({}, row, row);
        m_messages.removeAt(row);
        endRemoveRows();
    }
    emit countChanged();
}

bool MessageListModel::contains(qint64 messageId) const
{
    return indexForId(messageId) >= 0;
}

int MessageListModel::indexForId(qint64 messageId) const
{
    for (int row = 0; row < m_messages.size(); ++row) {
        if (m_messages.at(row).id == messageId)
            return row;
    }
    return -1;
}

void MessageListModel::sortMessages(QVector<MessageData>& messages)
{
    std::sort(messages.begin(), messages.end(),
              [](const MessageData& left, const MessageData& right) {
                  if (left.id == right.id)
                      return left.date < right.date;
                  return left.id < right.id;
              });
}

} // namespace beygram::telegram
