#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One literal find-and-replace pair for outgoing message text.
struct ReplacementRule {
    /// Needle to look for. Matched case-sensitively as an exact substring; a
    /// rule with an empty needle is ignored by TextReplacementModel.
    QString from;
    /// Text inserted in place of every occurrence of `from`. May be empty,
    /// which deletes the needle.
    QString to;
};

/// Ordered set of literal text replacements applied to composer text before a
/// message is sent, e.g. "brb" -> "be right back".
///
/// Rules are normalised on the way in: rules with an empty `from` are dropped
/// and a later rule that repeats an earlier `from` is discarded, so the first
/// rule for a needle is the one that survives. The surviving rules keep the
/// order the caller supplied, which is also the order rules() reports.
///
/// apply() rewrites the text in a single left-to-right pass, trying the longest
/// needle first at every position. Substituted text is therefore never scanned
/// again, so a rule can never cascade into another rule's output, and a short
/// needle can never shadow a longer one that starts at the same offset.
/// Matching is case-sensitive and literal: no regular expressions, no word
/// boundaries.
class TextReplacementModel final : public QObject {
    Q_OBJECT

public:
    explicit TextReplacementModel(QObject* parent = nullptr);

    /// Replaces the rule set. Emits changed() only when the normalised rules
    /// actually differ from the current ones.
    void setRules(QVector<ReplacementRule> rules);

    /// The effective rules, in the order they were supplied.
    [[nodiscard]] QVector<ReplacementRule> rules() const;

    /// Number of effective rules; ignored and duplicate rules are not counted.
    [[nodiscard]] int count() const;

    /// Applies every rule once, longest needle first. Empty text is returned
    /// unchanged, as is any text that contains no needle.
    [[nodiscard]] QString apply(const QString& text) const;

    /// Drops every rule. Emits changed() only when a rule was actually removed.
    void clear();

signals:
    void changed();

private:
    QVector<ReplacementRule> m_rules;
};

} // namespace beygram::telegram
