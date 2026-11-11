#pragma once

#include "ChatFolders.h"

#include <QAbstractListModel>
#include <QVector>

namespace beygram::telegram {

/// Exposes ChatFoldersState as a flat list for the folder strip. The main list
/// (id 0) is always row 0; custom folders keep the parser order unless their
/// positions require sorting.
class ChatFolderListModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        FolderIdRole = Qt::UserRole + 1,
        TitleRole,
        IconRole,
        UnreadCountRole,
        IsMainRole,
    };
    Q_ENUM(Role)

    explicit ChatFolderListModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const { return m_folders.size(); }
    [[nodiscard]] qint64 folderIdAt(int row) const;

    void setState(const ChatFoldersState& state);
    void clear();

signals:
    void countChanged();

private:
    QVector<ChatFolderInfo> m_folders;
};

} // namespace beygram::telegram
