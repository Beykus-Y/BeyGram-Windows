#include "SpeechRecognitionController.h"

namespace beygram::telegram {

SpeechRecognitionController::SpeechRecognitionController(QObject* parent)
    : QObject(parent)
{
}

void SpeechRecognitionController::start(qint64 messageId)
{
    // A non-positive id cannot identify a message, so it must not replace an
    // attempt that is already in flight.
    if (messageId <= 0)
        return;

    // A repeated start for the same message with nothing heard yet leaves the
    // observable state unchanged, even though it asks the owner to begin again.
    const bool alreadyListening = m_state == State::Listening
        && m_messageId == messageId && m_partial.isEmpty()
        && m_result.isEmpty() && m_error.isEmpty();

    m_messageId = messageId;
    m_partial.clear();
    m_result.clear();
    m_error.clear();
    m_state = State::Requesting;

    emit recognitionRequested(messageId);

    // The owner may fail() or cancel() from the signal, or start another
    // attempt. Only a request that is still pending for this message becomes a
    // listening attempt; a nested call has already announced its own outcome.
    if (m_state != State::Requesting || m_messageId != messageId)
        return;

    m_state = State::Listening;
    if (!alreadyListening)
        emit changed();
}

void SpeechRecognitionController::setPartialText(const QString& text)
{
    // Partials only describe speech that is currently being captured.
    if (m_state != State::Listening)
        return;

    if (m_partial == text)
        return;

    // Engines revise partials word by word, so the text is kept verbatim
    // rather than trimmed.
    m_partial = text;
    emit changed();
}

void SpeechRecognitionController::finish(const QString& text)
{
    if (m_state != State::Listening && m_state != State::Processing)
        return;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        // Completing with no text would leave a "successful" result that shows
        // nothing, so it is a failure with a stable, translatable message.
        m_state = State::Failed;
        m_error = QStringLiteral("empty result");
        m_partial.clear();
        m_result.clear();
        emit changed();
        return;
    }

    m_result = trimmed;
    m_partial.clear();
    m_state = m_state == State::Listening ? State::Processing : State::Done;
    emit changed();
}

void SpeechRecognitionController::fail(const QString& error)
{
    // Done and Failed are final, and Idle has no attempt to fail.
    if (!active())
        return;

    m_state = State::Failed;
    m_error = error;
    // A failure has no result to show, and the engine is no longer listening.
    m_partial.clear();
    m_result.clear();
    emit changed();
}

void SpeechRecognitionController::cancel()
{
    if (m_state == State::Idle && m_messageId == 0 && m_partial.isEmpty()
        && m_result.isEmpty() && m_error.isEmpty()) {
        return;
    }

    m_state = State::Idle;
    m_messageId = 0;
    m_partial.clear();
    m_result.clear();
    m_error.clear();
    emit changed();
}

QString SpeechRecognitionController::stateName() const
{
    switch (m_state) {
    case State::Idle:
        return QStringLiteral("idle");
    case State::Requesting:
        return QStringLiteral("requesting");
    case State::Listening:
        return QStringLiteral("listening");
    case State::Processing:
        return QStringLiteral("processing");
    case State::Done:
        return QStringLiteral("done");
    case State::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("idle");
}

bool SpeechRecognitionController::active() const
{
    return m_state == State::Requesting || m_state == State::Listening
        || m_state == State::Processing;
}

} // namespace beygram::telegram
