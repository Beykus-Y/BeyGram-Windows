#include "TranslationLanguageModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

// Codes are identifiers and are stored trimmed and lowercased, matching
// TranslationController's target language, so two spellings of one code must
// compare equal here or the list would carry the same language twice. A blank
// code normalises to an empty string and is therefore not a language.
// QString::toLower() leaves the '-' of regional codes such as "pt-br" alone.
QString normalizedCode(const QString& code)
{
    return code.trimmed().toLower();
}

bool storesCode(const QVector<LanguageRow>& rows, const QString& code)
{
    return std::any_of(rows.cbegin(), rows.cend(),
                       [&code](const LanguageRow& row) {
                           return row.code == code;
                       });
}

// LanguageRow carries no operator==, so the change check compares the fields
// explicitly. Keeps setLanguages() from notifying on a no-op assignment.
bool sameLanguages(const QVector<LanguageRow>& lhs,
                   const QVector<LanguageRow>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                      [](const LanguageRow& left, const LanguageRow& right) {
                          return left.code == right.code
                              && left.name == right.name
                              && left.nativeName == right.nativeName
                              && left.beta == right.beta
                              && left.percentTranslated
                                  == right.percentTranslated;
                      });
}

} // namespace

TranslationLanguageModel::TranslationLanguageModel(QObject* parent)
    : QObject(parent)
{
}

void TranslationLanguageModel::setLanguages(QVector<LanguageRow> languages)
{
    QVector<LanguageRow> next;
    next.reserve(languages.size());

    for (LanguageRow& row : languages) {
        row.code = normalizedCode(row.code);

        // A row without a code cannot be selected or persisted, so it is not a
        // language.
        if (row.code.isEmpty())
            continue;

        // The code identifies the language; the first row and its labels win
        // over later repeats.
        if (storesCode(next, row.code))
            continue;

        next.append(std::move(row));
    }

    if (sameLanguages(next, m_languages))
        return;

    m_languages = std::move(next);
    emit changed();
}

QVector<LanguageRow> TranslationLanguageModel::languages() const
{
    return m_languages;
}

int TranslationLanguageModel::count() const
{
    return static_cast<int>(m_languages.size());
}

QVector<LanguageRow> TranslationLanguageModel::filtered(const QString& query) const
{
    QVector<LanguageRow> matches;
    matches.reserve(m_languages.size());

    // An empty query is contained in every field, so it keeps the whole list.
    for (const LanguageRow& row : m_languages) {
        if (!row.code.contains(query, Qt::CaseInsensitive)
            && !row.name.contains(query, Qt::CaseInsensitive)) {
            continue;
        }

        matches.append(row);
    }

    return matches;
}

void TranslationLanguageModel::select(const QString& code)
{
    const QString wanted = normalizedCode(code);

    if (wanted.isEmpty()) {
        if (m_selected.isEmpty())
            return;

        m_selected.clear();
        emit changed();
        return;
    }

    const auto it = std::find_if(m_languages.cbegin(), m_languages.cend(),
                                 [&wanted](const LanguageRow& row) {
                                     return row.code == wanted;
                                 });

    // Selecting a language that is not in the list would leave the screen with
    // a choice it cannot show, so the request is ignored.
    if (it == m_languages.cend())
        return;

    if (m_selected == it->code)
        return;

    m_selected = it->code;
    emit changed();
}

QString TranslationLanguageModel::selectedCode() const
{
    return m_selected;
}

QString TranslationLanguageModel::selectedName() const
{
    if (m_selected.isEmpty())
        return {};

    const auto it = std::find_if(m_languages.cbegin(), m_languages.cend(),
                                 [this](const LanguageRow& row) {
                                     return row.code == m_selected;
                                 });

    if (it == m_languages.cend())
        return {};

    return it->name;
}

bool TranslationLanguageModel::autoTranslate() const
{
    return m_auto;
}

void TranslationLanguageModel::setAutoTranslate(bool on)
{
    if (m_auto == on)
        return;

    m_auto = on;
    emit changed();
}

void TranslationLanguageModel::clear()
{
    if (m_languages.isEmpty() && m_selected.isEmpty() && !m_auto)
        return;

    m_languages.clear();
    m_selected.clear();
    m_auto = false;
    emit changed();
}

} // namespace beygram::telegram
