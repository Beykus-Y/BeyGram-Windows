#pragma once

#include <QSortFilterProxyModel>
#include <QString>

namespace beygram::telegram {

class ChatFilterModel final : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit ChatFilterModel(QObject* parent = nullptr);

    [[nodiscard]] QString filterText() const { return m_filterText; }
    [[nodiscard]] int count() const { return rowCount(); }
    void setFilterText(const QString& text);

signals:
    void filterTextChanged();
    void countChanged();

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow,
                                        const QModelIndex& sourceParent) const override;

private:
    QString m_filterText;
};

} // namespace beygram::telegram
