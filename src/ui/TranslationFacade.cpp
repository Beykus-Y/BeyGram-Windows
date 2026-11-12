#include "TranslationFacade.h"

#include "telegram/LanguagePackModel.h"
#include "telegram/TranslationController.h"
#include "telegram/TranslationLanguageModel.h"

#include <QVector>

namespace beygram::ui {

TranslationFacade::TranslationFacade(QObject* parent)
    : QObject(parent)
    , m_controller(new telegram::TranslationController(this))
    , m_languages(new telegram::TranslationLanguageModel(this))
{
    // The model is the single source of truth for the offered languages, the
    // selection and the auto-translate flag, so its change signal is the facade
    // change signal for those values.
    connect(m_languages, &telegram::TranslationLanguageModel::changed,
            this, &TranslationFacade::changed);
}

QString TranslationFacade::targetLanguage() const
{
    return m_controller->targetLanguage();
}

QStringList TranslationFacade::languageNames() const
{
    const QVector<telegram::LanguageRow> rows = m_languages->languages();

    QStringList names;
    names.reserve(rows.size());
    for (const telegram::LanguageRow& row : rows)
        names.append(row.name);
    return names;
}

void TranslationFacade::setLanguages(const QStringList& codes, const QStringList& names)
{
    QVector<telegram::LanguageRow> rows;
    rows.reserve(codes.size());
    for (qsizetype i = 0; i < codes.size(); ++i) {
        telegram::LanguageRow row;
        row.code = codes.at(i);

        // A label is only worth showing when it is not blank; otherwise the code
        // is the one thing the user can still recognise the row by.
        const QString label = i < names.size() ? names.at(i).trimmed() : QString();
        row.name = label.isEmpty() ? codes.at(i) : label;
        row.nativeName = row.name;
        row.percentTranslated = 100;
        rows.append(row);
    }

    m_languages->setLanguages(rows);
}

int TranslationFacade::languageIndex() const
{
    const QString code = m_controller->targetLanguage();
    if (code.isEmpty())
        return -1;

    const QVector<telegram::LanguageRow> rows = m_languages->languages();
    for (qsizetype i = 0; i < rows.size(); ++i) {
        if (rows.at(i).code == code)
            return static_cast<int>(i);
    }
    return -1;
}

void TranslationFacade::selectLanguage(int index)
{
    const QVector<telegram::LanguageRow> rows = m_languages->languages();
    if (index < 0 || index >= static_cast<int>(rows.size()))
        return;

    const QString code = rows.at(index).code;
    if (code == m_controller->targetLanguage() && code == m_languages->selectedCode())
        return;

    // A reply for the previous language would be rejected by the controller, so
    // the loading state of the messages still waiting must not survive the
    // switch.
    abandonPending();

    // Set the target first so an observer reacting to the model's changed()
    // already sees the matching target language.
    m_controller->setTargetLanguage(code);
    m_languages->select(code);
}

bool TranslationFacade::autoTranslate() const
{
    return m_languages->autoTranslate();
}

void TranslationFacade::setAutoTranslate(bool on)
{
    m_languages->setAutoTranslate(on);
}

bool TranslationFacade::loading() const
{
    return !m_pending.isEmpty();
}

void TranslationFacade::requestTranslation(const QString& messageId)
{
    const qint64 id = parseId(messageId);
    if (id <= 0)
        return;

    const QString language = m_controller->targetLanguage();
    if (language.isEmpty())
        return;

    m_controller->request(id);
    if (m_controller->isLoading(id) && !m_pending.contains(id)) {
        m_pending.insert(id);
        emit changed();
    }

    emit translateRequested(QString::number(id), language);
}

void TranslationFacade::setTranslation(const QString& messageId, const QString& language,
                                       const QString& text)
{
    const qint64 id = parseId(messageId);
    if (id <= 0)
        return;

    const QString before = m_controller->translationFor(id);
    const bool wasLoading = m_controller->isLoading(id);

    telegram::TranslatedMessage message;
    message.messageId = id;
    message.language = language;
    message.text = text;
    m_controller->setTranslation(message);

    // The controller drops a result for the wrong language, so its loading flag
    // is the authority on whether this message still waits.
    const bool isLoading = m_controller->isLoading(id);
    if (!isLoading)
        m_pending.remove(id);

    if (before != m_controller->translationFor(id) || wasLoading != isLoading)
        emit changed();
}

QString TranslationFacade::translationFor(const QString& messageId) const
{
    const qint64 id = parseId(messageId);
    if (id <= 0)
        return {};

    return m_controller->translationFor(id);
}

void TranslationFacade::clear()
{
    const bool hadState = !m_pending.isEmpty() || m_controller->cachedCount() > 0;

    m_pending.clear();
    m_controller->clearAll();

    if (hadState)
        emit changed();
}

qint64 TranslationFacade::parseId(const QString& messageId)
{
    bool ok = false;
    const qint64 id = messageId.trimmed().toLongLong(&ok);
    return ok ? id : 0;
}

void TranslationFacade::abandonPending()
{
    for (const qint64 id : m_pending)
        m_controller->clearTranslation(id);
    m_pending.clear();
}

} // namespace beygram::ui
