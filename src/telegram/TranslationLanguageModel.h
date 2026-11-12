#pragma once

#include "LanguagePackModel.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// Holds the languages the translation settings offer, the one the user picked
/// and whether incoming messages are translated automatically.
///
/// The rows are the same LanguageRow values LanguagePackModel stores, so the
/// picker can share one parsed shape with the pack list instead of introducing a
/// second near-identical row type. This model is a plain value holder: it stores
/// already-parsed values, never talks to TDLib, performs no I/O and does not log.
///
/// Invariants:
///
///  - A language is identified by its code, so a row without a code is not a
///    language and is dropped on assignment. Codes are trimmed and lowercased,
///    matching how TranslationController stores its target language, so " EN "
///    and "en" name the same language: the first row wins and the incoming order
///    is otherwise preserved. The '-' separator of regional codes such as
///    "pt-br" is part of the code and is kept.
///  - The remaining LanguageRow fields (name, nativeName, beta,
///    percentTranslated) are stored verbatim; this model only cares about the
///    code and the label it shows.
///  - The selection is a code, not an index. select() accepts only a code that
///    is currently stored, compared case-insensitively because stored codes are
///    lowercased; an unknown code is ignored so the caller cannot select a
///    language that is not in the list. An empty (or blank) code clears the
///    selection.
///  - The selection survives a new row list, so a refresh that brings the
///    language back keeps the user's choice. If the selected code is no longer
///    stored, selectedCode() still reports it while selectedName() reports an
///    empty name because no stored row carries that code.
///  - autoTranslate() defaults to false and is independent of the selection: it
///    only records the user's preference, the caller decides what to do with it.
///  - clear() drops the rows, the selection and the auto-translate flag.
///  - changed() is emitted only when a value really differs. Assigning the same
///    rows, reselecting the stored code, setting the flag to its current value
///    or clearing an already empty model is silent, so a refresh does not
///    repaint the screen.
class TranslationLanguageModel final : public QObject {
    Q_OBJECT

public:
    explicit TranslationLanguageModel(QObject* parent = nullptr);

    /// Replaces the stored rows, dropping rows without a code, trimming and
    /// lowercasing the codes and dropping later rows repeating a stored code.
    /// Emits changed() only when the resulting list differs from the stored one.
    /// The selection is left untouched.
    void setLanguages(QVector<LanguageRow> languages);

    /// Returns the stored rows in order.
    [[nodiscard]] QVector<LanguageRow> languages() const;

    /// Returns the number of stored rows.
    [[nodiscard]] int count() const;

    /// Returns the rows whose code or name contains `query`, compared
    /// case-insensitively, in stored order. An empty query matches every row.
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

    /// True when incoming messages should be translated without asking.
    [[nodiscard]] bool autoTranslate() const;

    /// Sets the auto-translate preference, emitting changed() only when the
    /// value actually differs.
    void setAutoTranslate(bool on);

    /// Drops the stored rows, the selection and the auto-translate flag,
    /// emitting changed() only when there was something to drop.
    void clear();

signals:
    /// Emitted once per real change to the rows, the selection or the flag.
    void changed();

private:
    QVector<LanguageRow> m_languages;
    QString m_selected;
    bool m_auto = false;
};

} // namespace beygram::telegram
