#include "telegram/ChatMemberListModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

ChatMember makeMember(qint64 userId, const QString& name, const QString& status,
                      bool isAdmin = false, bool isOwner = false)
{
    ChatMember member;
    member.userId = userId;
    member.displayName = name;
    member.status = status;
    member.isAdmin = isAdmin;
    member.isOwner = isOwner;
    return member;
}

QString memberStatus()
{
    return QStringLiteral("member");
}

QString administratorStatus()
{
    return QStringLiteral("administrator");
}

QString creatorStatus()
{
    return QStringLiteral("creator");
}

} // namespace

class ChatMemberListModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setMembersKeepsOrder();
    void setMembersDropsNonPositiveUserIds();
    void setMembersKeepsFirstDuplicate();
    void setMembersEmitsChangedOnce();
    void setMembersSameMembersIsSilent();
    void setMembersReorderedIsAChange();
    void setMembersReplacesPrevious();
    void membersReturnsCopy();
    void countMatchesMembers();
    void filteredEmptyQueryReturnsAll();
    void filteredMatchesNameCaseInsensitively();
    void filteredMatchesStatusCaseInsensitively();
    void filteredNoMatchIsEmpty();
    void filteredKeepsOrder();
    void adminsReturnsOnlyAdmins();
    void adminsExcludesOwnerFlaggedMember();
    void adminsEmptyWhenNoAdmins();
    void adminsKeepsOrder();
    void updateStatusUpdatesExisting();
    void updateStatusSameStatusIsNoOp();
    void updateStatusUnknownUserIsRejected();
    void removeDropsMember();
    void removeAbsentIsNoOp();
    void clearForgetsMembers();
    void clearOnEmptyModelIsSilent();
    void modelIsUsableAfterClear();
};

void ChatMemberListModelTest::startsEmpty()
{
    ChatMemberListModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.members().isEmpty());
    QVERIFY(model.admins().isEmpty());
    QVERIFY(model.filtered(QString()).isEmpty());
}

void ChatMemberListModelTest::setMembersKeepsOrder()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(30, QStringLiteral("Carol"), memberStatus()),
                      makeMember(10, QStringLiteral("Alice"), administratorStatus(), true),
                      makeMember(20, QStringLiteral("Bob"), memberStatus())});

    const QVector<ChatMember> members = model.members();
    QCOMPARE(int(members.size()), 3);
    // The server's order is the screen's order: nothing here sorts by name or id.
    QCOMPARE(members.at(0).userId, qint64(30));
    QCOMPARE(members.at(1).userId, qint64(10));
    QCOMPARE(members.at(2).userId, qint64(20));
}

void ChatMemberListModelTest::setMembersDropsNonPositiveUserIds()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(0, QStringLiteral("No id"), memberStatus()),
                      makeMember(7, QStringLiteral("Alice"), memberStatus()),
                      makeMember(-3, QStringLiteral("Bad id"), memberStatus())});

    // A row without a positive userId can be neither looked up nor acted on.
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.members().at(0).userId, qint64(7));
    QCOMPARE(model.members().at(0).displayName, QStringLiteral("Alice"));
}

void ChatMemberListModelTest::setMembersKeepsFirstDuplicate()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(5, QStringLiteral("First"), memberStatus()),
                      makeMember(5, QStringLiteral("Second"), administratorStatus(), true),
                      makeMember(6, QStringLiteral("Other"), memberStatus())});

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.members().at(0).displayName, QStringLiteral("First"));
    QCOMPARE(model.members().at(0).status, memberStatus());
    QVERIFY(!model.members().at(0).isAdmin);
    QCOMPARE(model.members().at(1).userId, qint64(6));
}

void ChatMemberListModelTest::setMembersEmitsChangedOnce()
{
    ChatMemberListModel model;
    QSignalSpy spy(&model, &ChatMemberListModel::changed);

    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});

    QCOMPARE(spy.count(), 1);
}

void ChatMemberListModelTest::setMembersSameMembersIsSilent()
{
    ChatMemberListModel model;
    const QVector<ChatMember> members{
        makeMember(1, QStringLiteral("Alice"), memberStatus()),
        makeMember(2, QStringLiteral("Bob"), administratorStatus(), true)};
    model.setMembers(members);

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    model.setMembers(members);

    // A refresh that brings the same data must not repaint the list.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 2);
}

void ChatMemberListModelTest::setMembersReorderedIsAChange()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), memberStatus())});

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    model.setMembers({makeMember(2, QStringLiteral("Bob"), memberStatus()),
                      makeMember(1, QStringLiteral("Alice"), memberStatus())});

    // Same members, different display order: the screen would look different.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.members().at(0).userId, qint64(2));
}

void ChatMemberListModelTest::setMembersReplacesPrevious()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});
    model.setMembers({makeMember(2, QStringLiteral("Bob"), memberStatus())});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.members().at(0).userId, qint64(2));
}

void ChatMemberListModelTest::membersReturnsCopy()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});

    QVector<ChatMember> copy = model.members();
    copy[0].displayName = QStringLiteral("Mutated");

    // Handing out a copy keeps the model's own state private.
    QCOMPARE(model.members().at(0).displayName, QStringLiteral("Alice"));
}

void ChatMemberListModelTest::countMatchesMembers()
{
    ChatMemberListModel model;
    QCOMPARE(model.count(), 0);

    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), memberStatus())});
    QCOMPARE(model.count(), int(model.members().size()));
    QCOMPARE(model.count(), 2);
}

void ChatMemberListModelTest::filteredEmptyQueryReturnsAll()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), administratorStatus(), true)});

    const QVector<ChatMember> all = model.filtered(QString());
    QCOMPARE(int(all.size()), 2);
    QCOMPARE(all.at(0).userId, qint64(1));
    QCOMPARE(all.at(1).userId, qint64(2));
}

void ChatMemberListModelTest::filteredMatchesNameCaseInsensitively()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), memberStatus())});

    QCOMPARE(int(model.filtered(QStringLiteral("ali")).size()), 1);
    QCOMPARE(model.filtered(QStringLiteral("ali")).at(0).userId, qint64(1));
    QCOMPARE(int(model.filtered(QStringLiteral("ALICE")).size()), 1);
    // Substring, not prefix: the middle of a name matches too.
    QCOMPARE(int(model.filtered(QStringLiteral("LIC")).size()), 1);
}

void ChatMemberListModelTest::filteredMatchesStatusCaseInsensitively()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), administratorStatus(), true)});

    const QVector<ChatMember> admins = model.filtered(QStringLiteral("ADMIN"));
    QCOMPARE(int(admins.size()), 1);
    QCOMPARE(admins.at(0).userId, qint64(2));
    QCOMPARE(int(model.filtered(QStringLiteral("Restricted")).size()), 0);
}

void ChatMemberListModelTest::filteredNoMatchIsEmpty()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});

    QVERIFY(model.filtered(QStringLiteral("Zoe")).isEmpty());
    QVERIFY(model.filtered(QStringLiteral("banned")).isEmpty());
}

void ChatMemberListModelTest::filteredKeepsOrder()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(9, QStringLiteral("Alice"), memberStatus()),
                      makeMember(3, QStringLiteral("Bob"), memberStatus()),
                      makeMember(5, QStringLiteral("Alicia"), memberStatus())});

    const QVector<ChatMember> matches = model.filtered(QStringLiteral("ali"));
    QCOMPARE(int(matches.size()), 2);
    QCOMPARE(matches.at(0).userId, qint64(9));
    QCOMPARE(matches.at(1).userId, qint64(5));
}

void ChatMemberListModelTest::adminsReturnsOnlyAdmins()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), administratorStatus(), true),
                      makeMember(2, QStringLiteral("Bob"), memberStatus()),
                      makeMember(3, QStringLiteral("Carol"), administratorStatus(), true)});

    const QVector<ChatMember> admins = model.admins();
    QCOMPARE(int(admins.size()), 2);
    QCOMPARE(admins.at(0).userId, qint64(1));
    QCOMPARE(admins.at(1).userId, qint64(3));
}

void ChatMemberListModelTest::adminsExcludesOwnerFlaggedMember()
{
    ChatMemberListModel model;
    // The owner carries isOwner instead of isAdmin, so it is not in admins();
    // a caller that wants "may administer" unions admins() with the owner.
    model.setMembers({makeMember(1, QStringLiteral("Owner"), creatorStatus(), false, true),
                      makeMember(2, QStringLiteral("Alice"), administratorStatus(), true)});

    const QVector<ChatMember> admins = model.admins();
    QCOMPARE(int(admins.size()), 1);
    QCOMPARE(admins.at(0).userId, qint64(2));
}

void ChatMemberListModelTest::adminsEmptyWhenNoAdmins()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), memberStatus())});

    QVERIFY(model.admins().isEmpty());
}

void ChatMemberListModelTest::adminsKeepsOrder()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(8, QStringLiteral("Alice"), administratorStatus(), true),
                      makeMember(2, QStringLiteral("Bob"), memberStatus()),
                      makeMember(4, QStringLiteral("Carol"), administratorStatus(), true)});

    const QVector<ChatMember> admins = model.admins();
    QCOMPARE(int(admins.size()), 2);
    QCOMPARE(admins.at(0).userId, qint64(8));
    QCOMPARE(admins.at(1).userId, qint64(4));
}

void ChatMemberListModelTest::updateStatusUpdatesExisting()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), memberStatus())});

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    QVERIFY(model.updateStatus(2, QStringLiteral("banned")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.members().at(1).status, QStringLiteral("banned"));
    // The other member is untouched.
    QCOMPARE(model.members().at(0).status, memberStatus());
    QCOMPARE(model.count(), 2);
}

void ChatMemberListModelTest::updateStatusSameStatusIsNoOp()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    QVERIFY(!model.updateStatus(1, memberStatus()));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.members().at(0).status, memberStatus());
}

void ChatMemberListModelTest::updateStatusUnknownUserIsRejected()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    QVERIFY(!model.updateStatus(99, QStringLiteral("banned")));

    QCOMPARE(spy.count(), 0);
    // No member is invented for an id the server never reported.
    QCOMPARE(model.count(), 1);
}

void ChatMemberListModelTest::removeDropsMember()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), memberStatus()),
                      makeMember(3, QStringLiteral("Carol"), memberStatus())});

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    QVERIFY(model.remove(2));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.members().at(0).userId, qint64(1));
    QCOMPARE(model.members().at(1).userId, qint64(3));
}

void ChatMemberListModelTest::removeAbsentIsNoOp()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    QVERIFY(!model.remove(42));

    // A "left" update for someone already gone must not repaint the list.
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void ChatMemberListModelTest::clearForgetsMembers()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus()),
                      makeMember(2, QStringLiteral("Bob"), administratorStatus(), true)});

    QSignalSpy spy(&model, &ChatMemberListModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.members().isEmpty());
    QCOMPARE(model.count(), 0);
    QVERIFY(model.admins().isEmpty());
}

void ChatMemberListModelTest::clearOnEmptyModelIsSilent()
{
    ChatMemberListModel model;
    QSignalSpy spy(&model, &ChatMemberListModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.members().isEmpty());
}

void ChatMemberListModelTest::modelIsUsableAfterClear()
{
    ChatMemberListModel model;
    model.setMembers({makeMember(1, QStringLiteral("Alice"), memberStatus())});
    model.clear();

    // A fresh snapshot after closing the chat behaves like a first load.
    model.setMembers({makeMember(4, QStringLiteral("Dave"), memberStatus())});
    QCOMPARE(model.count(), 1);
    QVERIFY(model.updateStatus(4, administratorStatus()));
    QCOMPARE(model.members().at(0).status, administratorStatus());
    QVERIFY(model.remove(4));
    QCOMPARE(model.count(), 0);
}

QTEST_GUILESS_MAIN(ChatMemberListModelTest)
#include "tst_chat_member_list_model.moc"
