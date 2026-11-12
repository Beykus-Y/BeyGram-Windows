#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace beygram::telegram {
class TranslationController;
class TranslationLanguageModel;
}

namespace beygram::ui {

/// QML-facing view of the translation settings and the per-message translation
/// cache.
///
/// The facade composes two existing value holders instead of duplicating their
/// rules: a TranslationLanguageModel owns the offered languages, the selected
/// language and the auto-translate preference, and a TranslationController owns
/// the normalised target language, the cached texts and the loading flags. This
/// class only translates between those values and the shape QML needs: a list of
/// labels, an index, a boolean "anything loading" flag and string message ids.
///
/// Message ids arrive from QML as strings because QML numbers are doubles and a
/// 64-bit TDLib id would lose precision. Every id is parsed to qint64; anything
/// that is not a positive integer is ignored.
///
/// The target language is the normalised code ("en", "pt-br") or an empty string
/// while translation is off. Translation stays off until a language is selected;
/// selecting a language turns it on. Codes are trimmed and lowercased by the
/// underlying model and controller.
///
/// requestTranslation() emits translateRequested() only when a target language
/// is set; without one there is no language a result could be accepted for, so
/// nothing is requested and nothing starts loading.
///
/// Switching to another language abandons the messages that are still waiting:
/// their loading flag is dropped because a reply for the old language would be
/// rejected by the controller anyway, and a stale spinner must not stay on
/// screen. Cached texts are left alone.
///
/// clear() drops the cached translations and every loading flag, but keeps the
/// language list, the selection and the auto-translate flag: those are settings,
/// not per-message state.
class TranslationFacade final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString targetLanguage READ targetLanguage NOTIFY changed)
    Q_PROPERTY(QStringList languageNames READ languageNames NOTIFY changed)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY changed)
    Q_PROPERTY(bool autoTranslate READ autoTranslate WRITE setAutoTranslate NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)

public:
    explicit TranslationFacade(QObject* parent = nullptr);

    /// The normalised target language code, or an empty string when translation
    /// is off.
    [[nodiscard]] QString targetLanguage() const;

    /// The labels of the offered languages, in stored order.
    [[nodiscard]] QStringList languageNames() const;

    /// Replaces the offered languages. `codes` and `names` are paired by
    /// position; a missing or blank name falls back to the code so the row stays
    /// identifiable, and rows without a code are dropped by the model. The
    /// selection is kept.
    Q_INVOKABLE void setLanguages(const QStringList& codes, const QStringList& names);

    /// The index of the target language in languageNames(), or -1 when
    /// translation is off or the target language is no longer offered.
    [[nodiscard]] int languageIndex() const;

    /// Selects the language at `index` and turns translation on. An index that
    /// does not name a stored language is ignored.
    Q_INVOKABLE void selectLanguage(int index);

    /// True when incoming messages should be translated without asking.
    [[nodiscard]] bool autoTranslate() const;

    /// Stores the auto-translate preference.
    void setAutoTranslate(bool on);

    /// True while at least one message is waiting for a translation.
    [[nodiscard]] bool loading() const;

    /// Marks the message as waiting and asks the caller to translate it. Ignored
    /// for a non-positive id and while no target language is selected.
    Q_INVOKABLE void requestTranslation(const QString& messageId);

    /// Stores a finished translation. A result for a language that is not the
    /// current target language is ignored by the controller, leaving any pending
    /// loading state untouched.
    Q_INVOKABLE void setTranslation(const QString& messageId, const QString& language,
                                    const QString& text);

    /// The cached translation for the message, or an empty string when there is
    /// none.
    Q_INVOKABLE QString translationFor(const QString& messageId) const;

    /// Drops every cached translation and every loading flag, keeping the
    /// language settings.
    Q_INVOKABLE void clear();

signals:
    /// Emitted once per real change to the target language, the language list,
    /// the selection, the auto-translate flag, a stored translation or a loading
    /// flag.
    void changed();

    /// Emitted when the caller should translate `messageId` into `language`.
    void translateRequested(const QString& messageId, const QString& language);

private:
    /// Parses a QML message id, returning 0 when the text is not a number. The
    /// result can still be zero or negative; callers reject anything that is not
    /// positive.
    static qint64 parseId(const QString& messageId);

    /// Drops the loading flags of the messages still waiting, without touching
    /// cached texts.
    void abandonPending();

    telegram::TranslationController* m_controller = nullptr;
    telegram::TranslationLanguageModel* m_languages = nullptr;
    QSet<qint64> m_pending;
};

} // namespace beygram::ui
