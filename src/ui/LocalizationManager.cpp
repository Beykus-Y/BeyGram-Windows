#include "LocalizationManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSaveFile>

#include <utility>

namespace beygram::ui {
namespace {

constexpr int kLocaleConfigVersion = 1;

QString normalizeLanguage(const QString& raw)
{
    const QString lowered = raw.trimmed().toLower();
    if (lowered == QLatin1String("ru") || lowered.startsWith(QLatin1String("ru_"))
        || lowered.startsWith(QLatin1String("ru-"))) {
        return QStringLiteral("ru");
    }
    if (lowered == QLatin1String("en") || lowered.startsWith(QLatin1String("en_"))
        || lowered.startsWith(QLatin1String("en-"))) {
        return QStringLiteral("en");
    }
    return QStringLiteral("auto");
}

QLocale localeForEffective(const QString& effective)
{
    if (effective == QLatin1String("ru"))
        return QLocale(QLocale::Russian, QLocale::Russia);
    return QLocale(QLocale::English, QLocale::UnitedStates);
}

} // namespace

LocalizationManager::LocalizationManager(QString configFilePath, QObject* parent)
    : QObject(parent), m_configFilePath(std::move(configFilePath))
{
    m_effectiveLanguage = resolveEffective(m_language);
}

LocalizationManager::~LocalizationManager()
{
    if (m_translatorInstalled && QCoreApplication::instance() != nullptr)
        QCoreApplication::removeTranslator(&m_translator);
}

void LocalizationManager::attachEngine(QQmlEngine* engine)
{
    m_engine = engine;
    apply(false);
}

void LocalizationManager::load()
{
    QString next = QStringLiteral("auto");
    if (!m_configFilePath.isEmpty()) {
        QFile file(m_configFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject())
                next = normalizeLanguage(doc.object().value(QStringLiteral("language")).toString());
        }
    }
    m_language = next;
    apply(false);
}

void LocalizationManager::setLanguage(const QString& language)
{
    const QString next = normalizeLanguage(language);
    if (next == m_language)
        return;
    m_language = next;
    apply(true);
}

QString LocalizationManager::resolveEffective(const QString& language) const
{
    if (language == QLatin1String("ru") || language == QLatin1String("en"))
        return language;
    const QLocale sys = QLocale::system();
    if (sys.language() == QLocale::Russian)
        return QStringLiteral("ru");
    return QStringLiteral("en");
}

void LocalizationManager::apply(bool persistChoice)
{
    const QString nextEffective = resolveEffective(m_language);
    m_effectiveLanguage = nextEffective;

    const QLocale locale = localeForEffective(nextEffective);
    QLocale::setDefault(locale);

    if (QCoreApplication::instance() != nullptr) {
        if (m_translatorInstalled) {
            QCoreApplication::removeTranslator(&m_translator);
            m_translatorInstalled = false;
        }
        const QString qmPath = QStringLiteral(":/i18n/beygram_%1.qm").arg(nextEffective);
        if (m_translator.load(qmPath)) {
            QCoreApplication::installTranslator(&m_translator);
            m_translatorInstalled = true;
        }
    }

    if (persistChoice)
        persist();

    ++m_revision;
    if (m_engine)
        m_engine->retranslate();
    emit languageChanged();
}

void LocalizationManager::persist() const
{
    if (m_configFilePath.isEmpty())
        return;
    const QDir parent = QFileInfo(m_configFilePath).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return;

    const QJsonObject root{
        {QStringLiteral("version"), kLocaleConfigVersion},
        {QStringLiteral("language"), m_language},
    };
    QSaveFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return;
    }
    file.commit();
}

QString LocalizationManager::formatChatTimestamp(qint64 unixSeconds) const
{
    if (unixSeconds <= 0)
        return QString();
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(unixSeconds).toLocalTime();
    if (!dt.isValid())
        return QString();

    const QDate today = QDate::currentDate();
    const QDate date = dt.date();
    const QLocale locale = localeForEffective(m_effectiveLanguage);
    if (date == today)
        return locale.toString(dt.time(), QStringLiteral("HH:mm"));
    const qint64 daysAgo = date.daysTo(today);
    if (daysAgo > 0 && daysAgo < 7)
        return locale.toString(date, QStringLiteral("ddd"));
    if (date.year() == today.year())
        return locale.toString(date, QStringLiteral("d MMM"));
    return locale.toString(date, QStringLiteral("dd.MM.yy"));
}

QString LocalizationManager::formatDayDivider(qint64 unixSeconds) const
{
    if (unixSeconds <= 0)
        return QString();
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(unixSeconds).toLocalTime();
    if (!dt.isValid())
        return QString();

    const QDate today = QDate::currentDate();
    const QDate date = dt.date();
    if (date == today)
        return tr("Today");
    if (date == today.addDays(-1))
        return tr("Yesterday");

    const QLocale locale = localeForEffective(m_effectiveLanguage);
    if (date.year() == today.year())
        return locale.toString(date, QStringLiteral("d MMMM"));
    return locale.toString(date, QStringLiteral("d MMMM yyyy"));
}

QString LocalizationManager::formatDateTime(qint64 unixSeconds) const
{
    if (unixSeconds <= 0)
        return QString();
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(unixSeconds).toLocalTime();
    if (!dt.isValid())
        return QString();
    const QLocale locale = localeForEffective(m_effectiveLanguage);
    return locale.toString(dt, QStringLiteral("dd.MM.yyyy HH:mm"));
}

} // namespace beygram::ui
