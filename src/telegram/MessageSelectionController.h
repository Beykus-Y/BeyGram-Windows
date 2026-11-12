#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Owns the ordered set of message ids selected for a bulk action such as
/// forwarding. The ids are kept sorted and unique so callers always observe a
/// stable order regardless of the sequence in which messages were toggled.
///
/// Non-positive ids are ignored, and every mutating call emits changed() only
/// when the selection actually changed.
class MessageSelectionController final : public QObject {
    Q_OBJECT

public:
    explicit MessageSelectionController(QObject* parent = nullptr);

    /// Adds the message when it is not selected yet, removes it otherwise.
    void toggle(qint64 id);

    /// Adds the message when it is not selected yet; selecting twice is a no-op.
    void select(qint64 id);

    /// Drops the whole selection. Clearing an empty selection is a no-op.
    void clear();

    [[nodiscard]] bool active() const;
    [[nodiscard]] QVector<qint64> ids() const;
    [[nodiscard]] int count() const;

    /// Snapshot of the selected ids in ascending order, safe to hand to a
    /// forward request while the selection keeps changing.
    [[nodiscard]] QVector<qint64> idsForForward() const;

signals:
    /// Emitted whenever the selection actually changed.
    void changed();

private:
    QVector<qint64> m_ids;
};

} // namespace beygram::telegram
