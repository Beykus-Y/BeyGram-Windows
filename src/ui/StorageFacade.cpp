#include "StorageFacade.h"

#include <QHash>
#include <QVector>

#include <limits>
#include <utility>

namespace beygram::ui {

namespace {

/// The separator between the segments of a row: space, middle dot, space.
QString separator()
{
    return QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ");
}

/// A decimal integer from a QML-provided string, or 0 when the text is not a
/// number. Every field this facade parses treats 0 as "no value" (no count, no
/// size), so a malformed entry is rendered as absent instead of being guessed
/// at.
qint64 toInt64(const QString& text)
{
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    return ok ? value : 0;
}

/// A file count as the model stores it: never negative, never past INT_MAX.
int toFileCount(const QString& text)
{
    const qint64 value = toInt64(text);
    if (value <= 0)
        return 0;

    return value > std::numeric_limits<int>::max() ? std::numeric_limits<int>::max()
                                                   : static_cast<int>(value);
}

/// A byte size the screen can show: never negative. A malformed or negative
/// payload cannot make a row claim it would free less than nothing.
qint64 toBytes(const QString& text)
{
    const qint64 value = toInt64(text);
    return value > 0 ? value : 0;
}

/// The candidates the cleanup controller works with: one entry per stored row.
/// The rows are already de-duplicated, so no kind can be counted twice.
QHash<QString, qint64> candidatesOf(const QVector<telegram::StorageRow>& rows)
{
    QHash<QString, qint64> result;
    result.reserve(rows.size());

    for (const telegram::StorageRow& row : rows)
        result.insert(row.kind, row.bytes);

    return result;
}

} // namespace

StorageFacade::StorageFacade(QObject* parent) : QObject(parent)
{
    // Each model already suppresses no-op changes, so forwarding is enough: a
    // redundant update emits nothing, a real one repaints every binding.
    connect(&m_stats, &telegram::StorageStatsModel::changed, this,
            &StorageFacade::handleModelChanged);
    connect(&m_cleanup, &telegram::CacheCleanupController::changed, this,
            &StorageFacade::handleModelChanged);
    connect(&m_dataUsage, &telegram::DataUsageModel::changed, this,
            &StorageFacade::handleModelChanged);
    connect(&m_policy, &telegram::AutoDownloadPolicy::changed, this,
            &StorageFacade::handleModelChanged);
}

QStringList StorageFacade::kinds() const
{
    const QVector<telegram::StorageRow> rows = m_stats.rows();

    QStringList result;
    result.reserve(rows.size());

    for (const telegram::StorageRow& row : rows)
        result.append(row.kind);

    return result;
}

QStringList StorageFacade::sizes() const
{
    const QVector<telegram::StorageRow> rows = m_stats.rows();

    QStringList result;
    result.reserve(rows.size());

    for (const telegram::StorageRow& row : rows)
        result.append(telegram::DataUsageModel::formatBytes(row.bytes));

    return result;
}

void StorageFacade::setStorage(const QStringList& kinds, const QStringList& fileCounts,
                               const QStringList& bytes)
{
    QVector<telegram::StorageRow> rows;
    rows.reserve(kinds.size());

    for (qsizetype i = 0; i < kinds.size(); ++i) {
        telegram::StorageRow row;
        row.kind = kinds.at(i);
        row.fileCount = i < fileCounts.size() ? toFileCount(fileCounts.at(i)) : 0;
        row.bytes = i < bytes.size() ? toBytes(bytes.at(i)) : 0;
        rows.append(row);
    }

    // The breakdown and the cleanup candidates describe the same rows, so they
    // move together and the view repaints once.
    m_silent = true;
    m_stats.setRows(std::move(rows));

    // The candidates come from the model's cleaned rows: a kind that was
    // dropped or de-duplicated cannot linger as something the user could tick.
    m_cleanup.setCandidates(candidatesOf(m_stats.rows()));
    m_silent = false;

    if (m_pending) {
        m_pending = false;
        emit changed();
    }
}

QString StorageFacade::totalText() const
{
    return telegram::DataUsageModel::formatBytes(m_stats.totalBytes());
}

void StorageFacade::setSelection(const QStringList& kinds)
{
    // The controller owns the rules: unknown kinds ignored, duplicates
    // dropped, result sorted.
    m_cleanup.setSelected(kinds);
}

QStringList StorageFacade::selectedKinds() const
{
    return m_cleanup.selected();
}

QString StorageFacade::selectedText() const
{
    return telegram::DataUsageModel::formatBytes(m_cleanup.selectedBytes());
}

bool StorageFacade::canClean() const
{
    return m_cleanup.canClean();
}

bool StorageFacade::clean()
{
    if (!m_cleanup.canClean())
        return false;

    // Snapshot the selection before marking running: emitting may re-enter the
    // facade, and the payload must be the set the user actually ticked.
    const QStringList kinds = m_cleanup.selected();
    m_cleanup.setRunning(true);
    emit cleanRequested(kinds);
    return true;
}

void StorageFacade::setRunning(bool running)
{
    m_cleanup.setRunning(running);
}

bool StorageFacade::autoDownload() const
{
    return m_policy.autoDownload();
}

void StorageFacade::setAutoDownload(bool on)
{
    m_policy.setAutoDownload(on);
}

void StorageFacade::setUsage(const QStringList& categories, const QStringList& sent,
                             const QStringList& received)
{
    QVector<telegram::UsageRow> rows;
    rows.reserve(categories.size());

    for (qsizetype i = 0; i < categories.size(); ++i) {
        telegram::UsageRow row;
        row.category = categories.at(i);
        row.bytesSent = i < sent.size() ? toBytes(sent.at(i)) : 0;
        row.bytesReceived = i < received.size() ? toBytes(received.at(i)) : 0;
        rows.append(row);
    }

    // Dropping blank categories, collapsing duplicates and clamping negatives
    // are the model's rules, so the facade does not repeat them.
    m_dataUsage.setRows(std::move(rows));
}

QStringList StorageFacade::usage() const
{
    const QVector<telegram::UsageRow> rows = m_dataUsage.rows();

    QStringList result;
    result.reserve(rows.size());

    for (const telegram::UsageRow& row : rows) {
        result.append(row.category + separator()
                      + telegram::DataUsageModel::formatBytes(row.bytesSent)
                      + QStringLiteral(" sent") + separator()
                      + telegram::DataUsageModel::formatBytes(row.bytesReceived)
                      + QStringLiteral(" received"));
    }

    return result;
}

void StorageFacade::handleModelChanged()
{
    if (m_silent) {
        m_pending = true;
        return;
    }

    emit changed();
}

} // namespace beygram::ui
