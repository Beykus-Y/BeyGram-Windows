#include "core/AppPaths.h"
#include "core/Logging.h"
#include "ui/AppController.h"
#include "ui/QrCodeImageProvider.h"
#include "ui/ThemeController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
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
    QGuiApplication application(argc, argv);
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
    beygram::ui::AppController controller;

    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramTheme", &theme);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramApp", &controller);

    QQmlApplicationEngine engine;

    auto* qrProvider = new beygram::ui::QrCodeImageProvider();
    engine.addImageProvider(QStringLiteral("beygramqr"), qrProvider);
    QObject::connect(&controller, &beygram::ui::AppController::qrCodeChanged, qrProvider,
                     [&controller, qrProvider] {
                         qrProvider->setLink(controller.qrLoginLink());
                     });

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, handleObjectCreationFailure, Qt::QueuedConnection);

    engine.loadFromModule("BeyGram", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCritical("No QML root object was created; exiting");
        controller.shutdown();
        beygram::logging::shutdown();
        return 1;
    }

    const int exitCode = application.exec();

    controller.shutdown();
    beygram::logging::shutdown();
    return exitCode;
}
