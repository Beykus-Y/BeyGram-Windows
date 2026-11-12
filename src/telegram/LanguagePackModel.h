#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One language pack the language picker can offer.
///
/// code is the stable identifier TDLib uses for the pack (for example "en" or
/// "pt-br"), name is the English label, nativeName is the label the speakers
/// of that language expect and percentTranslated is how complete the pack is.
/// beta marks packs that are still being translated. All fields are already
/// parsed values; nothing here talks to TDLib.
struct LanguageRow {
    QString code;
    QString name;
    QString nativeName;
    bool beta = false;
    int percentTranslated = 0;
};

/// Holds the list of language packs the settings screen can offer and the one
/// the user picked.
///
/// The model is a plain value holder: it stores already-parsed LanguageRow
/// values, never talks to TDLib, performs no I/O and does not log. The caller
/// decides what the codes mean and what to do with the selected one.
///
/// Invariants:
///
///  - A language is identified by its code, so a row without a code is not a
///    language and is dropped on assignment. Codes are compared after
///    lowercasing, so "EN" and "en" name the same language: the first row wins
///    and the incoming order is otherwise preserved. The '-' separator of
///    regional codes such as "pt-br" is part of the code and is kept.
///  - percentTranslated is a percentage, so it is clamped to 0..100 instead of
///    storing a value a progress bar cannot show.
///  - The selection is a code, not an index. select() accepts only a code that
///    is currently stored, compared case-insensitively because stored codes are
///    lowercased; an unknown code is ignored so the caller cannot select a
///    language that is not in the list. An empty code clears the selection.
///  - The selection survives a new row list, so a refresh that brings the
///    language back keeps the user's choice. If the selected code is no longer
///    stored, selectedCode() still reports it while selectedName() reports an
///    empty name because no stored row carries that code.
///  - changed() is emitted only when a value really differs. Assigning the same
///    rows, reselecting the stored code or clearing an already empty model is
///    silent, so a refresh does not repaint the screen.
class LanguagePackModel final : public QObject {
    Q_OBJECT

public:
    explicit LanguagePackModel(QObject* parent = nullptr);

    /// Replaces the stored rows, dropping rows without a code, lowercasing the
    /// codes, dropping later rows repeating a stored code and clamping
    /// percentTranslated to 0..100. Emits changed() only when the resulting
    /// list differs from the stored one. The selection is left untouched.
    void setLanguages(QVector<LanguageRow> languages);

    /// Returns the stored rows in order.
    [[nodiscard]] QVector<LanguageRow> languages() const;

    /// Returns the number of stored rows.
    [[nodiscard]] int count() const;

    /// Returns the rows whose code, name or nativeName contains `query`,
    /// compared case-insensitively, in stored order. An empty query matches
    /// every row.
    [[nodiscard]] QVector<LanguageRow> filtered(const QString& query) const;

    /// Selects the language with `code`, compared case-insensitively. An empty
    /// code clears the selection and a code that is not stored is ignored.
    /// Emits changed() only when the selection actually differs.
    void select(const QString& code);

    /// Returns the selected code as stored (lowercased), or an empty string
    /// when nothing is selected. The code may name a row that is no longer
    /// stored.
    [[nodiscard]] QString selectedCode() const;

    /// Returns the name of the selected row, or an empty string when nothing is
    /// selected or the selected code is not stored.
    [[nodiscard]] QString selectedName() const;

    /// Drops the stored rows and the selection, emitting changed() only when
    /// there was something to drop.
    void clear();

signals:
    /// Emitted once per real change to the rows or the selection.
    void changed();

private:
    QVector<LanguageRow> m_languages;
    QString m_selected;
};

} // namespace beygram::telegram
