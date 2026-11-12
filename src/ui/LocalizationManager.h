#pragma once

#include <QLocale>
#include <QObject>
#include <QString>
#include <QTranslator>

class QQmlEngine;

namespace beygram::ui {

/// Centralized Qt localization controller for QML (`BeyGramLang` singleton).
///
/// Supports `"auto"` (follows the Windows/Qt system locale on startup), `"ru"`
/// (Russian), and `"en"` (English). Persists the user's explicit choice in
/// `<configDirectory>/locale.json`, installs the matching QTranslator from the
/// compiled Qt translation resources (`:/i18n/beygram_<lang>.qm`), updates the
/// default QLocale, and calls `QQmlEngine::retranslate()` so every `qsTr()`
/// binding across the running UI updates live without recreating screens.
class LocalizationManager final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY languageChanged)
    Q_PROPERTY(int revision READ revision NOTIFY languageChanged)

public:
    explicit LocalizationManager(QString configFilePath, QObject* parent = nullptr);
    ~LocalizationManager() override;

    void attachEngine(QQmlEngine* engine);
    void load();

    [[nodiscard]] QString language() const { return m_language; }
    void setLanguage(const QString& language);

    [[nodiscard]] QString effectiveLanguage() const { return m_effectiveLanguage; }
    [[nodiscard]] int revision() const { return m_revision; }

    /// Formats a Unix timestamp (seconds) for the chat list row: HH:mm today,
    /// localized weekday within the last 6 days, or localized short date.
    Q_INVOKABLE QString formatChatTimestamp(qint64 unixSeconds) const;

    /// Formats a Unix timestamp (seconds) for a chat message day divider:
    /// Today / Yesterday / localized date (e.g., "12 ноября 2026" or "Nov 12, 2026").
    Q_INVOKABLE QString formatDayDivider(qint64 unixSeconds) const;

    /// Formats a Unix timestamp (seconds) as full date + time for search/details.
    Q_INVOKABLE QString formatDateTime(qint64 unixSeconds) const;

signals:
    void languageChanged();

private:
    void apply(bool persistChoice);
    [[nodiscard]] QString resolveEffective(const QString& language) const;
    void persist() const;

    QString m_configFilePath;
    QString m_language = QStringLiteral("auto");
    QString m_effectiveLanguage = QStringLiteral("en");
    int m_revision = 0;
    QQmlEngine* m_engine = nullptr;
    QTranslator m_translator;
    bool m_translatorInstalled = false;
};

} // namespace beygram::ui
