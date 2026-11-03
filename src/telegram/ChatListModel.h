#pragma once

#include "TelegramTypes.h"

#include <QAbstractListModel>
#include <QVector>

namespace beygram::telegram {

class ChatListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        ChatIdRole = Qt::UserRole + 1,
        TitleRole,
        PreviewRole,
        UnreadCountRole,
        PinnedRole,
        MutedRole,
        LastDateRole,
        InitialsRole,
        AccentIndexRole,
        IsChannelRole,
        ChatTypeRole,
    };
    Q_ENUM(Role)

    explicit ChatListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const { return m_chats.size(); }

    void setChats(QVector<ChatData> chats);
    void clear();
    [[nodiscard]] qint64 chatIdAt(int row) const;

signals:
    void countChanged();

private:
    QVector<ChatData> m_chats;
};

} // namespace beygram::telegram
