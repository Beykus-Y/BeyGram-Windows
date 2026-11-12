#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One line of the privacy settings screen: the TDLib setting name (e.g.
/// "userPrivacySettingShowStatus"), the value the account currently has (a
/// userPrivacySettingRule name, or a plain value for the non-rule settings) and
/// whether this client is allowed to change that line.
///
/// The struct holds no pointers, no session state and no secrets, so a snapshot
/// of it can be cached per account and copied across threads.
struct PrivacySettingRow {
    QString setting;
    QString value;
    bool editable = true;

    bool operator==(const PrivacySettingRow& other) const = default;
};

/// Editable list of privacy settings for the settings screen.
///
/// The model is a plain value holder: it never talks to TDLib and never reads
/// the clock. The caller fetches getUserPrivacySettingRules / getPrivacySettings,
/// hands the flattened answer to setRows(), and sends an update itself when
/// setValue() reports a change. Because nothing here performs I/O, the tests
/// stay deterministic.
///
/// Invariants:
///
///  - `setting` is unique. setRows() keeps the first row for a setting and drops
///    later duplicates, so a caller that merges several TDLib answers cannot end
///    up with two lines fighting over one value. Rows without a setting name are
///    dropped as well; they cannot be looked up or updated.
///  - setValue() only touches a row that already exists and is editable. An
///    unknown setting or a read-only row is rejected without creating a row, so
///    a value the server would refuse is never shown as if it had been applied.
///  - changed() is emitted only when the visible rows really differ. Writing the
///    value a row already has, rejecting an unknown setting, or clearing an empty
///    model are all silent, so a refresh that brings the same data does not
///    repaint the screen or re-trigger a binding loop.
class PrivacySettingsModel final : public QObject {
    Q_OBJECT

public:
    explicit PrivacySettingsModel(QObject* parent = nullptr);

    /// Replaces every row with `rows`, in the given order. Empty setting names
    /// and duplicate settings (keeping the first occurrence) are dropped.
    /// changed() is emitted only when the resulting rows differ from the current
    /// ones.
    void setRows(QVector<PrivacySettingRow> rows);

    /// A copy of the rows, in display order.
    [[nodiscard]] QVector<PrivacySettingRow> rows() const;

    /// The value stored for `setting`, or an empty string when the model has no
    /// such row. An empty result therefore means "unknown setting", not "unset".
    [[nodiscard]] QString valueFor(const QString& setting) const;

    /// Stores `value` for `setting`. Returns true only when the stored value
    /// actually changed, which is also the only case that emits changed(). A
    /// missing row, a non-editable row and a write of the value already present
    /// all return false and leave the model untouched.
    bool setValue(const QString& setting, const QString& value);

    /// Forgets every row, e.g. when the account is signed out or the settings
    /// screen is reloaded from scratch. Emits changed() only if there was
    /// something to forget.
    void clear();

signals:
    void changed();

private:
    QVector<PrivacySettingRow> m_rows;
};

} // namespace beygram::telegram
