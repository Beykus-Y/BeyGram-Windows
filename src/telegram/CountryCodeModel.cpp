#include "CountryCodeModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {

namespace {

QString normalizedIso(const QString& iso)
{
    return iso.trimmed().toUpper();
}

QString digitsOnly(const QString& value)
{
    QString digits;
    digits.reserve(value.size());

    for (const QChar character : value) {
        if (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
            digits.append(character);
    }

    return digits;
}

bool sameRow(const CountryRow& left, const CountryRow& right)
{
    return left.iso == right.iso && left.name == right.name
        && left.dialCode == right.dialCode && left.flag == right.flag;
}

bool sameRows(const QVector<CountryRow>& left, const QVector<CountryRow>& right)
{
    return std::equal(left.cbegin(), left.cend(), right.cbegin(), right.cend(), sameRow);
}

} // namespace

CountryCodeModel::CountryCodeModel(QObject* parent)
    : QObject(parent)
{
}

void CountryCodeModel::setCountries(QVector<CountryRow> countries)
{
    QVector<CountryRow> normalized;
    normalized.reserve(countries.size());

    for (CountryRow& row : countries) {
        row.iso = normalizedIso(row.iso);
        if (row.iso.isEmpty())
            continue;

        row.dialCode = digitsOnly(row.dialCode);

        const bool duplicate = std::any_of(
            normalized.cbegin(), normalized.cend(), [&row](const CountryRow& existing) {
                return existing.iso == row.iso;
            });
        if (!duplicate)
            normalized.append(std::move(row));
    }

    if (sameRows(normalized, m_countries))
        return;

    m_countries = std::move(normalized);
    emit changed();
}

int CountryCodeModel::count() const
{
    return m_countries.size();
}

QVector<CountryRow> CountryCodeModel::countries() const
{
    return m_countries;
}

QVector<CountryRow> CountryCodeModel::filtered(const QString& query) const
{
    const QString needle = query.trimmed();
    if (needle.isEmpty())
        return m_countries;

    QVector<CountryRow> matches;
    matches.reserve(m_countries.size());

    for (const CountryRow& row : m_countries) {
        if (row.name.contains(needle, Qt::CaseInsensitive)
            || row.iso.contains(needle, Qt::CaseInsensitive)
            || row.dialCode.contains(needle, Qt::CaseInsensitive)) {
            matches.append(row);
        }
    }

    return matches;
}

CountryRow CountryCodeModel::byIso(const QString& iso) const
{
    const QString wanted = normalizedIso(iso);
    if (wanted.isEmpty())
        return {};

    const auto match = std::find_if(
        m_countries.cbegin(), m_countries.cend(),
        [&wanted](const CountryRow& row) { return row.iso == wanted; });
    if (match == m_countries.cend())
        return {};

    return *match;
}

CountryRow CountryCodeModel::byDialCode(const QString& dialCode) const
{
    const QString wanted = digitsOnly(dialCode);
    if (wanted.isEmpty())
        return {};

    const auto match = std::find_if(
        m_countries.cbegin(), m_countries.cend(),
        [&wanted](const CountryRow& row) { return row.dialCode == wanted; });
    if (match == m_countries.cend())
        return {};

    return *match;
}

void CountryCodeModel::select(const QString& iso)
{
    const QString wanted = normalizedIso(iso);

    if (wanted.isEmpty()) {
        if (m_selected.isEmpty())
            return;

        m_selected.clear();
        emit changed();
        return;
    }

    const bool known = std::any_of(
        m_countries.cbegin(), m_countries.cend(),
        [&wanted](const CountryRow& row) { return row.iso == wanted; });
    if (!known || wanted == m_selected)
        return;

    m_selected = wanted;
    emit changed();
}

QString CountryCodeModel::selectedIso() const
{
    return m_selected;
}

void CountryCodeModel::clear()
{
    if (m_countries.isEmpty() && m_selected.isEmpty())
        return;

    m_countries.clear();
    m_selected.clear();
    emit changed();
}

} // namespace beygram::telegram
