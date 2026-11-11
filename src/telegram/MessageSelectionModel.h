#pragma once

#include <QObject>
#include <QSet>
#include <QVector>

namespace beygram::telegram {

/// Tracks the set of messages selected for a bulk action. Selection is a
/// membership test only, so the order in which messages are toggled never
/// matters; ids must be positive and are ignored otherwise.
class MessageSelectionModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY selectionChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY selectionChanged)

public:
    explicit MessageSelectionModel(QObject* parent = nullptr);

    [[nodiscard]] int count() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] QVector<qint64> selectedIds() const;
    [[nodiscard]] bool contains(qint64 messageId) const;

    Q_INVOKABLE void toggle(qint64 messageId);
    Q_INVOKABLE void select(qint64 messageId);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void selectAll(const QVector<qint64>& messageIds);

signals:
    void selectionChanged();

private:
    QSet<qint64> m_selected;
};

} // namespace beygram::telegram
