#pragma once

#include "core/Result.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace beygram::telegram {
class TelegramSession;
struct TelegramConfig;
} // namespace beygram::telegram

namespace beygram::ui {

/// UI-facing application state. It owns the Telegram session and exposes only
/// honest, derived facts: whether tdjson exists, whether credentials are set,
/// and what TDLib last reported. No QML object ever sends TDLib requests.
class AppController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString buildInfo READ buildInfo CONSTANT)
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)
    Q_PROPERTY(QString logDirectory READ logDirectory CONSTANT)

    Q_PROPERTY(bool tdlibAvailable READ tdlibAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString tdlibDetail READ tdlibDetail NOTIFY stateChanged)
    Q_PROPERTY(bool credentialsConfigured READ credentialsConfigured NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY stateChanged)

    Q_PROPERTY(QString stageName READ stageName NOTIFY stateChanged)
    Q_PROPERTY(QString stageTitle READ stageTitle NOTIFY stateChanged)
    Q_PROPERTY(QString stageDetail READ stageDetail NOTIFY stateChanged)
    Q_PROPERTY(QString connectionTitle READ connectionTitle NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString credentialsHint READ credentialsHint CONSTANT)
    Q_PROPERTY(QString bgMediaTitle READ bgMediaTitle CONSTANT)
    Q_PROPERTY(QString bgMediaDetail READ bgMediaDetail CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    [[nodiscard]] QString appVersion() const;
    [[nodiscard]] QString qtVersion() const;
    [[nodiscard]] QString buildInfo() const;
    [[nodiscard]] QString dataDirectory() const;
    [[nodiscard]] QString logDirectory() const;

    [[nodiscard]] bool tdlibAvailable() const;
    [[nodiscard]] QString tdlibDetail() const;
    [[nodiscard]] bool credentialsConfigured() const;
    [[nodiscard]] bool running() const;
    [[nodiscard]] bool ready() const;
    [[nodiscard]] bool canStart() const;

    [[nodiscard]] QString stageName() const;
    [[nodiscard]] QString stageTitle() const;
    [[nodiscard]] QString stageDetail() const;
    [[nodiscard]] QString connectionTitle() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QString credentialsHint() const;
    [[nodiscard]] QString bgMediaTitle() const;
    [[nodiscard]] QString bgMediaDetail() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startTelegram();
    Q_INVOKABLE void stopTelegram();
    Q_INVOKABLE void openDataFolder();
    Q_INVOKABLE void openLogFolder();
    Q_INVOKABLE QStringList recentLogs() const;
    Q_INVOKABLE QString diagnosticsText() const;
    Q_INVOKABLE void copyDiagnostics();

    /// Stops the TDLib receiver before Qt tears down the QML engine.
    void shutdown();

signals:
    void stateChanged();

private:
    void buildConfig();
    void connectSession();
    void notifyChanged();

    std::unique_ptr<telegram::TelegramSession> m_session;
    QString m_tdlibDetail;
    bool m_tdlibAvailable = false;
    bool m_credentialsConfigured = false;
};

} // namespace beygram::ui
