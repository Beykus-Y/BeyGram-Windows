#include "TextReplacementModel.h"

#include <algorithm>
#include <QStringView>
#include <utility>

namespace beygram::telegram {
namespace {

/// Longest needle first, so a short rule can never shadow a longer one that
/// begins at the same offset. Stable, so equal lengths keep the caller's order
/// and the output stays deterministic.
bool longerFirst(const ReplacementRule& left, const ReplacementRule& right)
{
    return left.from.size() > right.from.size();
}

/// Drops rules with an empty needle and keeps only the first rule per needle.
QVector<ReplacementRule> normalizedRules(QVector<ReplacementRule> rules)
{
    QVector<ReplacementRule> kept;
    kept.reserve(rules.size());
    for (ReplacementRule& rule : rules) {
        if (rule.from.isEmpty())
            continue;

        const bool duplicate =
            std::any_of(kept.cbegin(), kept.cend(), [&rule](const ReplacementRule& existing) {
                return existing.from == rule.from;
            });
        if (!duplicate)
            kept.append(std::move(rule));
    }
    return kept;
}

/// ReplacementRule carries no equality operator, so the rule sets are compared
/// field by field; both sides are already normalised.
bool sameRules(const QVector<ReplacementRule>& left, const QVector<ReplacementRule>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype i = 0; i < left.size(); ++i) {
        if (left.at(i).from != right.at(i).from || left.at(i).to != right.at(i).to)
            return false;
    }
    return true;
}

} // namespace

TextReplacementModel::TextReplacementModel(QObject* parent)
    : QObject(parent)
{
}

void TextReplacementModel::setRules(QVector<ReplacementRule> rules)
{
    QVector<ReplacementRule> next = normalizedRules(std::move(rules));
    if (sameRules(next, m_rules))
        return;

    m_rules = std::move(next);
    emit changed();
}

QVector<ReplacementRule> TextReplacementModel::rules() const
{
    return m_rules;
}

int TextReplacementModel::count() const
{
    return static_cast<int>(m_rules.size());
}

QString TextReplacementModel::apply(const QString& text) const
{
    if (text.isEmpty() || m_rules.isEmpty())
        return text;

    QVector<ReplacementRule> ordered = m_rules;
    std::stable_sort(ordered.begin(), ordered.end(), longerFirst);

    const QStringView input(text);
    QString result;
    result.reserve(text.size());

    qsizetype index = 0;
    while (index < input.size()) {
        const ReplacementRule* match = nullptr;
        for (const ReplacementRule& rule : ordered) {
            // The needles are non-empty, so at(0) is always a real character.
            if (input.at(index) != rule.from.at(0))
                continue;
            if (rule.from.size() > input.size() - index)
                continue;
            if (input.mid(index, rule.from.size()) == QStringView(rule.from)) {
                match = &rule;
                break;
            }
        }

        if (match == nullptr) {
            result.append(input.at(index));
            ++index;
            continue;
        }

        // Consuming the whole needle is what makes the pass single: the text
        // just written is never scanned for further rules.
        result.append(match->to);
        index += match->from.size();
    }

    return result;
}

void TextReplacementModel::clear()
{
    if (m_rules.isEmpty())
        return;

    m_rules.clear();
    emit changed();
}

} // namespace beygram::telegram
