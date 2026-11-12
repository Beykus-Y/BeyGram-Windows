#include "telegram/UserStatusModel.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

PresenceInfo presence(const QString& status, qint64 lastSeen = 0)
{
    PresenceInfo info;
    info.status = status;
    info.lastSeen = lastSeen;
    info.online = status == QLatin1String("userStatusOnline");
    return info;
}

} // namespace

class UserStatusModelTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsToUnknown();
    void labelsMatchKind_data();
    void labelsMatchKind();
    void onlineOnlyForOnlineKind();
    void offlineKeepsLastSeen();
    void offlineClampsNegativeLastSeen();
    void nonOfflineKindsDropLastSeen_data();
    void nonOfflineKindsDropLastSeen();
    void emitsOnlyOnRealChange();
    void redundantLastSeenOnNonOfflineIsNoOp();
    void clearResetsAndEmits();
    void clearIsNoOpWhenAlreadyEmpty();
    void mapsPresenceInfo_data();
    void mapsPresenceInfo();
    void mapsPresenceInfoOfflineTimestamp();
};

void UserStatusModelTest::defaultsToUnknown()
{
    UserStatusModel model;
    QCOMPARE(model.kind(), PresenceKind::Unknown);
    QCOMPARE(model.lastSeenUnix(), qint64(0));
    QCOMPARE(model.online(), false);
    QCOMPARE(model.label(), QString());
}

void UserStatusModelTest::labelsMatchKind_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("label");

    QTest::newRow("unknown") << int(PresenceKind::Unknown) << QString();
    QTest::newRow("online") << int(PresenceKind::Online) << QStringLiteral("online");
    QTest::newRow("offline") << int(PresenceKind::Offline) << QStringLiteral("offline");
    QTest::newRow("recently") << int(PresenceKind::Recently) << QStringLiteral("last seen recently");
    QTest::newRow("week") << int(PresenceKind::LastWeek) << QStringLiteral("last seen within a week");
    QTest::newRow("month") << int(PresenceKind::LastMonth) << QStringLiteral("last seen within a month");
    QTest::newRow("hidden") << int(PresenceKind::Hidden) << QString();
}

void UserStatusModelTest::labelsMatchKind()
{
    QFETCH(int, kind);
    QFETCH(QString, label);

    UserStatusModel model;
    model.setStatus(PresenceKind(kind));
    QCOMPARE(model.kind(), PresenceKind(kind));
    QCOMPARE(model.label(), label);
}

void UserStatusModelTest::onlineOnlyForOnlineKind()
{
    UserStatusModel model;

    model.setStatus(PresenceKind::Online);
    QCOMPARE(model.online(), true);

    model.setStatus(PresenceKind::Offline, 1700000000);
    QCOMPARE(model.online(), false);

    model.setStatus(PresenceKind::Recently);
    QCOMPARE(model.online(), false);
}

void UserStatusModelTest::offlineKeepsLastSeen()
{
    UserStatusModel model;
    model.setStatus(PresenceKind::Offline, 1700000000);
    QCOMPARE(model.kind(), PresenceKind::Offline);
    QCOMPARE(model.lastSeenUnix(), qint64(1700000000));

    model.setStatus(PresenceKind::Offline, 1700000123);
    QCOMPARE(model.lastSeenUnix(), qint64(1700000123));
}

void UserStatusModelTest::offlineClampsNegativeLastSeen()
{
    UserStatusModel model;
    model.setStatus(PresenceKind::Offline, -42);
    QCOMPARE(model.kind(), PresenceKind::Offline);
    QCOMPARE(model.lastSeenUnix(), qint64(0));
}

void UserStatusModelTest::nonOfflineKindsDropLastSeen_data()
{
    QTest::addColumn<int>("kind");

    QTest::newRow("unknown") << int(PresenceKind::Unknown);
    QTest::newRow("online") << int(PresenceKind::Online);
    QTest::newRow("recently") << int(PresenceKind::Recently);
    QTest::newRow("week") << int(PresenceKind::LastWeek);
    QTest::newRow("month") << int(PresenceKind::LastMonth);
    QTest::newRow("hidden") << int(PresenceKind::Hidden);
}

void UserStatusModelTest::nonOfflineKindsDropLastSeen()
{
    QFETCH(int, kind);

    UserStatusModel model;
    model.setStatus(PresenceKind::Offline, 1700000000);
    QCOMPARE(model.lastSeenUnix(), qint64(1700000000));

    model.setStatus(PresenceKind(kind), 1700000000);
    QCOMPARE(model.kind(), PresenceKind(kind));
    QCOMPARE(model.lastSeenUnix(), qint64(0));
}

void UserStatusModelTest::emitsOnlyOnRealChange()
{
    UserStatusModel model;
    QSignalSpy spy(&model, &UserStatusModel::changed);

    model.setStatus(PresenceKind::Online);
    QCOMPARE(spy.count(), 1);

    model.setStatus(PresenceKind::Online);
    QCOMPARE(spy.count(), 1);

    model.setStatus(PresenceKind::Online, 1700000000);
    QCOMPARE(spy.count(), 1);

    model.setStatus(PresenceKind::Offline, 1700000000);
    QCOMPARE(spy.count(), 2);

    model.setStatus(PresenceKind::Offline, 1700000000);
    QCOMPARE(spy.count(), 2);

    model.setStatus(PresenceKind::Offline, 1700000001);
    QCOMPARE(spy.count(), 3);

    model.setStatus(PresenceKind::LastWeek);
    QCOMPARE(spy.count(), 4);
    QCOMPARE(model.lastSeenUnix(), qint64(0));
}

void UserStatusModelTest::redundantLastSeenOnNonOfflineIsNoOp()
{
    UserStatusModel model;
    model.setStatus(PresenceKind::Recently);

    QSignalSpy spy(&model, &UserStatusModel::changed);
    model.setStatus(PresenceKind::Recently, 1700000000);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.lastSeenUnix(), qint64(0));
}

void UserStatusModelTest::clearResetsAndEmits()
{
    UserStatusModel model;
    model.setStatus(PresenceKind::Offline, 1700000000);

    QSignalSpy spy(&model, &UserStatusModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.kind(), PresenceKind::Unknown);
    QCOMPARE(model.lastSeenUnix(), qint64(0));
    QCOMPARE(model.online(), false);
    QCOMPARE(model.label(), QString());
}

void UserStatusModelTest::clearIsNoOpWhenAlreadyEmpty()
{
    UserStatusModel model;
    QSignalSpy spy(&model, &UserStatusModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setStatus(PresenceKind::Hidden);
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void UserStatusModelTest::mapsPresenceInfo_data()
{
    QTest::addColumn<QString>("status");
    QTest::addColumn<int>("kind");

    QTest::newRow("online") << QStringLiteral("userStatusOnline") << int(PresenceKind::Online);
    QTest::newRow("offline") << QStringLiteral("userStatusOffline") << int(PresenceKind::Offline);
    QTest::newRow("recently") << QStringLiteral("userStatusRecently") << int(PresenceKind::Recently);
    QTest::newRow("week") << QStringLiteral("userStatusLastWeek") << int(PresenceKind::LastWeek);
    QTest::newRow("month") << QStringLiteral("userStatusLastMonth") << int(PresenceKind::LastMonth);
    QTest::newRow("empty") << QStringLiteral("userStatusEmpty") << int(PresenceKind::Hidden);
    QTest::newRow("missing") << QString() << int(PresenceKind::Unknown);
    QTest::newRow("future") << QStringLiteral("userStatusVacation") << int(PresenceKind::Unknown);
}

void UserStatusModelTest::mapsPresenceInfo()
{
    QFETCH(QString, status);
    QFETCH(int, kind);

    UserStatusModel model;
    QSignalSpy spy(&model, &UserStatusModel::changed);
    model.setStatus(presence(status));

    QCOMPARE(model.kind(), PresenceKind(kind));
    QCOMPARE(spy.count(), model.kind() == PresenceKind::Unknown ? 0 : 1);
}

void UserStatusModelTest::mapsPresenceInfoOfflineTimestamp()
{
    UserStatusModel model;

    model.setStatus(presence(QStringLiteral("userStatusOffline"), 1700000000));
    QCOMPARE(model.kind(), PresenceKind::Offline);
    QCOMPARE(model.lastSeenUnix(), qint64(1700000000));

    model.setStatus(presence(QStringLiteral("userStatusOnline")));
    QCOMPARE(model.kind(), PresenceKind::Online);
    QCOMPARE(model.lastSeenUnix(), qint64(0));
    QCOMPARE(model.online(), true);
}

QTEST_GUILESS_MAIN(UserStatusModelTest)
#include "tst_user_status_model.moc"
