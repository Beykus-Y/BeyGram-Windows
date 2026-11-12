#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One row of an inline query answer, as handed to the UI.
///
/// `id` is the TDLib result id and is the only field the controller treats as
/// an identity: an empty id makes a row unusable, and a repeated id is dropped
/// in favour of the first occurrence. `title` and `description` are display
/// text and are carried verbatim, because a bot may legitimately answer with an
/// empty description. `messageId` is the id of the message the result would
/// send, or 0 when the row does not describe an existing message.
///
/// The struct owns only value types, so it can be copied into a model and out
/// of it without lifetime rules.
struct InlineResultRow {
    QString id;
    QString title;
    QString description;
    qint64 messageId = 0;
};

/// Holds the client-side state of one inline query session with a bot.
///
/// The session is the triple (bot, query, answer): a bot must be open, a query
/// must be non-empty, and only then can an answer be stored. Every mutator is a
/// no-op when it would not change the stored state, so changed() is emitted once
/// per real transition and a view never reloads for nothing.
///
/// Opening a second bot is a fresh session: the query, the answer, the loading
/// flag and the selection of the previous bot are dropped, because an inline
/// answer belongs to the bot that produced it. Opening the bot that is already
/// open keeps the state, so a repeated open() from a re-entered screen does not
/// throw away a result list that is still valid.
///
/// The selection invariant is kept here rather than in the view: the selected
/// index is either -1 or a valid index into results(), and a new answer that no
/// longer contains the selected index clears the selection instead of pointing
/// past the end.
class InlineQueryController final : public QObject {
    Q_OBJECT

public:
    explicit InlineQueryController(QObject* parent = nullptr);

    /// Starts a session with `botUserId`. A non-positive id is rejected, since
    /// TDLib user ids are positive, so a default-constructed or bogus id never
    /// opens a session. Re-opening the same bot changes nothing.
    void open(qint64 botUserId);

    /// Ends the session and resets every field. Closing a closed controller is a
    /// no-op.
    void close();

    /// Whether a bot session is open, i.e. botUserId() is positive.
    [[nodiscard]] bool active() const { return m_botUserId > 0; }

    /// The bot of the open session, or 0 when no session is open.
    [[nodiscard]] qint64 botUserId() const { return m_botUserId; }

    /// Stores the trimmed query. A change of query starts a new answer, so the
    /// previous results, the selection and the loading flag are dropped; an
    /// empty query is a valid "nothing typed yet" state and leaves the
    /// controller with no results and no selection. Re-setting the same trimmed
    /// text is silent.
    void setQuery(const QString& query);
    [[nodiscard]] QString query() const { return m_query; }

    /// Replaces the answer of the current query.
    ///
    /// Ignored unless a bot is open and the query is non-empty, because an
    /// answer can only belong to a real query. Rows with an empty id are
    /// dropped and repeated ids keep their first position, so the TDLib order
    /// survives normalisation. A selection that the new answer cannot satisfy
    /// is cleared.
    void setResults(QVector<InlineResultRow> results);
    [[nodiscard]] QVector<InlineResultRow> results() const { return m_results; }

    /// Sets the "answer pending" flag. It carries no results of its own, so it
    /// may be set before the query produces anything; setQuery() and clear()
    /// reset it.
    void setLoading(bool loading);
    [[nodiscard]] bool loading() const { return m_loading; }

    /// Selects a row by index. Only -1 and an index inside results() are
    /// accepted; -1 clears the selection and any other out-of-range value is
    /// ignored, so the stored index can never point past the end.
    void setSelectedIndex(int index);
    [[nodiscard]] int selectedIndex() const { return m_selected; }

    /// The selected row, or a default-constructed row (empty id, empty texts,
    /// messageId 0) when nothing is selected.
    [[nodiscard]] InlineResultRow selected() const;

    /// Resets the whole session: bot, query, answer, loading flag and selection.
    /// Already-empty state is silent.
    void clear();

signals:
    /// Emitted whenever any part of the session actually changed.
    void changed();

private:
    qint64 m_botUserId = 0;
    QString m_query;
    QVector<InlineResultRow> m_results;
    bool m_loading = false;
    int m_selected = -1;
};

} // namespace beygram::telegram
