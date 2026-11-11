#include "ui/AppearanceStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using beygram::ui::AppearanceSettings;
using beygram::ui::AppearanceStore;

namespace {

QString settingsPath(const QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("appearance/appearance.json"));
}

bool writeFile(const QString& path, const QByteArray& payload)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(payload) == payload.size();
}

void verifyDefaults(const AppearanceSettings& settings)
{
    QCOMPARE(settings.themePreset, QStringLiteral("Warm"));
    QCOMPARE(settings.themeMode, QStringLiteral("FollowSystem"));
    QCOMPARE(settings.designStyle, QStringLiteral("Default"));
    QCOMPARE(settings.bubbleStyle, QStringLiteral("Rounded"));
    QCOMPARE(settings.wallpaperPreset, QStringLiteral("none"));
    QCOMPARE(settings.wallpaperIntensity, 24);
    QCOMPARE(settings.reduceMotion, false);
}

} // namespace

class AppearanceStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void defaults();
    void settersPersistAcrossInstances();
    void invalidEnumValuesFallBack();
    void clampsWallpaperIntensity();
    void noSignalForNoOpSetter();
    void corruptJsonFallsBackToDefaults();
    void changedEmittedOncePerRealChange();
};

void AppearanceStoreTest::defaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppearanceStore store(settingsPath(directory));
    QCOMPARE(store.filePath(), settingsPath(directory));

    // Before load() and after a load() of a missing file, the defaults hold.
    verifyDefaults(store.settings());
    store.load();
    verifyDefaults(store.settings());
    QVERIFY(!QFile::exists(store.filePath()));
}

void AppearanceStoreTest::settersPersistAcrossInstances()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppearanceStore store(settingsPath(directory));
    store.setThemePreset(QStringLiteral("Graphite"));
    store.setThemeMode(QStringLiteral("Amoled"));
    store.setDesignStyle(QStringLiteral("LiquidGlass"));
    store.setBubbleStyle(QStringLiteral("Square"));
    store.setWallpaperPreset(QStringLiteral("violet-mesh"));
    store.setWallpaperIntensity(38);
    store.setReduceMotion(true);

    QVERIFY(QFile::exists(store.filePath()));

    QFile file(store.filePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    QVERIFY(document.isObject());
    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(root.value(QStringLiteral("themePreset")).toString(), QStringLiteral("Graphite"));
    QCOMPARE(root.value(QStringLiteral("wallpaperIntensity")).toInt(), 38);
    QCOMPARE(root.value(QStringLiteral("reduceMotion")).toBool(), true);

    AppearanceStore reloaded(settingsPath(directory));
    reloaded.load();
    const AppearanceSettings settings = reloaded.settings();
    QCOMPARE(settings.themePreset, QStringLiteral("Graphite"));
    QCOMPARE(settings.themeMode, QStringLiteral("Amoled"));
    QCOMPARE(settings.designStyle, QStringLiteral("LiquidGlass"));
    QCOMPARE(settings.bubbleStyle, QStringLiteral("Square"));
    QCOMPARE(settings.wallpaperPreset, QStringLiteral("violet-mesh"));
    QCOMPARE(settings.wallpaperIntensity, 38);
    QCOMPARE(settings.reduceMotion, true);
}

void AppearanceStoreTest::invalidEnumValuesFallBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppearanceStore store(settingsPath(directory));

    // Move every field off its default first, so a rejection has to land on the
    // default rather than merely keep the previous value.
    store.setThemePreset(QStringLiteral("Graphite"));
    store.setThemeMode(QStringLiteral("Dark"));
    store.setDesignStyle(QStringLiteral("Signature"));
    store.setBubbleStyle(QStringLiteral("Soft"));
    store.setWallpaperPreset(QStringLiteral("mint-air"));

    // MaterialYou is a real Android preset but not renderable on Windows.
    store.setThemePreset(QStringLiteral("MaterialYou"));
    QCOMPARE(store.settings().themePreset, QStringLiteral("Warm"));

    store.setThemeMode(QStringLiteral("Night"));
    QCOMPARE(store.settings().themeMode, QStringLiteral("FollowSystem"));

    // "Compact" is not a DesignStyle in Android, and names are case-sensitive.
    store.setDesignStyle(QStringLiteral("Compact"));
    QCOMPARE(store.settings().designStyle, QStringLiteral("Default"));
    store.setDesignStyle(QStringLiteral("signature"));
    QCOMPARE(store.settings().designStyle, QStringLiteral("Default"));

    store.setBubbleStyle(QStringLiteral("Sharp"));
    QCOMPARE(store.settings().bubbleStyle, QStringLiteral("Rounded"));

    store.setWallpaperPreset(QStringLiteral("lavender"));
    QCOMPARE(store.settings().wallpaperPreset, QStringLiteral("none"));

    AppearanceStore reloaded(settingsPath(directory));
    reloaded.load();
    verifyDefaults(reloaded.settings());

    // Damaged stored names read as defaults too.
    const QByteArray payload = R"({
        "version": 1,
        "themePreset": "MaterialYou",
        "themeMode": "Midnight",
        "designStyle": "Compact",
        "bubbleStyle": "Round",
        "wallpaperPreset": "preset:lavender-flow:24"
    })";
    QVERIFY(writeFile(settingsPath(directory), payload));
    reloaded.load();
    verifyDefaults(reloaded.settings());
}

void AppearanceStoreTest::clampsWallpaperIntensity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppearanceStore store(settingsPath(directory));
    store.setWallpaperIntensity(-5);
    QCOMPARE(store.settings().wallpaperIntensity, 0);
    store.setWallpaperIntensity(999);
    QCOMPARE(store.settings().wallpaperIntensity, 45);
    store.setWallpaperIntensity(45);
    QCOMPARE(store.settings().wallpaperIntensity, 45);
    store.setWallpaperIntensity(0);
    QCOMPARE(store.settings().wallpaperIntensity, 0);

    QVERIFY(writeFile(settingsPath(directory),
                      R"({"version":1,"wallpaperIntensity":120})"));
    AppearanceStore reloaded(settingsPath(directory));
    reloaded.load();
    QCOMPARE(reloaded.settings().wallpaperIntensity, 45);

    QVERIFY(writeFile(settingsPath(directory),
                      R"({"version":1,"wallpaperIntensity":-9})"));
    reloaded.load();
    QCOMPARE(reloaded.settings().wallpaperIntensity, 0);
}

void AppearanceStoreTest::noSignalForNoOpSetter()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppearanceStore store(settingsPath(directory));
    QSignalSpy spy(&store, &AppearanceStore::changed);

    store.setThemePreset(QStringLiteral("Warm"));
    store.setThemeMode(QStringLiteral("FollowSystem"));
    store.setDesignStyle(QStringLiteral("Default"));
    store.setBubbleStyle(QStringLiteral("Rounded"));
    store.setWallpaperPreset(QStringLiteral("none"));
    store.setWallpaperIntensity(24);
    store.setReduceMotion(false);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!QFile::exists(store.filePath()));

    // Repeating an already stored value is a no-op as well.
    store.setThemeMode(QStringLiteral("Dark"));
    QCOMPARE(spy.count(), 1);
    store.setThemeMode(QStringLiteral("Dark"));
    QCOMPARE(spy.count(), 1);

    // An invalid value that normalizes to the current value is also a no-op.
    store.setWallpaperIntensity(45);
    QCOMPARE(spy.count(), 2);
    store.setWallpaperIntensity(46);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(store.settings().wallpaperIntensity, 45);
}

void AppearanceStoreTest::corruptJsonFallsBackToDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppearanceStore store(settingsPath(directory));

    QVERIFY(writeFile(settingsPath(directory), QByteArray("{ this is not json")));
    store.load();
    verifyDefaults(store.settings());

    QVERIFY(writeFile(settingsPath(directory), QByteArray("[1, 2, 3]")));
    store.load();
    verifyDefaults(store.settings());

    QVERIFY(writeFile(settingsPath(directory), QByteArray()));
    store.load();
    verifyDefaults(store.settings());

    // Right shape, wrong types: every field keeps its default.
    QVERIFY(writeFile(settingsPath(directory), R"({
        "version": "one",
        "themePreset": 7,
        "themeMode": null,
        "designStyle": [],
        "bubbleStyle": {},
        "wallpaperPreset": true,
        "wallpaperIntensity": "lots",
        "reduceMotion": "yes"
    })"));
    store.load();
    verifyDefaults(store.settings());

    // A later good write still works after the damaged reads.
    store.setThemeMode(QStringLiteral("Light"));
    AppearanceStore reloaded(settingsPath(directory));
    reloaded.load();
    QCOMPARE(reloaded.settings().themeMode, QStringLiteral("Light"));
}

void AppearanceStoreTest::changedEmittedOncePerRealChange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppearanceStore store(settingsPath(directory));
    QSignalSpy spy(&store, &AppearanceStore::changed);

    store.setThemeMode(QStringLiteral("Dark"));
    QCOMPARE(spy.count(), 1);
    store.setThemeMode(QStringLiteral("Dark"));
    QCOMPARE(spy.count(), 1);

    store.setThemeMode(QStringLiteral("Light"));
    QCOMPARE(spy.count(), 2);

    store.setReduceMotion(true);
    QCOMPARE(spy.count(), 3);
    store.setReduceMotion(true);
    QCOMPARE(spy.count(), 3);

    store.setDesignStyle(QStringLiteral("Material3"));
    QCOMPARE(spy.count(), 4);

    // load() reports a real change once, and nothing when the file agrees.
    AppearanceStore reloaded(settingsPath(directory));
    QSignalSpy loadSpy(&reloaded, &AppearanceStore::changed);
    reloaded.load();
    QCOMPARE(loadSpy.count(), 1);
    reloaded.load();
    QCOMPARE(loadSpy.count(), 1);
}

QTEST_GUILESS_MAIN(AppearanceStoreTest)
#include "tst_appearance_store.moc"
