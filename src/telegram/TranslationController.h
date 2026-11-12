#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// One translated message as delivered by the translation backend. The language
/// is the language the text was translated into, not the source language.
struct TranslatedMessage {
    qint64 messageId = 0;
    QString language;
    QString text;
};

/// Owns the target language and the per-message translations the chat view
/// shows, and tracks which messages are waiting for a translation.
///
/// The target language is stored trimmed and lowercased. An empty language means
/// "translation is off": request() then does nothing, because there is no
/// language a result could be accepted for. Changing the language does not
/// discard translations that are already cached; call clearAll() for that.
///
/// request() marks a message as loading so the view can show a placeholder, and
/// setTranslation() resolves it. A translation is only accepted when it carries
/// the current target language, so a late reply for a language the user has
/// since switched away from is dropped instead of being shown under the wrong
/// label. Non-positive ids are ignored everywhere, and changed() is emitted only
/// when the stored state actually changed.
class TranslationController final : public QObject {
    Q_OBJECT

public:
    explicit TranslationController(QObject* parent = nullptr);

    /// Sets the language translations are requested in. The value is trimmed and
    /// lowercased, and an empty value disables translation.
    void setTargetLanguage(const QString& language);

    /// The normalised target language, or an empty string when disabled.
    [[nodiscard]] QString targetLanguage() const;

    /// Marks the message as waiting for a translation. Ignored for a
    /// non-positive id, for a message that already has a translation, and while
    /// translation is disabled. Repeating the request is a no-op.
    void request(qint64 messageId);

    /// Stores a finished translation and clears the loading flag. Ignored when
    /// the id is not positive, the text is empty, or the language is empty or
    /// does not match the target language; an ignored result leaves any pending
    /// loading state untouched.
    void setTranslation(TranslatedMessage message);

    /// The cached translation for the message, or an empty string when there is
    /// none.
    [[nodiscard]] QString translationFor(qint64 messageId) const;

    /// True while the message is waiting for a translation.
    [[nodiscard]] bool isLoading(qint64 messageId) const;

    /// Drops the cached translation and any loading state for the message.
    void clearTranslation(qint64 messageId);

    /// Drops every cached translation and every loading state.
    void clearAll();

    /// Number of messages with a cached translation.
    [[nodiscard]] int cachedCount() const;

signals:
    /// Emitted whenever a translation, a loading flag or the target language
    /// actually changed.
    void changed();

private:
    QString m_language;
    QHash<qint64, QString> m_texts;
    QSet<qint64> m_loading;
};

} // namespace beygram::telegram
