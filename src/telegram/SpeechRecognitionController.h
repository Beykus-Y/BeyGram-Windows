#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks one speech-recognition attempt for one message, from the request the
/// owner has to start to the text or the error it produces.
///
///   Idle        no attempt; messageId() is 0
///   Requesting  start() is delivering recognitionRequested(); only observable
///               from that signal, where the owner may fail() or cancel()
///   Listening   the engine is capturing speech and may report partial text
///   Processing  listening ended and a provisional result is stored while the
///               engine delivers the final text
///   Done        the final text is stored
///   Failed      the attempt ended with an error, or with an empty result
///
/// start() is the only entry point. A non-positive message id is ignored, so a
/// malformed request cannot replace the attempt in flight. A valid id always
/// begins a new attempt and clears the previous partial text, result and error.
/// The request is emitted while the state is Requesting, which lets an owner
/// that cannot start the engine call fail() from the signal; once the request
/// has been delivered the state becomes Listening.
///
/// Partial text belongs to Listening only: finish(), fail() and cancel() all
/// clear it, because it describes speech that is no longer being captured.
/// finish() is a two-step completion: from Listening it stores the trimmed text
/// as the provisional result and moves to Processing; from Processing it stores
/// the trimmed final text and moves to Done. An empty text at either step fails
/// the attempt with "empty result" instead of completing it with nothing.
///
/// Done and Failed keep messageId() so a view can attach the outcome to the
/// message it belongs to; cancel() is the only way back to Idle. finish(),
/// fail() and cancel() are no-ops when they do not fit the current state, and
/// changed() is emitted only when the stored state really changed. The
/// controller performs no I/O, talks to no engine and logs nothing.
class SpeechRecognitionController final : public QObject {
    Q_OBJECT

public:
    enum class State { Idle, Requesting, Listening, Processing, Done, Failed };

    explicit SpeechRecognitionController(QObject* parent = nullptr);

    /// Starts a new attempt for `messageId` and emits recognitionRequested().
    /// A non-positive id is ignored and leaves the current attempt untouched.
    /// A valid id clears the partial text, result and error, then asks the
    /// owner to begin recognition; if the owner does not fail() or cancel()
    /// from that signal, the attempt starts Listening. changed() reports the
    /// resulting transition once.
    void start(qint64 messageId);

    /// Replaces the live partial text. Accepted only while Listening, because a
    /// partial describes speech that is still being captured. The text is
    /// stored verbatim, since engines revise it word by word, and an identical
    /// value emits nothing.
    void setPartialText(const QString& text);

    /// Completes the listening phase with `text`. The text is trimmed and
    /// stored as resultText(); an empty value fails the attempt with
    /// "empty result". From Listening this enters Processing, keeping the
    /// provisional text available while the engine finalises; from Processing
    /// it enters Done. Ignored in every other state, so a late result cannot
    /// revive an attempt that was cancelled or failed.
    void finish(const QString& text);

    /// Fails an active attempt (Requesting, Listening or Processing) with
    /// `error`, stored verbatim, and clears the partial text and result.
    /// Ignored once the attempt is Done, Failed or Idle.
    void fail(const QString& error);

    /// Drops the attempt and returns to Idle: id, partial text, result and
    /// error are all cleared. Silent when already idle.
    void cancel();

    [[nodiscard]] State state() const { return m_state; }

    /// The message being recognised, or 0 while Idle. Kept after Done and
    /// Failed so the UI can attach the outcome to the right message.
    [[nodiscard]] qint64 messageId() const { return m_messageId; }

    /// The latest partial from the engine, or an empty string outside
    /// Listening.
    [[nodiscard]] QString partialText() const { return m_partial; }

    /// The trimmed text accepted by finish(): the provisional text while
    /// Processing and the final text once Done. Empty in every other state.
    [[nodiscard]] QString resultText() const { return m_result; }

    /// The failure message, or an empty string unless Failed.
    [[nodiscard]] QString error() const { return m_error; }

    /// One of "idle", "requesting", "listening", "processing", "done" or
    /// "failed".
    [[nodiscard]] QString stateName() const;

    /// True while the attempt can still succeed: Requesting, Listening or
    /// Processing.
    [[nodiscard]] bool active() const;

signals:
    /// Emitted when an update changes what a view can show: the settled state,
    /// the id, the partial text, the result or the error. Rejected calls and
    /// no-op setters stay silent; the request itself is announced by
    /// recognitionRequested().
    void changed();

    /// Emitted by start() after the attempt has been reset and before it starts
    /// listening. The owner begins recognition for this message.
    void recognitionRequested(qint64 messageId);

private:
    State m_state = State::Idle;
    qint64 m_messageId = 0;
    QString m_partial;
    QString m_result;
    QString m_error;
};

} // namespace beygram::telegram
