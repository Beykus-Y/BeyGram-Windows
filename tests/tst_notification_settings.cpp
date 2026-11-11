#include "ui/NotificationSettingsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using beygram::ui::NotificationSettings;
using beygram::ui::NotificationSettingsStore;

namespace {

QString settingsPath(const QTemporaryDir& directory)
{
    return directory.filePath(QStringLiteral("notifications/notifications.json"));
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

void verifyDefaults(const NotificationSettings& settings)
{
    QCOMPARE(settings.enabled, true);
    QCOMPARE(settings.showPreview, true);
    QCOMPARE(settings.sound, true);
    QCOMPARE(settings.notifyMutedChats, false);
}

} // namespace

class NotificationSettingsStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void defaults();
    void settersPersistAcrossInstances();
    void noSignalForNoOpSetter();
    void corruptJsonFallsBackToDefaults();
    void rejectsNonBooleanFieldsIndividually();
    void loadEmitsOnlyForRealChange();
};

void NotificationSettingsStoreTest::defaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    NotificationSettingsStore store(settingsPath(directory));
    QCOMPARE(store.filePath(), settingsPath(directory));

    // Before load() and after a load() of a missing file, the defaults hold.
    verifyDefaults(store.settings());
    store.load();
    verifyDefaults(store.settings());
    QVERIFY(!QFile::exists(store.filePath()));
}

void NotificationSettingsStoreTest::settersPersistAcrossInstances()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    NotificationSettingsStore store(settingsPath(directory));
    store.setEnabled(false);
    store.setShowPreview(false);
    store.setSound(false);
    store.setNotifyMutedChats(true);

    QVERIFY(QFile::exists(store.filePath()));

    QFile file(store.filePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    QVERIFY(document.isObject());
    const QJsonObject root = document.object();
    QCOMPARE(root.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(root.value(QStringLiteral("enabled")).toBool(true), false);
    QCOMPARE(root.value(QStringLiteral("showPreview")).toBool(true), false);
    QCOMPARE(root.value(QStringLiteral("sound")).toBool(true), false);
    QCOMPARE(root.value(QStringLiteral("notifyMutedChats")).toBool(false), true);

    NotificationSettingsStore reloaded(settingsPath(directory));
    reloaded.load();
    const NotificationSettings settings = reloaded.settings();
    QCOMPARE(settings.enabled, false);
    QCOMPARE(settings.showPreview, false);
    QCOMPARE(settings.sound, false);
    QCOMPARE(settings.notifyMutedChats, true);

    // Flipping back round trips as well, so neither direction is sticky.
    reloaded.setEnabled(true);
    reloaded.setShowPreview(true);
    reloaded.setSound(true);
    reloaded.setNotifyMutedChats(false);

    NotificationSettingsStore again(settingsPath(directory));
    again.load();
    verifyDefaults(again.settings());
}

void NotificationSettingsStoreTest::noSignalForNoOpSetter()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    NotificationSettingsStore store(settingsPath(directory));
    QSignalSpy spy(&store, &NotificationSettingsStore::changed);

    store.setEnabled(true);
    store.setShowPreview(true);
    store.setSound(true);
    store.setNotifyMutedChats(false);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!QFile::exists(store.filePath()));

    // Repeating an already stored value is a no-op as well.
    store.setEnabled(false);
    QCOMPARE(spy.count(), 1);
    store.setEnabled(false);
    QCOMPARE(spy.count(), 1);

    store.setNotifyMutedChats(true);
    QCOMPARE(spy.count(), 2);
    store.setNotifyMutedChats(true);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(store.settings().notifyMutedChats, true);
}

void NotificationSettingsStoreTest::corruptJsonFallsBackToDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    NotificationSettingsStore store(settingsPath(directory));

    // Move every switch off its default first, so a failed read has to land on
    // the default rather than merely keep the previous value.
    store.setEnabled(false);
    store.setShowPreview(false);
    store.setSound(false);
    store.setNotifyMutedChats(true);

    QVERIFY(writeFile(settingsPath(directory), QByteArray("{ this is not json")));
    store.load();
    verifyDefaults(store.settings());

    QVERIFY(writeFile(settingsPath(directory), QByteArray("[1, 2, 3]")));
    store.load();
    verifyDefaults(store.settings());

    QVERIFY(writeFile(settingsPath(directory), QByteArray()));
    store.load();
    verifyDefaults(store.settings());

    // Right shape, wrong types for every field.
    QVERIFY(writeFile(settingsPath(directory), R"({
        "version": "one",
        "enabled": "false",
        "showPreview": 0,
        "sound": null,
        "notifyMutedChats": []
    })"));
    store.load();
    verifyDefaults(store.settings());

    // A later good write still works after the damaged reads.
    store.setSound(false);
    NotificationSettingsStore reloaded(settingsPath(directory));
    reloaded.load();
    QCOMPARE(reloaded.settings().sound, false);
    QCOMPARE(reloaded.settings().enabled, true);
}

void NotificationSettingsStoreTest::rejectsNonBooleanFieldsIndividually()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    NotificationSettingsStore store(settingsPath(directory));

    // Only `sound` is damaged; the other three stored booleans must survive.
    QVERIFY(writeFile(settingsPath(directory), R"({
        "version": 1,
        "enabled": false,
        "showPreview": false,
        "sound": "yes",
        "notifyMutedChats": true
    })"));
    store.load();
    QCOMPARE(store.settings().enabled, false);
    QCOMPARE(store.settings().showPreview, false);
    QCOMPARE(store.settings().sound, true);
    QCOMPARE(store.settings().notifyMutedChats, true);

    // Numbers are not booleans either, even 0/1 which coerce so easily elsewhere.
    QVERIFY(writeFile(settingsPath(directory), R"({
        "version": 1,
        "enabled": 0,
        "showPreview": 1,
        "sound": false,
        "notifyMutedChats": true
    })"));
    store.load();
    QCOMPARE(store.settings().enabled, true);
    QCOMPARE(store.settings().showPreview, true);
    QCOMPARE(store.settings().sound, false);
    QCOMPARE(store.settings().notifyMutedChats, true);

    // A missing field keeps its default while the present ones are honoured.
    QVERIFY(writeFile(settingsPath(directory), R"({
        "version": 1,
        "showPreview": false,
        "notifyMutedChats": true
    })"));
    store.load();
    QCOMPARE(store.settings().enabled, true);
    QCOMPARE(store.settings().showPreview, false);
    QCOMPARE(store.settings().sound, true);
    QCOMPARE(store.settings().notifyMutedChats, true);
}

void NotificationSettingsStoreTest::loadEmitsOnlyForRealChange()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    NotificationSettingsStore store(settingsPath(directory));
    QSignalSpy spy(&store, &NotificationSettingsStore::changed);

    // Loading a missing file leaves the defaults alone: nothing to report.
    store.load();
    QCOMPARE(spy.count(), 0);

    QVERIFY(writeFile(settingsPath(directory), R"({"version":1,"enabled":false})"));
    store.load();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(store.settings().enabled, false);

    // The file now agrees with memory, so a second load is silent.
    store.load();
    QCOMPARE(spy.count(), 1);

    QVERIFY(writeFile(settingsPath(directory), R"({"version":1,"enabled":true})"));
    store.load();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(store.settings().enabled, true);

    // load() never writes the file it read.
    QFile file(store.filePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray onDisk = file.readAll();
    file.close();
    QCOMPARE(onDisk, QByteArray(R"({"version":1,"enabled":true})"));
}

QTEST_GUILESS_MAIN(NotificationSettingsStoreTest)
#include "tst_notification_settings.moc"
