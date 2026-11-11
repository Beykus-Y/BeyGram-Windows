#include "ui/WindowStateStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using beygram::ui::WindowState;
using beygram::ui::WindowStateStore;

namespace {

QString statePath(const QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("window/window-state.json"));
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

void verifyDefaults(const WindowState& state)
{
    QCOMPARE(state.x, 0);
    QCOMPARE(state.y, 0);
    QCOMPARE(state.width, 1280);
    QCOMPARE(state.height, 860);
    QCOMPARE(state.maximized, false);
}

} // namespace

class WindowStateTest final : public QObject {
    Q_OBJECT

private slots:
    void defaults();
    void roundTripAcrossInstances();
    void clampsGeometry();
    void noSignalForIdenticalSave();
    void corruptJsonFallsBackToDefaults();
};

void WindowStateTest::defaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowStateStore store(statePath(directory));
    QCOMPARE(store.filePath(), statePath(directory));

    // Before load() and after a load() of a missing file, the defaults hold.
    verifyDefaults(store.state());
    store.load();
    verifyDefaults(store.state());
    QVERIFY(!QFile::exists(store.filePath()));
}

void WindowStateTest::roundTripAcrossInstances()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowState state;
    state.x = -1920; // Secondary monitor placed left of the primary one.
    state.y = -180;
    state.width = 1600;
    state.height = 900;
    state.maximized = true;

    WindowStateStore store(statePath(directory));
    store.save(state);
    QCOMPARE(store.state().x, -1920);
    QVERIFY(QFile::exists(store.filePath()));

    QFile file(store.filePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    QVERIFY(document.isObject());
    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(root.value(QStringLiteral("x")).toInt(), -1920);
    QCOMPARE(root.value(QStringLiteral("y")).toInt(), -180);
    QCOMPARE(root.value(QStringLiteral("width")).toInt(), 1600);
    QCOMPARE(root.value(QStringLiteral("height")).toInt(), 900);
    QCOMPARE(root.value(QStringLiteral("maximized")).toBool(), true);

    WindowStateStore reloaded(statePath(directory));
    reloaded.load();
    const WindowState loaded = reloaded.state();
    QCOMPARE(loaded.x, -1920);
    QCOMPARE(loaded.y, -180);
    QCOMPARE(loaded.width, 1600);
    QCOMPARE(loaded.height, 900);
    QCOMPARE(loaded.maximized, true);
}

void WindowStateTest::clampsGeometry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowStateStore store(statePath(directory));

    // Too small to be usable becomes the minimum, too large becomes the maximum.
    WindowState tiny;
    tiny.width = 100;
    tiny.height = 200;
    store.save(tiny);
    QCOMPARE(store.state().width, 640);
    QCOMPARE(store.state().height, 640);

    WindowState huge;
    huge.width = 99999;
    huge.height = 123456;
    store.save(huge);
    QCOMPARE(store.state().width, 10000);
    QCOMPARE(store.state().height, 10000);

    // The stored file holds the clamped values, so a reload agrees.
    WindowStateStore reloaded(statePath(directory));
    reloaded.load();
    QCOMPARE(reloaded.state().width, 10000);
    QCOMPARE(reloaded.state().height, 10000);

    // Out-of-range values already in the file are clamped on read.
    QVERIFY(writeFile(statePath(directory),
                      R"({"version":1,"x":10,"y":20,"width":1,"height":50000,"maximized":false})"));
    reloaded.load();
    QCOMPARE(reloaded.state().x, 10);
    QCOMPARE(reloaded.state().y, 20);
    QCOMPARE(reloaded.state().width, 640);
    QCOMPARE(reloaded.state().height, 10000);

    // A missing or non-numeric size keeps the default rather than the clamp.
    QVERIFY(writeFile(statePath(directory),
                      R"({"version":1,"width":"wide","height":null})"));
    reloaded.load();
    QCOMPARE(reloaded.state().width, 1280);
    QCOMPARE(reloaded.state().height, 860);
}

void WindowStateTest::noSignalForIdenticalSave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowStateStore store(statePath(directory));
    QSignalSpy spy(&store, &WindowStateStore::changed);

    // Saving the defaults changes nothing, so no write and no signal.
    store.save(WindowState{});
    QCOMPARE(spy.count(), 0);
    QVERIFY(!QFile::exists(store.filePath()));

    WindowState state;
    state.width = 1024;
    state.height = 768;
    store.save(state);
    QCOMPARE(spy.count(), 1);

    // Repeating the same geometry, including a size that clamps to the stored
    // one, is a no-op.
    store.save(state);
    QCOMPARE(spy.count(), 1);
    store.save(store.state());
    QCOMPARE(spy.count(), 1);

    // load() of a file that agrees with memory reports nothing either.
    WindowStateStore reloaded(statePath(directory));
    QSignalSpy loadSpy(&reloaded, &WindowStateStore::changed);
    reloaded.load();
    QCOMPARE(loadSpy.count(), 1);
    reloaded.load();
    QCOMPARE(loadSpy.count(), 1);

    // A real change still emits exactly once.
    WindowState moved = state;
    moved.x = 42;
    store.save(moved);
    QCOMPARE(spy.count(), 2);
}

void WindowStateTest::corruptJsonFallsBackToDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    WindowStateStore store(statePath(directory));

    QVERIFY(writeFile(statePath(directory), QByteArray("{ this is not json")));
    store.load();
    verifyDefaults(store.state());

    QVERIFY(writeFile(statePath(directory), QByteArray("[1, 2, 3]")));
    store.load();
    verifyDefaults(store.state());

    QVERIFY(writeFile(statePath(directory), QByteArray()));
    store.load();
    verifyDefaults(store.state());

    // Right shape, wrong types: every field keeps its default.
    QVERIFY(writeFile(statePath(directory), R"({
        "version": "one",
        "x": "left",
        "y": null,
        "width": [],
        "height": {},
        "maximized": "yes"
    })"));
    store.load();
    verifyDefaults(store.state());

    // Partly damaged input keeps the readable fields and defaults the rest.
    QVERIFY(writeFile(statePath(directory),
                      R"({"version":1,"x":-50,"y":75,"width":1440,"height":"tall"})"));
    store.load();
    QCOMPARE(store.state().x, -50);
    QCOMPARE(store.state().y, 75);
    QCOMPARE(store.state().width, 1440);
    QCOMPARE(store.state().height, 860);
    QCOMPARE(store.state().maximized, false);

    // A later good save still works after the damaged reads.
    WindowState state;
    state.width = 1500;
    store.save(state);
    WindowStateStore reloaded(statePath(directory));
    reloaded.load();
    QCOMPARE(reloaded.state().width, 1500);
}

QTEST_GUILESS_MAIN(WindowStateTest)
#include "tst_window_state.moc"
