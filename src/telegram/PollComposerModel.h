#pragma once

#include "PollSend.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {

/// The poll being composed in the chat composer, before it becomes an
/// OutgoingPoll.
///
/// Every text field is normalised on the way in: surrounding whitespace is
/// trimmed and the result is capped (question 255, option 100, explanation 200
/// UTF-16 units) without ever splitting a surrogate pair. A blank option is
/// never stored, so options() only ever holds non-empty text, and a duplicate is
/// rejected when it repeats an existing option case-sensitively after trimming.
///
/// The model is deliberately permissive about state that is not currently in
/// use: a quiz answer and an explanation are kept while the poll is regular, so
/// switching quiz off and back on does not lose them. valid() is the single gate
/// that decides whether the composition may be sent.
///
/// changed() is emitted only when a call actually modifies the model, so a view
/// can bind to it without handling redundant updates. Question, option and
/// explanation text is user content and is never logged.
class PollComposerModel final : public QObject {
    Q_OBJECT

public:
    explicit PollComposerModel(QObject* parent = nullptr);

    /// Replaces the question. Emits changed() only when the normalised text
    /// differs from the current question.
    void setQuestion(const QString& question);
    [[nodiscard]] QString question() const;

    /// Appends option after the existing ones. Returns false, storing nothing,
    /// when the trimmed option is empty, when it already exists (compared
    /// case-sensitively) or when ten options are already stored.
    bool addOption(const QString& option);

    /// Removes the option at index. Returns false for an out-of-range index.
    /// When the removal leaves correctOption past the end of the list it is
    /// reset to 0, so it always indexes options() or is 0 for an empty list.
    bool removeOption(int index);

    /// Replaces the option at index. Returns false for an out-of-range index, a
    /// blank option, or a value already used at another position. Storing the
    /// text that is already there is a successful no-op.
    bool setOption(int index, const QString& option);

    /// The stored options, in composer order. correctOption is an index into
    /// this list.
    [[nodiscard]] QStringList options() const;

    /// Telegram polls are anonymous by default. A quiz is forced anonymous when
    /// it is sent (see sendPollRequest); the choice is still kept here.
    void setAnonymous(bool anonymous);
    [[nodiscard]] bool anonymous() const;

    /// Whether a regular poll accepts several answers. Telegram ignores this for
    /// quizzes; the flag is kept regardless.
    void setMultiple(bool multiple);
    [[nodiscard]] bool multiple() const;

    /// Switching quiz off keeps the correct option and the explanation, so they
    /// are still there if the quiz is switched back on.
    void setQuiz(bool quiz);
    [[nodiscard]] bool quiz() const;

    /// Sets the 0-based correct answer. An out-of-range index is ignored, so the
    /// stored value always indexes options() while options are present.
    void setCorrectOption(int index);
    [[nodiscard]] int correctOption() const;

    void setExplanation(const QString& explanation);
    [[nodiscard]] QString explanation() const;

    /// True when the composition can be sent: a non-empty question, two to ten
    /// non-empty options and, for a quiz, a correct option inside options().
    [[nodiscard]] bool valid() const;

    /// The poll to hand to sendPollRequest. When !valid() the question and the
    /// options are left empty (the flags and the explanation are still
    /// reported), so the request builder rejects it rather than sending a
    /// half-filled poll. correctOption is -1 for a regular poll, where it has no
    /// meaning.
    [[nodiscard]] OutgoingPoll build() const;

    /// Returns every field to its default: no question, no options, anonymous,
    /// single answer, regular, correct option 0, no explanation. Emits
    /// changed() only when something was actually reset.
    void reset();

signals:
    void changed();

private:
    QString m_question;
    QStringList m_options;
    bool m_anonymous = true;
    bool m_multiple = false;
    bool m_quiz = false;
    int m_correct = 0;
    QString m_explanation;
};

} // namespace beygram::telegram
