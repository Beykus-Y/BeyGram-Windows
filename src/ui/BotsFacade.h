#pragma once

#include "telegram/BotCommandModel.h"
#include "telegram/InlineQueryController.h"
#include "telegram/WebAppSessionController.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::ui {

/// The bot surfaces of a chat as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the
/// command menu lives in BotCommandModel, the inline query session in
/// InlineQueryController and the Web App session in
/// WebAppSessionController. Every property is read straight from those
/// models, and their changed() signals are forwarded as this object's
/// changed(), so a QML binding repaints on a real update and stays quiet on a
/// redundant one.
///
/// The facade only does what QML cannot: it accepts the string lists that
/// TDLib-shaped payloads and text fields arrive as, parses the decimal ids
/// QML holds as strings, and turns a tap on a row into the id the caller needs.
/// It never talks to TDLib, the network or storage, never logs, and never
/// launches anything itself; the request signals are the caller's cue to send
/// the matching TDLib request.
///
/// Command names are stored bare, exactly like BotCommands: a single leading
/// '/' is dropped, the name is trimmed, and an empty name is skipped. The
/// model de-duplicates case-insensitively and keeps the incoming order.
///
/// Ids are only accepted as plain positive decimal digits, because TDLib user
/// and chat ids are positive integers: an empty string, a sign, whitespace
/// inside the digits or a non-numeric character is rejected instead of being
/// guessed at, and a rejected id never disturbs the state already on screen.
///
/// Web App launches are gated on isSafeWebAppUrl(): only a well-formed https
/// URL with a host and no credentials is accepted, so a javascript:, tg: or
/// plain http button can never reach the embedded browser.
class BotsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList commands READ commands NOTIFY changed)
    Q_PROPERTY(bool inlineActive READ inlineActive NOTIFY changed)
    Q_PROPERTY(QString inlineQuery READ inlineQuery NOTIFY changed)
    Q_PROPERTY(QStringList inlineTitles READ inlineTitles NOTIFY changed)
    Q_PROPERTY(bool webAppActive READ webAppActive NOTIFY changed)
    Q_PROPERTY(QString webAppTitle READ webAppTitle NOTIFY changed)

public:
    explicit BotsFacade(QObject* parent = nullptr);

    /// The advertised command names, bare and in menu order.
    [[nodiscard]] QStringList commands() const;

    /// Replaces the command menu from the parallel lists a TDLib payload is
    /// read into. The lists are paired positionally and may differ in length:
    /// a name without a description gets an empty one, and an extra
    /// description is ignored. Names are normalized as described above and
    /// descriptions are trimmed; the model then drops repeated names
    /// case-insensitively, keeping the first entry.
    ///
    /// An empty `names` list clears the menu, which is a real change and is
    /// reported through changed().
    Q_INVOKABLE void setCommands(const QStringList& names,
                                 const QStringList& descriptions);

    /// The command names starting with `prefix`, in menu order. The prefix is
    /// trimmed and may carry a leading '/', so "/st" and "st" select the same
    /// commands; matching is case-insensitive and an empty prefix matches
    /// everything. Filtering never reorders or mutates the stored menu.
    Q_INVOKABLE QStringList filterCommands(const QString& prefix) const;

    /// Whether an inline query session with a bot is open.
    [[nodiscard]] bool inlineActive() const;

    /// The current inline query, trimmed; empty before anything is typed.
    [[nodiscard]] QString inlineQuery() const;

    /// The titles of the current inline answer, in the order the bot sent
    /// them. An empty title is kept as an empty string, because a bot may
    /// legitimately answer with one.
    [[nodiscard]] QStringList inlineTitles() const;

    /// Opens an inline session with `botUserId`. A non-positive or
    /// non-numeric id is ignored, leaving any current session untouched.
    /// Opening the bot that is already open keeps its query and answer, so a
    /// re-entered screen does not throw away a still valid result list;
    /// opening a different bot starts over with an empty query and answer.
    Q_INVOKABLE void openInline(const QString& botUserId);

    /// Stores the trimmed query. Ignored while no inline session is open,
    /// because a query belongs to a bot. A real change starts a new answer:
    /// the previous results and the selection are dropped, changed() is
    /// emitted, and then inlineQueryRequested() carries the new query.
    ///
    /// The empty query is a real change too: it clears the answer and emits
    /// inlineQueryRequested(""), so a caller can cancel an answer that is
    /// still in flight instead of showing it for the wrong query. Re-setting
    /// the same trimmed text is silent.
    Q_INVOKABLE void setInlineQuery(const QString& query);

    /// Replaces the answer of the current query from parallel string lists.
    /// Ids are opaque TDLib result ids and titles are display text; the lists
    /// are paired positionally and may differ in length, with a missing title
    /// read as empty. Rows are sent with no description and messageId 0.
    ///
    /// The controller owns the rules: an answer is ignored unless a bot is
    /// open and the query is non-empty, a row without an id is dropped, a
    /// repeated id keeps its first position, and an identical answer is
    /// silent.
    Q_INVOKABLE void setInlineResults(const QStringList& ids,
                                      const QStringList& titles);

    /// Chooses the inline result at `index`, emits inlineResultChosen() with
    /// that row's id, and leaves the row selected. Only an index inside the
    /// current answer is accepted; -1 and any out-of-range value are ignored
    /// and emit nothing, so a stale tap can never send a result that is no
    /// longer on screen.
    ///
    /// Choosing the already selected row emits the signal again, because the
    /// tap is a real user action even though the stored selection did not
    /// change.
    Q_INVOKABLE void selectInlineResult(int index);

    /// Ends the inline session and drops the bot, the query, the answer and
    /// the selection. Closing a closed session is a no-op.
    Q_INVOKABLE void closeInline();

    /// Whether a Web App session is open.
    [[nodiscard]] bool webAppActive() const;

    /// The title of the open Web App, trimmed by openWebApp(); empty while no
    /// session is open.
    [[nodiscard]] QString webAppTitle() const;

    /// Starts a Web App session for the app owned by `botUserId` in `chatId`,
    /// and then emits webAppOpenRequested() with the URL the caller should
    /// load.
    ///
    /// Both ids must be positive decimal integers and `url` must pass
    /// isSafeWebAppUrl(); otherwise nothing happens and nothing is emitted,
    /// so a malformed button cannot tear down the app already on screen.
    /// The URL is stored and reported trimmed.
    ///
    /// The request is emitted on every accepted call, even when the identical
    /// session is reopened and changed() stays quiet, because the caller
    /// asked for the app to be launched again.
    Q_INVOKABLE void openWebApp(const QString& chatId, const QString& botUserId,
                                const QString& title, const QString& url);

    /// Ends the Web App session and clears its title. Closing a closed
    /// session is a no-op.
    Q_INVOKABLE void closeWebApp();

signals:
    /// Emitted whenever any of the three bound models actually changed.
    void changed();

    /// Emitted by the keyboard when a command button is activated, with the
    /// bare command name ("start", not "/start"). The host decides whether to
    /// insert it into the composer or send it.
    void commandActivated(const QString& command);

    /// Emitted by setInlineQuery() with the stored query, including the empty
    /// query that cancels a pending answer.
    void inlineQueryRequested(const QString& query);

    /// Emitted by selectInlineResult() with the chosen row's TDLib result id.
    void inlineResultChosen(const QString& resultId);

    /// Emitted by openWebApp() with the validated https URL to load.
    void webAppOpenRequested(const QString& url);

private:
    telegram::BotCommandModel m_commands;
    telegram::InlineQueryController m_inline;
    telegram::WebAppSessionController m_webApp;
};

} // namespace beygram::ui
