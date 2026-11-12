#include "telegram/GroupCreationController.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using beygram::telegram::GroupCreationController;
using beygram::telegram::GroupKind;

namespace {

QString typeOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("@type")).toString();
}

QString titleOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("title")).toString();
}

QString descriptionOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("description")).toString();
}

QString usernameOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("username")).toString();
}

qint64 chatIdOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("chat_id")).toVariant().toLongLong();
}

qint64 userIdOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("user_id")).toVariant().toLongLong();
}

QJsonArray userIdsOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("user_ids")).toArray();
}

/// A draft that is valid as a basic group and carries two members.
void fillGroup(GroupCreationController& controller)
{
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("  Team  "));
    controller.setDescription(QStringLiteral("Coffee and compilers"));
    controller.addMember(10);
    controller.addMember(20);
}

} // namespace

class GroupCreationControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void beginActivatesDraft();
    void beginStartsFreshDraft();
    void beginSwitchesKind();
    void beginSameEmptyDraftIsSilent();
    void resetAbandonsDraft();
    void resetIsSilentWhenInactive();
    void settersIgnoredWhileInactive();
    void titleIsTrimmed();
    void titleIsCappedAt128CodePoints();
    void descriptionIsCappedAt255CodePoints();
    void descriptionKeepsWhitespace();
    void usernameIsCanonicalised();
    void setPublicSignalsRealChangesOnly();
    void validRequiresTitle();
    void validRequiresUsernameWhenPublic();
    void usernameLengthBounds();
    void usernameCharacterRules();
    void addMemberRejectsNonPositive();
    void addMemberRejectsDuplicates();
    void addMemberCapsAt200();
    void removeMember();
    void requestsEmptyWhenInvalid();
    void groupRequestsCreateAndConfigure();
    void supergroupRequestsCreateAndConfigure();
    void channelRequestsSetIsChannel();
    void publicRequestsIncludeUsername();
    void privateRequestsOmitUsername();
    void emptyDescriptionIsStillSent();
    void requestsAreRepeatable();
    void requestsCarryNoExtra();
    void resetDropsRequests();
};

void GroupCreationControllerTest::startsInactive()
{
    GroupCreationController controller;

    QVERIFY(!controller.active());
    QVERIFY(controller.kind() == GroupKind::Group);
    QVERIFY(controller.title().isEmpty());
    QVERIFY(controller.description().isEmpty());
    QVERIFY(!controller.isPublic());
    QVERIFY(controller.username().isEmpty());
    QVERIFY(controller.members().isEmpty());
    QVERIFY(!controller.valid());
    QVERIFY(controller.requests().isEmpty());
}

void GroupCreationControllerTest::beginActivatesDraft()
{
    GroupCreationController controller;
    QSignalSpy spy(&controller, &GroupCreationController::changed);

    controller.begin(GroupKind::Supergroup);

    QVERIFY(controller.active());
    QVERIFY(controller.kind() == GroupKind::Supergroup);
    QCOMPARE(spy.count(), 1);

    // A kind alone is not a group: the title is still missing.
    QVERIFY(!controller.valid());
    QVERIFY(controller.requests().isEmpty());
}

void GroupCreationControllerTest::beginStartsFreshDraft()
{
    GroupCreationController controller;
    fillGroup(controller);
    controller.setPublic(true, QStringLiteral("team_chat"));
    QVERIFY(controller.valid());

    QSignalSpy spy(&controller, &GroupCreationController::changed);
    controller.begin(GroupKind::Channel);

    QVERIFY(controller.active());
    QVERIFY(controller.kind() == GroupKind::Channel);
    QVERIFY(controller.title().isEmpty());
    QVERIFY(controller.description().isEmpty());
    QVERIFY(!controller.isPublic());
    QVERIFY(controller.username().isEmpty());
    QVERIFY(controller.members().isEmpty());
    QVERIFY(!controller.valid());
    QCOMPARE(spy.count(), 1);
}

void GroupCreationControllerTest::beginSwitchesKind()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("Team"));

    controller.begin(GroupKind::Supergroup);

    QVERIFY(controller.kind() == GroupKind::Supergroup);
    QVERIFY(controller.title().isEmpty());
}

void GroupCreationControllerTest::beginSameEmptyDraftIsSilent()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);

    QSignalSpy spy(&controller, &GroupCreationController::changed);
    controller.begin(GroupKind::Group);

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.active());
}

void GroupCreationControllerTest::resetAbandonsDraft()
{
    GroupCreationController controller;
    fillGroup(controller);
    QVERIFY(controller.valid());

    QSignalSpy spy(&controller, &GroupCreationController::changed);
    controller.reset();

    QVERIFY(!controller.active());
    QVERIFY(controller.kind() == GroupKind::Group);
    QVERIFY(controller.title().isEmpty());
    QVERIFY(controller.description().isEmpty());
    QVERIFY(controller.members().isEmpty());
    QVERIFY(controller.requests().isEmpty());
    QCOMPARE(spy.count(), 1);
}

void GroupCreationControllerTest::resetIsSilentWhenInactive()
{
    GroupCreationController controller;
    QSignalSpy spy(&controller, &GroupCreationController::changed);

    controller.reset();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.active());
}

void GroupCreationControllerTest::settersIgnoredWhileInactive()
{
    // A draft only exists between begin() and reset(); a field cannot be
    // recorded for a creation that was never started.
    GroupCreationController controller;
    QSignalSpy spy(&controller, &GroupCreationController::changed);

    controller.setTitle(QStringLiteral("Team"));
    controller.setDescription(QStringLiteral("Desc"));
    controller.setPublic(true, QStringLiteral("team_chat"));
    QVERIFY(!controller.addMember(1));
    QVERIFY(!controller.removeMember(1));

    QVERIFY(controller.title().isEmpty());
    QVERIFY(controller.description().isEmpty());
    QVERIFY(!controller.isPublic());
    QVERIFY(controller.username().isEmpty());
    QVERIFY(controller.members().isEmpty());
    QVERIFY(!controller.valid());
    QCOMPARE(spy.count(), 0);
}

void GroupCreationControllerTest::titleIsTrimmed()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);

    controller.setTitle(QStringLiteral("  Team \t"));
    QCOMPARE(controller.title(), QStringLiteral("Team"));
    QVERIFY(controller.valid());

    controller.setTitle(QStringLiteral("   "));
    QVERIFY(controller.title().isEmpty());
    QVERIFY(!controller.valid());
}

void GroupCreationControllerTest::titleIsCappedAt128CodePoints()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);

    controller.setTitle(QString(130, QLatin1Char('a')));
    QCOMPARE(controller.title().size(), 128);
    QCOMPARE(controller.title(), QString(128, QLatin1Char('a')));
    QVERIFY(controller.valid());

    // The cap counts code points, so an emoji is never split into a lone
    // surrogate and 128 emoji fit exactly.
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    controller.setTitle(emoji.repeated(130));
    QCOMPARE(controller.title(), emoji.repeated(128));
    QCOMPARE(controller.title().size(), 256);
}

void GroupCreationControllerTest::descriptionIsCappedAt255CodePoints()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);

    controller.setDescription(QString(300, QLatin1Char('d')));
    QCOMPARE(controller.description().size(), 255);
    QCOMPARE(controller.description(), QString(255, QLatin1Char('d')));

    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    controller.setDescription(emoji.repeated(300));
    QCOMPARE(controller.description(), emoji.repeated(255));
}

void GroupCreationControllerTest::descriptionKeepsWhitespace()
{
    // The contract bounds the description but does not ask for trimming.
    GroupCreationController controller;
    controller.begin(GroupKind::Group);

    controller.setDescription(QStringLiteral("  first line\nsecond line  "));
    QCOMPARE(controller.description(), QStringLiteral("  first line\nsecond line  "));
}

void GroupCreationControllerTest::usernameIsCanonicalised()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);

    controller.setPublic(true, QStringLiteral("  @Alice_2024  "));
    QVERIFY(controller.isPublic());
    QCOMPARE(controller.username(), QStringLiteral("alice_2024"));

    controller.setPublic(true, QStringLiteral("@@@Bob_99"));
    QCOMPARE(controller.username(), QStringLiteral("bob_99"));

    controller.setPublic(true, QStringLiteral(" @ Carol_7 "));
    QCOMPARE(controller.username(), QStringLiteral("carol_7"));
}

void GroupCreationControllerTest::setPublicSignalsRealChangesOnly()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("Team"));
    controller.setPublic(true, QStringLiteral("@Team_Chat"));

    QSignalSpy spy(&controller, &GroupCreationController::changed);

    // Same public flag and the same canonical name: nothing to announce.
    controller.setPublic(true, QStringLiteral(" team_chat "));
    QCOMPARE(spy.count(), 0);

    // Going private is a change, but the username is retained for a later
    // switch back to public.
    controller.setPublic(false, QStringLiteral("team_chat"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.isPublic());
    QCOMPARE(controller.username(), QStringLiteral("team_chat"));

    controller.setPublic(false, QStringLiteral("team_chat"));
    QCOMPARE(spy.count(), 1);
}

void GroupCreationControllerTest::validRequiresTitle()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    QVERIFY(!controller.valid());

    controller.setTitle(QStringLiteral("   "));
    QVERIFY(!controller.valid());

    controller.setTitle(QStringLiteral("Team"));
    QVERIFY(controller.valid());
}

void GroupCreationControllerTest::validRequiresUsernameWhenPublic()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("Team"));

    controller.setPublic(true, QString());
    QVERIFY(!controller.valid());

    controller.setPublic(true, QStringLiteral("abcd"));
    QVERIFY(!controller.valid());

    controller.setPublic(true, QStringLiteral("abcde"));
    QVERIFY(controller.valid());

    // A private draft ignores the username entirely.
    controller.setPublic(false, QStringLiteral("abcd"));
    QVERIFY(controller.valid());
}

void GroupCreationControllerTest::usernameLengthBounds()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("Team"));

    controller.setPublic(true, QString(4, QLatin1Char('a')));
    QVERIFY(!controller.valid());

    controller.setPublic(true, QString(5, QLatin1Char('a')));
    QVERIFY(controller.valid());

    controller.setPublic(true, QString(32, QLatin1Char('a')));
    QVERIFY(controller.valid());

    controller.setPublic(true, QString(33, QLatin1Char('a')));
    QVERIFY(!controller.valid());
}

void GroupCreationControllerTest::usernameCharacterRules()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("Team"));

    // Upper case is normalised away, so this is a valid lower-case name.
    controller.setPublic(true, QStringLiteral("ABCDE"));
    QVERIFY(controller.valid());
    QCOMPARE(controller.username(), QStringLiteral("abcde"));

    // The contract allows digits and underscores anywhere, including first.
    controller.setPublic(true, QStringLiteral("12345"));
    QVERIFY(controller.valid());
    controller.setPublic(true, QStringLiteral("_abcd"));
    QVERIFY(controller.valid());

    controller.setPublic(true, QStringLiteral("abc-d"));
    QVERIFY(!controller.valid());
    controller.setPublic(true, QStringLiteral("abc d"));
    QVERIFY(!controller.valid());
    controller.setPublic(true, QStringLiteral("abc@d"));
    QVERIFY(!controller.valid());
    controller.setPublic(true, QStringLiteral("abc.d"));
    QVERIFY(!controller.valid());
}

void GroupCreationControllerTest::addMemberRejectsNonPositive()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    QSignalSpy spy(&controller, &GroupCreationController::changed);

    QVERIFY(!controller.addMember(0));
    QVERIFY(!controller.addMember(-1));
    QVERIFY(!controller.addMember(-100));
    QVERIFY(controller.members().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void GroupCreationControllerTest::addMemberRejectsDuplicates()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    QSignalSpy spy(&controller, &GroupCreationController::changed);

    QVERIFY(controller.addMember(5));
    QVERIFY(!controller.addMember(5));
    QCOMPARE(controller.members(), (QVector<qint64>{5}));
    QCOMPARE(spy.count(), 1);
}

void GroupCreationControllerTest::addMemberCapsAt200()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);

    for (qint64 userId = 1; userId <= 200; ++userId)
        QVERIFY(controller.addMember(userId));

    QCOMPARE(controller.members().size(), 200);
    QVERIFY(!controller.addMember(201));
    QCOMPARE(controller.members().size(), 200);

    // Removing one frees a slot again.
    QVERIFY(controller.removeMember(100));
    QVERIFY(controller.addMember(201));
    QCOMPARE(controller.members().size(), 200);
    QCOMPARE(controller.members().last(), qint64(201));
}

void GroupCreationControllerTest::removeMember()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    QVERIFY(controller.addMember(1));
    QVERIFY(controller.addMember(2));

    QSignalSpy spy(&controller, &GroupCreationController::changed);

    QVERIFY(controller.removeMember(1));
    QCOMPARE(controller.members(), (QVector<qint64>{2}));
    QCOMPARE(spy.count(), 1);

    QVERIFY(!controller.removeMember(99));
    QCOMPARE(spy.count(), 1);

    QVERIFY(controller.removeMember(2));
    QVERIFY(controller.members().isEmpty());
    QCOMPARE(spy.count(), 2);
}

void GroupCreationControllerTest::requestsEmptyWhenInvalid()
{
    GroupCreationController controller;
    QVERIFY(controller.requests().isEmpty());

    controller.begin(GroupKind::Supergroup);
    QVERIFY(controller.requests().isEmpty());

    controller.setTitle(QStringLiteral("Team"));
    controller.setPublic(true, QStringLiteral("bad"));
    QVERIFY(!controller.valid());
    QVERIFY(controller.requests().isEmpty());
}

void GroupCreationControllerTest::groupRequestsCreateAndConfigure()
{
    GroupCreationController controller;
    fillGroup(controller);
    QVERIFY(controller.valid());

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 5);

    QCOMPARE(typeOf(requests.at(0)), QStringLiteral("createNewBasicGroupChat"));
    QCOMPARE(titleOf(requests.at(0)), QStringLiteral("Team"));
    QVERIFY(userIdsOf(requests.at(0)).isEmpty());
    QCOMPARE(requests.at(0).value(QStringLiteral("message_auto_delete_time")).toInt(), 0);

    QCOMPARE(typeOf(requests.at(1)), QStringLiteral("setChatTitle"));
    QCOMPARE(chatIdOf(requests.at(1)), qint64(0));
    QCOMPARE(titleOf(requests.at(1)), QStringLiteral("Team"));

    QCOMPARE(typeOf(requests.at(2)), QStringLiteral("setChatDescription"));
    QCOMPARE(chatIdOf(requests.at(2)), qint64(0));
    QCOMPARE(descriptionOf(requests.at(2)), QStringLiteral("Coffee and compilers"));

    QCOMPARE(typeOf(requests.at(3)), QStringLiteral("addChatMember"));
    QCOMPARE(chatIdOf(requests.at(3)), qint64(0));
    QCOMPARE(userIdOf(requests.at(3)), qint64(10));
    QCOMPARE(requests.at(3).value(QStringLiteral("forward_limit")).toInt(), 0);

    QCOMPARE(typeOf(requests.at(4)), QStringLiteral("addChatMember"));
    QCOMPARE(userIdOf(requests.at(4)), qint64(20));
}

void GroupCreationControllerTest::supergroupRequestsCreateAndConfigure()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Supergroup);
    controller.setTitle(QStringLiteral("Team"));
    controller.setDescription(QStringLiteral("Desc"));
    controller.addMember(7);

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 4);

    const QJsonObject create = requests.at(0);
    QCOMPARE(typeOf(create), QStringLiteral("createNewSupergroupChat"));
    QCOMPARE(titleOf(create), QStringLiteral("Team"));
    QCOMPARE(create.value(QStringLiteral("is_forum")).toBool(true), false);
    QCOMPARE(create.value(QStringLiteral("is_channel")).toBool(true), false);
    QCOMPARE(descriptionOf(create), QStringLiteral("Desc"));
    QVERIFY(create.value(QStringLiteral("location")).isNull());
    QCOMPARE(create.value(QStringLiteral("message_auto_delete_time")).toInt(), 0);
    QCOMPARE(create.value(QStringLiteral("for_import")).toBool(true), false);

    QCOMPARE(typeOf(requests.at(1)), QStringLiteral("setChatTitle"));
    QCOMPARE(typeOf(requests.at(2)), QStringLiteral("setChatDescription"));
    QCOMPARE(typeOf(requests.at(3)), QStringLiteral("addChatMember"));
    QCOMPARE(userIdOf(requests.at(3)), qint64(7));
}

void GroupCreationControllerTest::channelRequestsSetIsChannel()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Channel);
    controller.setTitle(QStringLiteral("News"));

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 3);
    QCOMPARE(typeOf(requests.at(0)), QStringLiteral("createNewSupergroupChat"));
    QCOMPARE(requests.at(0).value(QStringLiteral("is_channel")).toBool(false), true);
    QCOMPARE(requests.at(0).value(QStringLiteral("is_forum")).toBool(true), false);
}

void GroupCreationControllerTest::publicRequestsIncludeUsername()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Supergroup);
    controller.setTitle(QStringLiteral("Team"));
    controller.setPublic(true, QStringLiteral("@My_Channel"));
    controller.addMember(5);

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 5);
    QCOMPARE(typeOf(requests.at(3)), QStringLiteral("setChatUsername"));
    QCOMPARE(chatIdOf(requests.at(3)), qint64(0));
    QCOMPARE(usernameOf(requests.at(3)), QStringLiteral("my_channel"));
    QCOMPARE(typeOf(requests.at(4)), QStringLiteral("addChatMember"));
    QCOMPARE(userIdOf(requests.at(4)), qint64(5));
}

void GroupCreationControllerTest::privateRequestsOmitUsername()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("Team"));
    controller.setPublic(false, QStringLiteral("team_chat"));

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 3);
    for (const QJsonObject& request : requests)
        QVERIFY(typeOf(request) != QLatin1String("setChatUsername"));
}

void GroupCreationControllerTest::emptyDescriptionIsStillSent()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Group);
    controller.setTitle(QStringLiteral("Team"));

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 3);
    QCOMPARE(typeOf(requests.at(2)), QStringLiteral("setChatDescription"));
    QVERIFY(requests.at(2).contains(QStringLiteral("description")));
    QCOMPARE(descriptionOf(requests.at(2)), QString());
}

void GroupCreationControllerTest::requestsAreRepeatable()
{
    GroupCreationController controller;
    fillGroup(controller);

    const QVector<QJsonObject> first = controller.requests();
    const QVector<QJsonObject> second = controller.requests();

    QCOMPARE(first.size(), second.size());
    for (qsizetype index = 0; index < first.size(); ++index) {
        QCOMPARE(typeOf(first.at(index)), typeOf(second.at(index)));
        QVERIFY(first.at(index) == second.at(index));
    }
    QVERIFY(controller.valid());
    QCOMPARE(controller.members(), (QVector<qint64>{10, 20}));
}

void GroupCreationControllerTest::requestsCarryNoExtra()
{
    GroupCreationController controller;
    controller.begin(GroupKind::Channel);
    controller.setTitle(QStringLiteral("News"));
    controller.setDescription(QStringLiteral("Updates"));
    controller.setPublic(true, QStringLiteral("news_channel"));
    controller.addMember(42);

    const QVector<QJsonObject> requests = controller.requests();
    QVERIFY(!requests.isEmpty());
    for (const QJsonObject& request : requests) {
        QVERIFY(!request.contains(QStringLiteral("@extra")));
        const QString type = typeOf(request);
        QVERIFY(type == QLatin1String("createNewBasicGroupChat")
                || type == QLatin1String("createNewSupergroupChat")
                || type == QLatin1String("setChatTitle")
                || type == QLatin1String("setChatDescription")
                || type == QLatin1String("setChatUsername")
                || type == QLatin1String("addChatMember"));
    }
}

void GroupCreationControllerTest::resetDropsRequests()
{
    GroupCreationController controller;
    fillGroup(controller);
    QVERIFY(!controller.requests().isEmpty());

    controller.reset();

    QVERIFY(controller.requests().isEmpty());
    QVERIFY(!controller.valid());
}

QTEST_GUILESS_MAIN(GroupCreationControllerTest)
#include "tst_group_creation_controller.moc"
