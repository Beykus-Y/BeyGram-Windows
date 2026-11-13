#include "ui/GroupAdminFacade.h"

#include <QList>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtTest>

using namespace beygram::ui;

namespace {

QString administrator()
{
    return QStringLiteral("administrator");
}

QString creator()
{
    return QStringLiteral("creator");
}

QString canChangeInfo()
{
    return QStringLiteral("canChangeInfo");
}

QString canDeleteMessages()
{
    return QStringLiteral("canDeleteMessages");
}

QString canBanUsers()
{
    return QStringLiteral("canBanUsers");
}

QString canInviteUsers()
{
    return QStringLiteral("canInviteUsers");
}

QString canPinMessages()
{
    return QStringLiteral("canPinMessages");
}

} // namespace

class GroupAdminFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setMembersLoadsParallelLists();
    void setMembersFillsMissingNamesAndStatuses();
    void setMembersDropsInvalidAndDuplicateIds();
    void setMembersIsSilentForSameRowsWhenIdle();
    void setMembersClearsSelectionAndDraft();
    void setMembersWithEmptyListsClearsThePanel();
    void selectMemberSelectsTheRow();
    void selectMemberRejectsOutOfRange();
    void selectMemberClearsSelection();
    void selectMemberDropsThePreviousDraft();
    void setRightUpdatesEveryRight();
    void setRightAcceptsSchemaSpellings();
    void setRightRejectsUnknownNames();
    void setRightWithoutSelectionIsIgnored();
    void setRightSameValueIsSilent();
    void creatorIsListedButReadOnly();
    void statusesAreNormalized();
    void saveAnnouncesTheEnabledRights();
    void saveWithNoRightsAnnouncesAnEmptyList();
    void saveWithoutSelectionIsSilent();
    void saveKeepsTheDraft();
};

void GroupAdminFacadeTest::startsEmpty()
{
    GroupAdminFacade facade;

    QVERIFY(facade.memberIds().isEmpty());
    QVERIFY(facade.memberNames().isEmpty());
    QVERIFY(facade.memberStatuses().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.selectedEditable());
    QVERIFY(!facade.canChangeInfo());
    QVERIFY(!facade.canDeleteMessages());
    QVERIFY(!facade.canBanUsers());
    QVERIFY(!facade.canInviteUsers());
    QVERIFY(!facade.canPinMessages());
}

void GroupAdminFacadeTest::setMembersLoadsParallelLists()
{
    GroupAdminFacade facade;
    QSignalSpy spy(&facade, &GroupAdminFacade::changed);

    // Names are trimmed on the way in, so " Alice " reaches the panel as
    // "Alice".
    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2"),
                      QStringList() << QStringLiteral(" Alice ") << QStringLiteral("Bob"),
                      QStringList() << administrator() << creator());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.memberIds(),
             QStringList() << QStringLiteral("1") << QStringLiteral("2"));
    QCOMPARE(facade.memberNames(),
             QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"));
    QCOMPARE(facade.memberStatuses(),
             QStringList() << administrator() << creator());
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.selectedEditable());
}

void GroupAdminFacadeTest::setMembersFillsMissingNamesAndStatuses()
{
    GroupAdminFacade facade;

    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << creator());

    // The tail of a short list is empty: the name stays empty for the panel to
    // fall back to the id, and the missing status reads as an administrator.
    QCOMPARE(facade.memberNames(), QStringList() << QStringLiteral("Alice") << QString());
    QCOMPARE(facade.memberStatuses(), QStringList() << creator() << administrator());
}

void GroupAdminFacadeTest::setMembersDropsInvalidAndDuplicateIds()
{
    GroupAdminFacade facade;

    // Empty, textual, zero and negative ids cannot name a Telegram user. The
    // first row of "42" wins, " 0042 " is the same user in another spelling,
    // and the surrounding spaces of " 43 " are trimmed.
    facade.setMembers(QStringList() << QString() << QStringLiteral("abc")
                                    << QStringLiteral("0") << QStringLiteral("-7")
                                    << QStringLiteral("42") << QStringLiteral("42")
                                    << QStringLiteral(" 0042 ") << QStringLiteral(" 43 "),
                      QStringList() << QStringLiteral("empty") << QStringLiteral("text")
                                    << QStringLiteral("zero") << QStringLiteral("negative")
                                    << QStringLiteral("first") << QStringLiteral("second")
                                    << QStringLiteral("leading zero") << QStringLiteral("trimmed"),
                      QStringList() << creator() << creator() << creator() << creator()
                                    << creator() << administrator() << administrator()
                                    << administrator());

    QCOMPARE(facade.memberIds(),
             QStringList() << QStringLiteral("42") << QStringLiteral("43"));
    QCOMPARE(facade.memberNames(),
             QStringList() << QStringLiteral("first") << QStringLiteral("trimmed"));
    QCOMPARE(facade.memberStatuses(), QStringList() << creator() << administrator());
}

void GroupAdminFacadeTest::setMembersIsSilentForSameRowsWhenIdle()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());

    QCOMPARE(spy.count(), 0);
}

void GroupAdminFacadeTest::setMembersClearsSelectionAndDraft()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2"),
                      QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"),
                      QStringList() << administrator() << administrator());
    facade.selectMember(0);
    facade.setRight(canChangeInfo(), true);
    QVERIFY(facade.canChangeInfo());

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2"),
                      QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"),
                      QStringList() << administrator() << administrator());

    // A reload starts a new session: the old selection and its grants are gone
    // rather than being carried onto a member the caller did not confirm.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.selectedEditable());
    QVERIFY(!facade.canChangeInfo());
}

void GroupAdminFacadeTest::setMembersWithEmptyListsClearsThePanel()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.setMembers(QStringList(), QStringList(), QStringList());

    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.memberIds().isEmpty());
    QVERIFY(facade.memberNames().isEmpty());
    QVERIFY(facade.memberStatuses().isEmpty());
    QCOMPARE(facade.selectedIndex(), -1);
}

void GroupAdminFacadeTest::selectMemberSelectsTheRow()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2"),
                      QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"),
                      QStringList() << administrator() << administrator());

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.selectMember(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.selectedIndex(), 1);
    QVERIFY(facade.selectedEditable());
    QVERIFY(!facade.canChangeInfo());
    QVERIFY(!facade.canPinMessages());
}

void GroupAdminFacadeTest::selectMemberRejectsOutOfRange()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2"),
                      QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"),
                      QStringList() << administrator() << administrator());
    facade.selectMember(0);

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.selectMember(-2);
    facade.selectMember(2);
    facade.selectMember(99);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.selectedIndex(), 0);
}

void GroupAdminFacadeTest::selectMemberClearsSelection()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());
    facade.selectMember(0);

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.selectMember(-1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.selectedEditable());

    // Clearing an already idle selection is silent.
    facade.selectMember(-1);
    QCOMPARE(spy.count(), 1);
}

void GroupAdminFacadeTest::selectMemberDropsThePreviousDraft()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2"),
                      QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"),
                      QStringList() << administrator() << administrator());

    facade.selectMember(0);
    facade.setRight(canChangeInfo(), true);
    facade.setRight(canPinMessages(), true);

    facade.selectMember(1);
    QVERIFY(!facade.canChangeInfo());
    QVERIFY(!facade.canPinMessages());

    // The first member's draft was not kept: the facade has one draft and the
    // payload carries no confirmed rights to restore.
    facade.selectMember(0);
    QVERIFY(!facade.canChangeInfo());
    QVERIFY(!facade.canPinMessages());
}

void GroupAdminFacadeTest::setRightUpdatesEveryRight()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());
    facade.selectMember(0);

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);

    facade.setRight(canChangeInfo(), true);
    QVERIFY(facade.canChangeInfo());
    QCOMPARE(spy.count(), 1);

    facade.setRight(canDeleteMessages(), true);
    QVERIFY(facade.canDeleteMessages());
    QCOMPARE(spy.count(), 2);

    facade.setRight(canBanUsers(), true);
    QVERIFY(facade.canBanUsers());
    QCOMPARE(spy.count(), 3);

    facade.setRight(canInviteUsers(), true);
    QVERIFY(facade.canInviteUsers());
    QCOMPARE(spy.count(), 4);

    facade.setRight(canPinMessages(), true);
    QVERIFY(facade.canPinMessages());
    QCOMPARE(spy.count(), 5);

    facade.setRight(canDeleteMessages(), false);
    QVERIFY(!facade.canDeleteMessages());
    QVERIFY(facade.canChangeInfo());
    QCOMPARE(spy.count(), 6);
}

void GroupAdminFacadeTest::setRightAcceptsSchemaSpellings()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());
    facade.selectMember(0);

    facade.setRight(QStringLiteral("can_change_info"), true);
    facade.setRight(QStringLiteral("can_delete_messages"), true);
    facade.setRight(QStringLiteral("can_restrict_members"), true);
    facade.setRight(QStringLiteral("can_invite_users"), true);
    facade.setRight(QStringLiteral("can_pin_messages"), true);

    QVERIFY(facade.canChangeInfo());
    QVERIFY(facade.canDeleteMessages());
    QVERIFY(facade.canBanUsers());
    QVERIFY(facade.canInviteUsers());
    QVERIFY(facade.canPinMessages());

    // The older fallback spelling and a differently cased loose spelling both
    // address the same field.
    facade.setRight(QStringLiteral("can_ban_users"), false);
    QVERIFY(!facade.canBanUsers());
    facade.setRight(QStringLiteral("CAN PIN MESSAGES"), false);
    QVERIFY(!facade.canPinMessages());
}

void GroupAdminFacadeTest::setRightRejectsUnknownNames()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());
    facade.selectMember(0);

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.setRight(QStringLiteral("canManageTopics"), true);
    facade.setRight(QStringLiteral("canPromoteMembers"), true);
    facade.setRight(QStringLiteral("can_manage_chat"), true);
    facade.setRight(QString(), true);
    facade.setRight(QStringLiteral("   "), true);
    facade.setRight(QStringLiteral("canChangeInfoExtra"), true);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.canChangeInfo());
    QVERIFY(!facade.canDeleteMessages());
    QVERIFY(!facade.canBanUsers());
    QVERIFY(!facade.canInviteUsers());
    QVERIFY(!facade.canPinMessages());
}

void GroupAdminFacadeTest::setRightWithoutSelectionIsIgnored()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.setRight(canChangeInfo(), true);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.canChangeInfo());
}

void GroupAdminFacadeTest::setRightSameValueIsSilent()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());
    facade.selectMember(0);
    facade.setRight(canChangeInfo(), true);

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.setRight(canChangeInfo(), true);

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.canChangeInfo());
}

void GroupAdminFacadeTest::creatorIsListedButReadOnly()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Owner"),
                      QStringList() << creator());
    facade.selectMember(0);

    QCOMPARE(facade.memberStatuses(), QStringList() << creator());
    QVERIFY(!facade.selectedEditable());

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.setRight(canChangeInfo(), true);
    facade.setRight(canPinMessages(), true);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.canChangeInfo());
    QVERIFY(!facade.canPinMessages());

    QSignalSpy saveSpy(&facade, &GroupAdminFacade::rightsSaveRequested);
    facade.save();
    QCOMPARE(saveSpy.count(), 0);
}

void GroupAdminFacadeTest::statusesAreNormalized()
{
    GroupAdminFacade facade;

    facade.setMembers(QStringList() << QStringLiteral("1") << QStringLiteral("2")
                                    << QStringLiteral("3") << QStringLiteral("4"),
                      QStringList() << QStringLiteral("A") << QStringLiteral("B")
                                    << QStringLiteral("C") << QStringLiteral("D"),
                      QStringList() << QStringLiteral("Owner")
                                    << QStringLiteral("chatMemberStatusCreator")
                                    << QStringLiteral("chatMemberStatusAdministrator")
                                    << QString());

    QCOMPARE(facade.memberStatuses(),
             QStringList() << creator() << creator() << administrator()
                           << administrator());

    facade.selectMember(0);
    QVERIFY(!facade.selectedEditable());
    facade.selectMember(1);
    QVERIFY(!facade.selectedEditable());
    facade.selectMember(2);
    QVERIFY(facade.selectedEditable());
    facade.selectMember(3);
    QVERIFY(facade.selectedEditable());
}

void GroupAdminFacadeTest::saveAnnouncesTheEnabledRights()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("7") << QStringLiteral("8"),
                      QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"),
                      QStringList() << administrator() << administrator());
    facade.selectMember(0);

    // Flip in a different order than the canonical one; the payload is sorted
    // by the canonical order, not by the order the switches were touched.
    facade.setRight(canPinMessages(), true);
    facade.setRight(canChangeInfo(), true);
    facade.setRight(canBanUsers(), true);

    QSignalSpy saveSpy(&facade, &GroupAdminFacade::rightsSaveRequested);
    facade.save();

    QCOMPARE(saveSpy.count(), 1);
    const QList<QVariant> arguments = saveSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("7"));
    QCOMPARE(arguments.at(1).toStringList(),
             QStringList() << canChangeInfo() << canBanUsers() << canPinMessages());
}

void GroupAdminFacadeTest::saveWithNoRightsAnnouncesAnEmptyList()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("7") << QStringLiteral("8"),
                      QStringList() << QStringLiteral("Alice") << QStringLiteral("Bob"),
                      QStringList() << administrator() << administrator());
    facade.selectMember(1);

    QSignalSpy saveSpy(&facade, &GroupAdminFacade::rightsSaveRequested);
    facade.save();

    // Saving an all-denied draft is a real request (it can strip every right),
    // so it is announced with an empty list rather than suppressed.
    QCOMPARE(saveSpy.count(), 1);
    const QList<QVariant> arguments = saveSpy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("8"));
    QVERIFY(arguments.at(1).toStringList().isEmpty());
}

void GroupAdminFacadeTest::saveWithoutSelectionIsSilent()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());

    QSignalSpy saveSpy(&facade, &GroupAdminFacade::rightsSaveRequested);
    facade.save();

    QCOMPARE(saveSpy.count(), 0);
}

void GroupAdminFacadeTest::saveKeepsTheDraft()
{
    GroupAdminFacade facade;
    facade.setMembers(QStringList() << QStringLiteral("1"),
                      QStringList() << QStringLiteral("Alice"),
                      QStringList() << administrator());
    facade.selectMember(0);
    facade.setRight(canInviteUsers(), true);

    QSignalSpy spy(&facade, &GroupAdminFacade::changed);
    facade.save();

    // Saving is a request, not a state transition: the panel keeps showing the
    // values it sent until the caller loads TDLib's answer.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.selectedIndex(), 0);
    QVERIFY(facade.canInviteUsers());
}

QTEST_GUILESS_MAIN(GroupAdminFacadeTest)
#include "tst_group_admin_facade.moc"
