#include "TranslationController.h"

namespace beygram::telegram {

namespace {

QString normalizedLanguage(const QString& language)
{
    return language.trimmed().toLower();
}

} // namespace

TranslationController::TranslationController(QObject* parent)
    : QObject(parent)
{
}

void TranslationController::setTargetLanguage(const QString& language)
{
    const QString normalized = normalizedLanguage(language);
    if (m_language == normalized)
        return;

    m_language = normalized;
    emit changed();
}

QString TranslationController::targetLanguage() const
{
    return m_language;
}

void TranslationController::request(qint64 messageId)
{
    if (messageId <= 0)
        return;

    // Without a target language there is nothing to request and no result could
    // ever be accepted, so the message must not be left loading forever.
    if (m_language.isEmpty())
        return;

    if (m_texts.contains(messageId) || m_loading.contains(messageId))
        return;

    m_loading.insert(messageId);
    emit changed();
}

void TranslationController::setTranslation(TranslatedMessage message)
{
    if (message.messageId <= 0)
        return;

    if (message.text.trimmed().isEmpty())
        return;

    const QString language = normalizedLanguage(message.language);
    if (language.isEmpty() || language != m_language)
        return;

    const bool wasLoading = m_loading.remove(message.messageId) > 0;
    const bool textChanged = m_texts.value(message.messageId) != message.text;
    if (textChanged)
        m_texts.insert(message.messageId, message.text);

    if (wasLoading || textChanged)
        emit changed();
}

QString TranslationController::translationFor(qint64 messageId) const
{
    return m_texts.value(messageId);
}

bool TranslationController::isLoading(qint64 messageId) const
{
    return m_loading.contains(messageId);
}

void TranslationController::clearTranslation(qint64 messageId)
{
    if (messageId <= 0)
        return;

    const bool removedText = m_texts.remove(messageId) > 0;
    const bool removedLoading = m_loading.remove(messageId) > 0;
    if (removedText || removedLoading)
        emit changed();
}

void TranslationController::clearAll()
{
    if (m_texts.isEmpty() && m_loading.isEmpty())
        return;

    m_texts.clear();
    m_loading.clear();
    emit changed();
}

int TranslationController::cachedCount() const
{
    return static_cast<int>(m_texts.size());
}

} // namespace beygram::telegram
