#include "telegram/WebAppSessionController.h"

#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

WebAppInfo makeInfo(const QString& url = QStringLiteral("https://mini.example/app"),
                    const QString& title = QStringLiteral("Example App"))
{
    WebAppInfo info;
    info.botUserId = 777;
    info.url = url;
    info.title = title;
    info.description = QStringLiteral("Buy things from the bot.");
    info.isSecure = isSafeWebAppUrl(url);
    return info;
}

} // namespace

class WebAppSessionControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void openActivatesSession();
    void openRejectsNonPositiveIds();
    void openWhileActiveReplacesSession();
    void reopeningIdenticalSessionIsQuiet();
    void closeIsNoOpWhenInactive();
    void closeEndsSession();
    void loadingIgnoredWhenInactive();
    void loadingReportsRealChanges();
    void errorIgnoredWhenInactive();
    void errorReportsRealChanges();
    void openClearsLoadingAndError();
    void clearResetsSession();
    void clearIsQuietWhenAlreadyEmpty();
};

void WebAppSessionControllerTest::startsInactive()
{
    WebAppSessionController controller;
    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.botUserId(), qint64(0));
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());

    const WebAppInfo empty = controller.info();
    QVERIFY(empty.url.isEmpty());
    QVERIFY(empty.title.isEmpty());
    QVERIFY(empty.description.isEmpty());
    QCOMPARE(empty.botUserId, qint64(0));
}

void WebAppSessionControllerTest::openActivatesSession()
{
    WebAppSessionController controller;
    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    const WebAppInfo info = makeInfo();
    controller.open(42, 777, info);

    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(42));
    QCOMPARE(controller.botUserId(), qint64(777));
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());

    const WebAppInfo stored = controller.info();
    QCOMPARE(stored.url, info.url);
    QCOMPARE(stored.title, info.title);
    QCOMPARE(stored.description, info.description);
    QCOMPARE(stored.botUserId, info.botUserId);
    QCOMPARE(stored.isSecure, info.isSecure);
}

void WebAppSessionControllerTest::openRejectsNonPositiveIds()
{
    WebAppSessionController controller;
    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    controller.open(0, 777, makeInfo());
    controller.open(-1, 777, makeInfo());
    controller.open(42, 0, makeInfo());
    controller.open(42, -777, makeInfo());

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.botUserId(), qint64(0));

    // A malformed open must not disturb the session already on screen either.
    controller.open(42, 777, makeInfo());
    spy.clear();

    controller.open(0, 0, makeInfo(QStringLiteral("https://other.example/app")));
    controller.open(42, -1, makeInfo(QStringLiteral("https://other.example/app")));

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(42));
    QCOMPARE(controller.botUserId(), qint64(777));
    QCOMPARE(controller.info().url, QStringLiteral("https://mini.example/app"));
}

void WebAppSessionControllerTest::openWhileActiveReplacesSession()
{
    WebAppSessionController controller;
    controller.open(42, 777, makeInfo());
    controller.setLoading(true);

    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    const WebAppInfo second =
        makeInfo(QStringLiteral("https://second.example/app"), QStringLiteral("Second"));
    controller.open(43, 888, second);

    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(43));
    QCOMPARE(controller.botUserId(), qint64(888));
    QCOMPARE(controller.info().url, QStringLiteral("https://second.example/app"));
    QCOMPARE(controller.info().title, QStringLiteral("Second"));
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());
}

void WebAppSessionControllerTest::reopeningIdenticalSessionIsQuiet()
{
    WebAppSessionController controller;
    const WebAppInfo info = makeInfo();
    controller.open(42, 777, info);

    QSignalSpy spy(&controller, &WebAppSessionController::changed);
    controller.open(42, 777, info);

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.active());
    QCOMPARE(controller.chatId(), qint64(42));
    QCOMPARE(controller.botUserId(), qint64(777));

    // A description change alone is still a real change.
    controller.open(42, 777, makeInfo(QStringLiteral("https://mini.example/app"),
                                     QStringLiteral("Renamed")));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.info().title, QStringLiteral("Renamed"));
}

void WebAppSessionControllerTest::closeIsNoOpWhenInactive()
{
    WebAppSessionController controller;
    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    controller.close();
    controller.close();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.botUserId(), qint64(0));
}

void WebAppSessionControllerTest::closeEndsSession()
{
    WebAppSessionController controller;
    controller.open(42, 777, makeInfo());
    controller.setLoading(true);
    controller.setError(QStringLiteral("Could not load"));

    QSignalSpy spy(&controller, &WebAppSessionController::changed);
    controller.close();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.botUserId(), qint64(0));
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.info().url.isEmpty());

    // Closing again changes nothing.
    controller.close();
    QCOMPARE(spy.count(), 1);
}

void WebAppSessionControllerTest::loadingIgnoredWhenInactive()
{
    WebAppSessionController controller;
    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    controller.setLoading(true);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.loading());
    QVERIFY(!controller.active());
}

void WebAppSessionControllerTest::loadingReportsRealChanges()
{
    WebAppSessionController controller;
    controller.open(42, 777, makeInfo());

    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    controller.setLoading(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.loading());

    controller.setLoading(true);
    QCOMPARE(spy.count(), 1);

    controller.setLoading(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.loading());
}

void WebAppSessionControllerTest::errorIgnoredWhenInactive()
{
    WebAppSessionController controller;
    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    controller.setError(QStringLiteral("Could not load"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.error().isEmpty());
    QVERIFY(!controller.active());
}

void WebAppSessionControllerTest::errorReportsRealChanges()
{
    WebAppSessionController controller;
    controller.open(42, 777, makeInfo());

    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    controller.setError(QStringLiteral("Could not load"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.error(), QStringLiteral("Could not load"));

    controller.setError(QStringLiteral("Could not load"));
    QCOMPARE(spy.count(), 1);

    controller.setError(QStringLiteral("Timed out"));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.error(), QStringLiteral("Timed out"));

    controller.setError(QString());
    QCOMPARE(spy.count(), 3);
    QVERIFY(controller.error().isEmpty());
}

void WebAppSessionControllerTest::openClearsLoadingAndError()
{
    WebAppSessionController controller;
    controller.open(42, 777, makeInfo());
    controller.setLoading(true);
    controller.setError(QStringLiteral("Could not load"));

    // Reopening the very same session still drops the stale flags.
    QSignalSpy spy(&controller, &WebAppSessionController::changed);
    controller.open(42, 777, makeInfo());

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.active());
}

void WebAppSessionControllerTest::clearResetsSession()
{
    WebAppSessionController controller;
    controller.open(42, 777, makeInfo());
    controller.setLoading(true);
    controller.setError(QStringLiteral("Could not load"));

    QSignalSpy spy(&controller, &WebAppSessionController::changed);
    controller.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.active());
    QCOMPARE(controller.chatId(), qint64(0));
    QCOMPARE(controller.botUserId(), qint64(0));
    QVERIFY(!controller.loading());
    QVERIFY(controller.error().isEmpty());
    QVERIFY(controller.info().url.isEmpty());
}

void WebAppSessionControllerTest::clearIsQuietWhenAlreadyEmpty()
{
    WebAppSessionController controller;
    QSignalSpy spy(&controller, &WebAppSessionController::changed);

    controller.clear();
    QCOMPARE(spy.count(), 0);

    controller.open(42, 777, makeInfo());
    controller.clear();
    spy.clear();

    controller.clear();
    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());
}

QTEST_GUILESS_MAIN(WebAppSessionControllerTest)
#include "tst_web_app_session_controller.moc"
