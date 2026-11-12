#include "LanguagePackModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int MinPercentTranslated = 0;
constexpr int MaxPercentTranslated = 100;

// Codes are identifiers and are stored lowercased, so two spellings of one code
// must compare equal here or the list would carry the same language twice.
// QString::toLower() leaves the '-' of regional codes such as "pt-br" alone.
QString normalizedCode(const QString& code)
{
    return code.toLower();
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

LanguagePackModel::LanguagePackModel(QObject* parent)
    : QObject(parent)
{
}

void LanguagePackModel::setLanguages(QVector<LanguageRow> languages)
{
    QVector<LanguageRow> next;
    next.reserve(languages.size());

    for (LanguageRow& row : languages) {
        // A row without a code cannot be selected or persisted, so it is not a
        // language.
        if (row.code.isEmpty())
            continue;

        row.code = normalizedCode(row.code);

        // The code identifies the language; the first row and its labels win
        // over later repeats.
        if (storesCode(next, row.code))
            continue;

        row.percentTranslated = qBound(MinPercentTranslated,
                                       row.percentTranslated,
                                       MaxPercentTranslated);

        next.append(std::move(row));
    }

    if (sameLanguages(next, m_languages))
        return;

    m_languages = std::move(next);
    emit changed();
}

QVector<LanguageRow> LanguagePackModel::languages() const
{
    return m_languages;
}

int LanguagePackModel::count() const
{
    return static_cast<int>(m_languages.size());
}

QVector<LanguageRow> LanguagePackModel::filtered(const QString& query) const
{
    QVector<LanguageRow> matches;
    matches.reserve(m_languages.size());

    // An empty query is contained in every field, so it keeps the whole list.
    for (const LanguageRow& row : m_languages) {
        if (!row.code.contains(query, Qt::CaseInsensitive)
            && !row.name.contains(query, Qt::CaseInsensitive)
            && !row.nativeName.contains(query, Qt::CaseInsensitive)) {
            continue;
        }

        matches.append(row);
    }

    return matches;
}

void LanguagePackModel::select(const QString& code)
{
    if (code.isEmpty()) {
        if (m_selected.isEmpty())
            return;

        m_selected.clear();
        emit changed();
        return;
    }

    const QString wanted = normalizedCode(code);
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

QString LanguagePackModel::selectedCode() const
{
    return m_selected;
}

QString LanguagePackModel::selectedName() const
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

void LanguagePackModel::clear()
{
    if (m_languages.isEmpty() && m_selected.isEmpty())
        return;

    m_languages.clear();
    m_selected.clear();
    emit changed();
}

} // namespace beygram::telegram
