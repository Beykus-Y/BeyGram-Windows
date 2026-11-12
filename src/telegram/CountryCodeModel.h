#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One entry of the country / calling-code picker.
struct CountryRow {
    /// ISO 3166-1 alpha-2 code, stored upper-case.
    QString iso;
    /// Display name of the country or region.
    QString name;
    /// International calling code without punctuation, digits only.
    QString dialCode;
    /// Emoji flag or any other short glyph the UI shows next to the name.
    QString flag;
};

/// Holds the country list of the phone-number picker and the current choice.
///
/// The model is a plain value holder: it never loads the list itself and never
/// touches TDLib or the UI. setCountries() normalises what it is given so the
/// rest of the code can compare fields directly: rows without an ISO code are
/// dropped, ISO codes are trimmed and upper-cased, calling codes keep their
/// digits only, and rows sharing an ISO code are collapsed to the first one so
/// the incoming order survives.
///
/// The selection is a single ISO code. select() only accepts codes that are
/// currently in the list and an empty code clears the choice; setCountries()
/// leaves the stored code alone even when the new list no longer contains it,
/// because the caller decides when a picker session resets.
class CountryCodeModel final : public QObject {
    Q_OBJECT

public:
    explicit CountryCodeModel(QObject* parent = nullptr);

    /// Replaces the stored rows with the normalised form of `countries`.
    /// Emits changed() only when the resulting rows differ from the current
    /// ones. The selection is kept as it is.
    void setCountries(QVector<CountryRow> countries);

    [[nodiscard]] int count() const;

    /// Returns a copy of the stored rows, in list order.
    [[nodiscard]] QVector<CountryRow> countries() const;

    /// Returns the rows whose name, ISO code or calling code contains `query`,
    /// compared case-insensitively and keeping the stored order. The query is
    /// trimmed; an empty query matches every row.
    [[nodiscard]] QVector<CountryRow> filtered(const QString& query) const;

    /// Returns the row with ISO code `iso`, compared after trimming and
    /// case-insensitively, or a default-constructed row when there is none.
    [[nodiscard]] CountryRow byIso(const QString& iso) const;

    /// Returns the first row whose calling code equals the digits of
    /// `dialCode`, so "+49" and "49" both find Germany, or a
    /// default-constructed row when there is none.
    [[nodiscard]] CountryRow byDialCode(const QString& dialCode) const;

    /// Selects `iso` (trimmed, case-insensitive). Unknown codes are ignored and
    /// an empty code clears the selection. Emits changed() when the stored code
    /// actually changes.
    void select(const QString& iso);

    /// Returns the selected ISO code, or an empty string when nothing is
    /// selected.
    [[nodiscard]] QString selectedIso() const;

    /// Drops every row and the selection. Emits changed() when there was
    /// anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<CountryRow> m_countries;
    QString m_selected;
};

} // namespace beygram::telegram
