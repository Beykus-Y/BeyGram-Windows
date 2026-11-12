#include "PrivacySettingsModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

// A setting name is the key of every lookup, so a nameless row could never be
// found or updated and is dropped when rows are set.
bool hasSettingName(const PrivacySettingRow& row)
{
    return !row.setting.isEmpty();
}

bool containsSetting(const QVector<PrivacySettingRow>& rows, const QString& setting)
{
    for (const PrivacySettingRow& row : rows) {
        if (row.setting == setting)
            return true;
    }
    return false;
}

} // namespace

PrivacySettingsModel::PrivacySettingsModel(QObject* parent)
    : QObject(parent)
{
}

void PrivacySettingsModel::setRows(QVector<PrivacySettingRow> rows)
{
    // Keep the first row of each setting: rows arrive in the order the caller
    // read them, and the first answer is the one already on screen, so it wins
    // over a later duplicate instead of making the visible value depend on
    // merge order.
    QVector<PrivacySettingRow> unique;
    unique.reserve(rows.size());
    for (PrivacySettingRow& row : rows) {
        if (!hasSettingName(row))
            continue;
        if (containsSetting(unique, row.setting))
            continue;
        unique.append(std::move(row));
    }

    // A reload that brings the same data is not a change: repainting the screen
    // for it would also restart every animation bound to changed().
    if (unique == m_rows)
        return;

    m_rows = std::move(unique);
    emit changed();
}

QVector<PrivacySettingRow> PrivacySettingsModel::rows() const
{
    return m_rows;
}

QString PrivacySettingsModel::valueFor(const QString& setting) const
{
    for (const PrivacySettingRow& row : m_rows) {
        if (row.setting == setting)
            return row.value;
    }
    return {};
}

bool PrivacySettingsModel::setValue(const QString& setting, const QString& value)
{
    for (PrivacySettingRow& row : m_rows) {
        if (row.setting != setting)
            continue;

        // A read-only row is rejected before the comparison, so the caller is
        // told the write did not happen even if the value happens to match.
        if (!row.editable)
            return false;
        if (row.value == value)
            return false;

        row.value = value;
        emit changed();
        return true;
    }

    // Unknown setting: no row is created, because a setting the server did not
    // report is not one this client may invent.
    return false;
}

void PrivacySettingsModel::clear()
{
    if (m_rows.isEmpty())
        return;

    m_rows.clear();
    emit changed();
}

} // namespace beygram::telegram
