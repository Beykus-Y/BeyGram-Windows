#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The option selection of one poll message, held while the user edits it and
/// before it is sent.
///
/// Option indices are the 0-based positions in PollInfo::options, which is the
/// same numbering PollResultOption::index carries and the one votePollRequest()
/// sends as setPollAnswer's option_ids. The model therefore reuses those payload
/// structs instead of introducing a second option type: the caller parses a poll
/// with pollFromMessage(), calls
/// setPoll(chatId, messageId, poll.options.size(), poll.multipleChoice), and
/// later passes selected() straight to
/// votePollRequest(chatId(), messageId(), selected()).
///
/// This model is a draft, not a result. It never reads PollOption::chosen or
/// PollResultOption::chosen and never talks to TDLib: the server's counts and
/// the account's current vote stay in PollResults, while this model only answers
/// "which options would the next vote name". Because nothing here performs I/O,
/// logs or reads the clock, the tests stay deterministic.
///
/// Invariants:
///
///  - A poll is active only after setPoll() accepted a positive chat id, a
///    positive message id and a positive option count. Until then active() is
///    false, every index is out of range and toggle() ignores them all.
///  - setPoll() rejects an unparsed poll (non-positive id or count) instead of
///    storing it, so a malformed answer never replaces the poll the user is
///    looking at.
///  - selected() is always sorted ascending and holds no duplicates. The order
///    is a storage detail; option ids are a set, and a sorted list keeps
///    comparisons and request bodies stable.
///  - A single-choice poll (multiple() == false) holds at most one index: a new
///    index replaces the previous one.
///  - changed() is emitted only when the stored state really differs. Re-setting
///    the same poll, toggling an out-of-range index, clearing an empty selection
///    and resetting an already reset model are all silent, so a repaint is never
///    requested for nothing.
class PollVoteModel final : public QObject {
    Q_OBJECT

public:
    explicit PollVoteModel(QObject* parent = nullptr);

    /// Points the model at one poll and clears the selection.
    ///
    /// optionCount is the number of answers, i.e. PollInfo::options.size(), and
    /// multiple mirrors PollInfo::multipleChoice. A non-positive chat id, message
    /// id or option count is rejected: the model keeps whatever it held and no
    /// signal is emitted.
    ///
    /// Re-setting the poll that is already stored with the same ids, count and
    /// mode still drops any selection, because a fresh answer from TDLib may
    /// carry a different vote. changed() is emitted only if something actually
    /// differs from the previous state.
    void setPoll(qint64 chatId, qint64 messageId, int optionCount, bool multiple);

    /// True while a valid poll is stored, so a selection can be built. Always
    /// false before the first accepted setPoll() and after reset().
    [[nodiscard]] bool active() const;

    /// Chat of the stored poll, 0 when no poll is active.
    [[nodiscard]] qint64 chatId() const;

    /// Message of the stored poll, 0 when no poll is active.
    [[nodiscard]] qint64 messageId() const;

    /// Number of answers of the stored poll, 0 when no poll is active.
    [[nodiscard]] int optionCount() const;

    /// Whether the stored poll allows several answers at once.
    [[nodiscard]] bool multiple() const;

    /// Toggles one answer by its 0-based position in PollInfo::options.
    ///
    /// An index outside `[0, optionCount())` is ignored, including every index
    /// while no poll is active, and returns false without emitting changed().
    ///
    /// Otherwise the selection changes and the call returns true: an index that
    /// is already selected is removed, so tapping a chosen answer again clears
    /// it. A new index is added in a multiple-answer poll and replaces the whole
    /// selection in a single-choice poll.
    bool toggle(int optionIndex);

    /// A copy of the selected indices, sorted ascending. This is the option_ids
    /// argument votePollRequest() expects.
    [[nodiscard]] QVector<int> selected() const;

    /// True when at least one answer is selected.
    [[nodiscard]] bool hasSelection() const;

    /// Drops the selection but keeps the poll, e.g. when the user cancels an
    /// edit. No-op, with no signal, when nothing is selected.
    void clearSelection();

    /// Forgets the poll and the selection, e.g. when the message is closed, the
    /// chat changes or the account signs out. changed() is emitted only if there
    /// was something to forget.
    void reset();

signals:
    /// The stored poll or the selection changed. Not emitted for a no-op call.
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_messageId = 0;
    int m_count = 0;
    bool m_multiple = false;
    QVector<int> m_selected;
};

} // namespace beygram::telegram
