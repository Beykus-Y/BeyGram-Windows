#include "core/AppPaths.h"
#include "core/Logging.h"
#include "platform/windows/WindowChrome.h"
#include "ui/AppController.h"
#include "ui/AppearanceStore.h"
#include "ui/LocalizationManager.h"
#include "ui/MediaImageProvider.h"
#include "ui/QrCodeImageProvider.h"
#include "ui/ThemeController.h"
#include "ui/WindowStateStore.h"

#include <QApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QWindow>
#include <QtQml>

namespace {

void handleObjectCreationFailure()
{
    qCritical("The BeyGram QML root object could not be created");
    QCoreApplication::exit(1);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Beykus"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("beygram.app"));
    QCoreApplication::setApplicationName(QStringLiteral("BeyGram Windows"));
    QCoreApplication::setApplicationVersion(QStringLiteral(BEYGRAM_VERSION_STRING));

    QString directoryError;
    if (!beygram::AppPaths::ensureDirectory(beygram::AppPaths::dataDirectory(), &directoryError)
        || !beygram::AppPaths::ensureDirectory(beygram::AppPaths::logDirectory(), &directoryError)) {
        // Logging still falls back to stderr, so the app remains usable.
        qWarning("%s", qUtf8Printable(directoryError));
    }

    const bool verbose = !qEnvironmentVariableIsEmpty("BEYGRAM_VERBOSE_LOGS");
    beygram::logging::initialize(beygram::AppPaths::logDirectory(), verbose);

    qInfo("BeyGram Windows %s starting (Qt %s)",
          BEYGRAM_VERSION_STRING, qVersion());
    qInfo("Data directory: %s", qUtf8Printable(beygram::AppPaths::dataDirectory()));

    // The foundation UI is intentionally independent of a platform widget style.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    beygram::ui::ThemeController theme;
    beygram::ui::AppearanceStore appearance(
        QDir(beygram::AppPaths::dataDirectory()).filePath(QStringLiteral("appearance.json")));
    appearance.load();
    beygram::ui::LocalizationManager lang(
        QDir(beygram::AppPaths::configDirectory()).filePath(QStringLiteral("locale.json")));
    lang.load();
    beygram::ui::WindowStateStore windowState(
        QDir(beygram::AppPaths::configDirectory())
            .filePath(QStringLiteral("window/window-state.json")));
    windowState.load();
    theme.setPresetName(appearance.settings().themePreset);
    theme.setModeName(appearance.settings().themeMode);
    QObject::connect(&theme, &beygram::ui::ThemeController::themeChanged, &appearance,
                     [&theme, &appearance] {
                         appearance.setThemePreset(theme.presetName());
                         appearance.setThemeMode(theme.modeName());
                     });
    QObject::connect(&appearance, &beygram::ui::AppearanceStore::changed, &theme,
                     [&theme, &appearance] {
                         theme.setPresetName(appearance.themePreset());
                         theme.setModeName(appearance.themeMode());
                     });

    beygram::ui::AppController controller;

    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramTheme", &theme);
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramAppearance", &appearance);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramApp", &controller);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramLang", &lang);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramMediaViewer",
                                 controller.mediaViewer());
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramPlayback",
                                 controller.playback());

    QQmlApplicationEngine engine;
    lang.attachEngine(&engine);

    auto* qrProvider = new beygram::ui::QrCodeImageProvider();
    engine.addImageProvider(QStringLiteral("beygramqr"), qrProvider);
    QObject::connect(&controller, &beygram::ui::AppController::qrCodeChanged, qrProvider,
                     [&controller, qrProvider] {
                         qrProvider->setLink(controller.qrLoginLink());
                     });

    engine.addImageProvider(
        QStringLiteral("beygrammedia"),
        new beygram::ui::MediaImageProvider(controller.mediaStore()));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, handleObjectCreationFailure, Qt::QueuedConnection);

    engine.loadFromModule("BeyGram", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCritical("No QML root object was created; exiting");
        controller.shutdown();
        beygram::logging::shutdown();
        return 1;
    }

    if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst())) {
        const beygram::ui::WindowState saved = windowState.state();
        if (saved.maximized) {
            window->showMaximized();
        } else {
            window->setGeometry(saved.x, saved.y, saved.width, saved.height);
        }
        auto syncChrome = [window, &theme] {
            const QColor caption =
                (theme.modeName() == QLatin1String("Amoled")) ? theme.bg() : theme.surface();
            beygram::platform::applyWindowTitleBarTheme(window, theme.darkMode(), caption,
                                                       theme.text());
        };
        syncChrome();
        QObject::connect(&theme, &beygram::ui::ThemeController::themeChanged, window, syncChrome);
        QObject::connect(&application, &QCoreApplication::aboutToQuit,
                         [window, &windowState] {
                             windowState.save({window->x(), window->y(),
                                               window->width(), window->height(),
                                               window->visibility() == QWindow::Maximized});
                         });
    }

    const int exitCode = application.exec();

    controller.shutdown();
    beygram::logging::shutdown();
    return exitCode;
}
