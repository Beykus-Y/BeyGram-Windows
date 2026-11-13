#include "core/ColorContrast.h"
#include "telegram/ChatFilterModel.h"
#include "telegram/ChatListModel.h"
#include "ui/AppController.h"
#include "ui/AppearanceStore.h"
#include "ui/LocalizationManager.h"
#include "ui/MediaImageProvider.h"
#include "ui/QrCodeImageProvider.h"
#include "ui/ThemeController.h"

#include <QCoreApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QStringList g_qmlWarnings;
QtMessageHandler g_previousHandler = nullptr;

void captureQmlWarnings(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if ((type == QtWarningMsg || type == QtCriticalMsg)
        && (msg.contains(QStringLiteral("QML"), Qt::CaseInsensitive)
            || msg.contains(QStringLiteral("qrc:/qt/qml/BeyGram"), Qt::CaseInsensitive)
            || msg.contains(QStringLiteral("TypeError"), Qt::CaseInsensitive)
            || msg.contains(QStringLiteral("ReferenceError"), Qt::CaseInsensitive)
            || msg.contains(QStringLiteral("Binding loop"), Qt::CaseInsensitive))) {
        g_qmlWarnings.append(msg);
    }
    if (g_previousHandler)
        g_previousHandler(type, context, msg);
}

} // namespace

class QmlUiTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void themePresetsMaintainContrast();
    void localizationSwitchesLanguagesAndPlurals();
    void mainAndCorePagesLoadWithoutQmlWarnings();
    void shellCompactNavigationAndThemeToggle();
    void chatFilterModelRowCountIsReactiveInQml();
    void responsiveLayoutTransitionsCleanly();

private:
    std::unique_ptr<QTemporaryDir> m_tempDir;
};

void QmlUiTest::initTestCase()
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    m_tempDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tempDir->isValid());
    qputenv("BEYGRAM_DATA_DIR", m_tempDir->path().toUtf8());
    qputenv("BEYGRAM_CONFIG_DIR", m_tempDir->path().toUtf8());
    qputenv("BEYGRAM_TDLIB_DLL", "");
    g_previousHandler = qInstallMessageHandler(captureQmlWarnings);
}

void QmlUiTest::cleanupTestCase()
{
    qInstallMessageHandler(g_previousHandler);
}

void QmlUiTest::themePresetsMaintainContrast()
{
    beygram::ui::ThemeController theme;
    for (const QString& preset : {QStringLiteral("Warm"), QStringLiteral("Graphite")}) {
        theme.setPresetName(preset);
        for (const QString& mode :
             {QStringLiteral("Light"), QStringLiteral("Dark"), QStringLiteral("Amoled")}) {
            theme.setModeName(mode);
            QVERIFY2(beygram::color::isReadableText(theme.text(), theme.surface()),
                     qPrintable(QStringLiteral("%1/%2 failed surface contrast").arg(preset, mode)));
            QVERIFY2(beygram::color::isReadableText(theme.text(), theme.surface2()),
                     qPrintable(QStringLiteral("%1/%2 failed surface2 contrast").arg(preset, mode)));
            QVERIFY2(beygram::color::isReadableText(theme.text(), theme.bubbleOut()),
                     qPrintable(QStringLiteral("%1/%2 failed bubbleOut contrast").arg(preset, mode)));
        }
    }
}

void QmlUiTest::localizationSwitchesLanguagesAndPlurals()
{
    QQmlEngine engine;
    beygram::ui::LocalizationManager lang(
        QDir(m_tempDir->path()).filePath(QStringLiteral("locale-test.json")));
    lang.attachEngine(&engine);

    lang.setLanguage(QStringLiteral("ru"));
    QCOMPARE(lang.effectiveLanguage(), QStringLiteral("ru"));
    QCOMPARE(lang.formatDayDivider(QDateTime::currentSecsSinceEpoch()), QStringLiteral("Сегодня"));

    QQmlComponent comp(&engine);
    comp.setData("import QtQuick\nQtObject {\n"
                 "  property int count: 1\n"
                 "  property string label: qsTr(\"%n chat(s)\", \"\", count)\n"
                 "}\n",
                 QUrl(QStringLiteral("qrc:/qt/qml/BeyGram/TestPlural.qml")));
    QScopedPointer<QObject> obj(comp.create());
    QVERIFY2(obj, qPrintable(comp.errorString()));
    QCOMPARE(obj->property("label").toString(), QStringLiteral("1 чат"));

    obj->setProperty("count", 3);
    QCOMPARE(obj->property("label").toString(), QStringLiteral("3 чата"));

    obj->setProperty("count", 5);
    QCOMPARE(obj->property("label").toString(), QStringLiteral("5 чатов"));

    lang.setLanguage(QStringLiteral("en"));
    QCOMPARE(lang.effectiveLanguage(), QStringLiteral("en"));
    QCOMPARE(lang.formatDayDivider(QDateTime::currentSecsSinceEpoch()), QStringLiteral("Today"));
    QCOMPARE(obj->property("label").toString(), QStringLiteral("5 chats"));
}

void QmlUiTest::mainAndCorePagesLoadWithoutQmlWarnings()
{
    g_qmlWarnings.clear();

    beygram::ui::ThemeController theme;
    beygram::ui::AppearanceStore appearance(
        QDir(m_tempDir->path()).filePath(QStringLiteral("appearance-ui-test.json")));
    appearance.load();
    beygram::ui::LocalizationManager lang(
        QDir(m_tempDir->path()).filePath(QStringLiteral("locale-ui-test.json")));
    lang.setLanguage(QStringLiteral("ru"));
    beygram::ui::AppController controller;

    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramTheme", &theme);
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramAppearance", &appearance);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramApp", &controller);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramLang", &lang);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramMediaViewer",
                                 controller.mediaViewer());
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramPlayback", controller.playback());

    QQmlApplicationEngine engine;
    lang.attachEngine(&engine);
    engine.addImageProvider(QStringLiteral("beygramqr"), new beygram::ui::QrCodeImageProvider());
    engine.addImageProvider(QStringLiteral("beygrammedia"),
                            new beygram::ui::MediaImageProvider(controller.mediaStore()));

    engine.loadFromModule("BeyGram", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);

    // Load Shell and key redesigned pages explicitly even when session is not authorized.
    for (const QString& typeName :
         {QStringLiteral("Shell"), QStringLiteral("ChatsPage"), QStringLiteral("OverviewPage"),
          QStringLiteral("SettingsPage"), QStringLiteral("GlobalSearchPage"),
          QStringLiteral("EnvironmentPage"), QStringLiteral("LogsPage"),
          QStringLiteral("DesignGalleryPage")}) {
        QQmlComponent comp(&engine, QStringLiteral("BeyGram"), typeName);
        QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
        QScopedPointer<QObject> instance(comp.create());
        QVERIFY2(instance != nullptr, qPrintable(comp.errorString()));
        if (auto* item = qobject_cast<QQuickItem*>(instance.get())) {
            item->setParentItem(window->contentItem());
            item->setWidth(1280);
            item->setHeight(820);
        }
        QCoreApplication::processEvents();
    }


    // Exercise live theme and language switches with loaded UI.
    theme.setModeName(QStringLiteral("Amoled"));
    theme.setPresetName(QStringLiteral("Graphite"));
    lang.setLanguage(QStringLiteral("en"));
    lang.setLanguage(QStringLiteral("ru"));
    QCoreApplication::processEvents();

    const QString shotDir = qEnvironmentVariable("BEYGRAM_UI_SCREENSHOT_DIR");
    if (!shotDir.isEmpty()) {
        QDir().mkpath(shotDir);

        auto renderObject = [&](QQuickWindow* targetWin, const QString& fileName) {
            targetWin->show();
            QTest::qWait(80);
            QCoreApplication::processEvents();
            const QImage img = targetWin->grabWindow();
            QVERIFY2(!img.isNull(), qPrintable(QStringLiteral("Failed to grab ") + fileName));
            QVERIFY(img.save(QDir(shotDir).filePath(fileName)));
        };

        auto renderPageType = [&](const QString& typeName, const QString& preset,
                                  const QString& mode, const QString& design,
                                  const QString& wallpaper, int w, int h,
                                  const QString& fileName, int shellSection = -1) {
            appearance.setThemePreset(preset);
            appearance.setThemeMode(mode);
            appearance.setDesignStyle(design);
            appearance.setWallpaperPreset(wallpaper);
            theme.setPresetName(preset);
            theme.setModeName(mode);
            lang.setLanguage(QStringLiteral("ru"));

            QQuickWindow win;
            win.setWidth(w);
            win.setHeight(h);
            win.setColor(theme.bg());

            QQmlComponent pageComp(&engine, QStringLiteral("BeyGram"), typeName);
            QVERIFY2(pageComp.status() == QQmlComponent::Ready, qPrintable(pageComp.errorString()));
            QScopedPointer<QObject> pageObj(pageComp.create());
            QVERIFY(pageObj != nullptr);
            auto* pageItem = qobject_cast<QQuickItem*>(pageObj.get());
            QVERIFY(pageItem != nullptr);
            pageItem->setParentItem(win.contentItem());
            pageItem->setWidth(w);
            pageItem->setHeight(h);
            if (shellSection >= 0)
                pageObj->setProperty("currentSection", shellSection);
            QCoreApplication::processEvents();
            renderObject(&win, fileName);
        };

        renderPageType(QStringLiteral("AuthPage"), QStringLiteral("Warm"), QStringLiteral("Light"),
                       QStringLiteral("Signature"), QStringLiteral("none"), 1180, 760,
                       QStringLiteral("01_auth_ru_warm_light.png"));
        renderPageType(QStringLiteral("Shell"), QStringLiteral("Warm"), QStringLiteral("Dark"),
                       QStringLiteral("LiquidGlass"), QStringLiteral("lavender-flow"), 1280, 820,
                       QStringLiteral("02_chats_ru_warm_dark_glass.png"), 0);
        renderPageType(QStringLiteral("Shell"), QStringLiteral("Graphite"), QStringLiteral("Amoled"),
                       QStringLiteral("Signature"), QStringLiteral("deep-night"), 1280, 820,
                       QStringLiteral("03_chats_ru_amoled_sig.png"), 0);
        renderPageType(QStringLiteral("Shell"), QStringLiteral("Warm"), QStringLiteral("Light"),
                       QStringLiteral("Default"), QStringLiteral("none"), 1280, 820,
                       QStringLiteral("04_profile_ru_warm_light.png"), 2);
        renderPageType(QStringLiteral("Shell"), QStringLiteral("Warm"), QStringLiteral("Dark"),
                       QStringLiteral("LiquidGlass"), QStringLiteral("mint-air"), 1280, 820,
                       QStringLiteral("05_settings_ru_dark.png"), 3);
        renderPageType(QStringLiteral("Shell"), QStringLiteral("Graphite"), QStringLiteral("Amoled"),
                       QStringLiteral("Default"), QStringLiteral("none"), 1280, 820,
                       QStringLiteral("06_search_ru_amoled.png"), 1);
    }

    QVERIFY2(g_qmlWarnings.isEmpty(), qPrintable(g_qmlWarnings.join(QStringLiteral("\n"))));
    controller.shutdown();
}

void QmlUiTest::shellCompactNavigationAndThemeToggle()
{
    beygram::ui::ThemeController theme;
    beygram::ui::AppearanceStore appearance(
        QDir(m_tempDir->path()).filePath(QStringLiteral("appearance-shell-test.json")));
    beygram::ui::LocalizationManager lang(
        QDir(m_tempDir->path()).filePath(QStringLiteral("locale-shell-test.json")));
    beygram::ui::AppController controller;

    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramTheme", &theme);
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramAppearance", &appearance);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramApp", &controller);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramLang", &lang);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramMediaViewer",
                                 controller.mediaViewer());
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramPlayback", controller.playback());

    QQmlApplicationEngine engine;
    lang.attachEngine(&engine);
    QQmlComponent comp(&engine, QStringLiteral("BeyGram"), QStringLiteral("Shell"));
    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    QScopedPointer<QObject> shell(comp.create());
    QVERIFY(shell != nullptr);

    auto* item = qobject_cast<QQuickItem*>(shell.get());
    QVERIFY(item != nullptr);
    item->setWidth(1280);
    item->setHeight(820);

    // Compact slim rail by default (68-76 px) and Chats selected by default.
    QCOMPARE(shell->property("currentSection").toInt(), 0);
    QCOMPARE(shell->property("railExpanded").toBool(), false);
    const int collapsedWidth = shell->property("railWidth").toInt();
    QVERIFY(collapsedWidth >= 64 && collapsedWidth <= 80);

    QMetaObject::invokeMethod(shell.get(), "openSearch");
    QCOMPARE(shell->property("currentSection").toInt(), 1);

    controller.shutdown();
}

void QmlUiTest::chatFilterModelRowCountIsReactiveInQml()
{
    beygram::telegram::ChatListModel source;
    beygram::telegram::ChatFilterModel proxy;
    proxy.setSourceModel(&source);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("testModel"), &proxy);

    QQmlComponent comp(&engine);
    comp.setData("import QtQuick\nItem {\n"
                 "  property alias listCount: view.count\n"
                 "  ListView { id: view; model: testModel; delegate: Item { width: 10; height: 10 } }\n"
                 "}\n",
                 QUrl(QStringLiteral("qrc:/qt/qml/BeyGram/TestCount.qml")));
    QScopedPointer<QObject> obj(comp.create());
    QVERIFY2(obj, qPrintable(comp.errorString()));
    QCOMPARE(obj->property("listCount").toInt(), 0);

    beygram::telegram::ChatData chat1;
    chat1.id = 101;
    chat1.title = QStringLiteral("Alice");
    chat1.unreadCount = 3;
    beygram::telegram::ChatData chat2;
    chat2.id = 102;
    chat2.title = QStringLiteral("Bob");
    chat2.unreadCount = 99;
    source.setChats({chat1, chat2});
    QCoreApplication::processEvents();

    QCOMPARE(obj->property("listCount").toInt(), 2);

    proxy.setFilterText(QStringLiteral("Alice"));
    QCoreApplication::processEvents();
    QCOMPARE(obj->property("listCount").toInt(), 1);
}

void QmlUiTest::responsiveLayoutTransitionsCleanly()
{
    beygram::ui::ThemeController theme;
    beygram::ui::AppearanceStore appearance(
        QDir(m_tempDir->path()).filePath(QStringLiteral("appearance-resp-test.json")));
    beygram::ui::LocalizationManager lang(
        QDir(m_tempDir->path()).filePath(QStringLiteral("locale-resp-test.json")));
    beygram::ui::AppController controller;

    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramTheme", &theme);
    qmlRegisterSingletonInstance("BeyGram.Theme", 1, 0, "BeyGramAppearance", &appearance);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramApp", &controller);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramLang", &lang);
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramMediaViewer",
                                 controller.mediaViewer());
    qmlRegisterSingletonInstance("BeyGram.App", 1, 0, "BeyGramPlayback", controller.playback());

    QQmlApplicationEngine engine;
    lang.attachEngine(&engine);
    QQmlComponent comp(&engine, QStringLiteral("BeyGram"), QStringLiteral("ChatsPage"));
    QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
    QScopedPointer<QObject> chatsObj(comp.create());
    QVERIFY(chatsObj != nullptr);
    auto* item = qobject_cast<QQuickItem*>(chatsObj.get());
    QVERIFY(item != nullptr);

    item->setWidth(640);
    item->setHeight(680);
    QCOMPARE(item->property("wideLayout").toBool(), false);

    item->setWidth(1280);
    QCOMPARE(item->property("wideLayout").toBool(), true);
    const int listWidth = item->property("chatListWidth").toInt();
    QVERIFY(listWidth >= 320 && listWidth <= 390);

    controller.shutdown();
}

QTEST_MAIN(QmlUiTest)
#include "tst_qml_ui.moc"
