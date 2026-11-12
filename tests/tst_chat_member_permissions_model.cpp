#include "telegram/ChatMemberPermissionsModel.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::telegram::ChatMemberPermissionsModel;
using beygram::telegram::ChatPermissions;
using beygram::telegram::MemberRights;

namespace {

/// The six rights the dialog may edit. "isAdmin" is deliberately absent.
QStringList editableNames()
{
    return {QStringLiteral("canSendMessages"), QStringLiteral("canSendMedia"),
            QStringLiteral("canSendPolls"),    QStringLiteral("canAddMembers"),
            QStringLiteral("canPinMessages"),  QStringLiteral("canManageTopics")};
}

/// Reads a right by the name setRight() takes, mirroring the documented mapping
/// so a test can check one field without knowing which one it is.
bool fieldOf(const MemberRights& rights, const QString& name)
{
    if (name == QLatin1String("canSendMessages"))
        return rights.canSendMessages;
    if (name == QLatin1String("canSendMedia"))
        return rights.canSendMedia;
    if (name == QLatin1String("canSendPolls"))
        return rights.canSendPolls;
    if (name == QLatin1String("canAddMembers"))
        return rights.canAddMembers;
    if (name == QLatin1String("canPinMessages"))
        return rights.canPinMessages;
    if (name == QLatin1String("canManageTopics"))
        return rights.canManageTopics;
    return rights.isAdmin;
}

MemberRights allRights()
{
    MemberRights rights;
    rights.canSendMessages = true;
    rights.canSendMedia = true;
    rights.canSendPolls = true;
    rights.canAddMembers = true;
    rights.canPinMessages = true;
    rights.canManageTopics = true;
    return rights;
}

/// A plain group member: may send text, nothing else.
MemberRights basicMember()
{
    MemberRights rights;
    rights.canSendMessages = true;
    return rights;
}

constexpr qint64 kChatId = -1001234567890; // supergroups have negative ids
constexpr qint64 kUserId = 424242;

void beginBasic(ChatMemberPermissionsModel& model)
{
    model.begin(kChatId, kUserId, basicMember());
}

} // namespace

class ChatMemberPermissionsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsDenyEverything();
    void startsInactive();
    void beginActivatesSession();
    void beginStartsClean();
    void beginKeepsAdminFlag();
    void beginRejectsZeroChatId();
    void beginRejectsZeroUserId();
    void beginRejectsNegativeIds();
    void beginRejectionLeavesSessionUntouched();
    void beginReplacesSession();
    void beginDropsPendingEdit();
    void beginIsSilentForIdenticalState();
    void beginSignalsWhenAdminFlagChanges();
    void cancelDeactivates();
    void cancelClearsState();
    void cancelIsSilentWhenInactive();
    void setRightAppliesEveryEditableRight();
    void setRightRejectsUnknownName();
    void setRightRejectsIsAdmin();
    void setRightRejectsIsAdminSpellingVariants();
    void setRightRequiresActiveSession();
    void setRightAcceptsStoredValueSilently();
    void setRightSignalsRealChangesOnly();
    void dirtyTracksEveryRight();
    void revertingByHandClearsDirty();
    void dirtyIsFalseWithoutSession();
    void markSavedClearsDirty();
    void markSavedKeepsDraftForFurtherEdits();
    void markSavedIsSilentWhenClean();
    void markSavedIsNoOpWithoutSession();
    void canSaveNeedsSessionAndEdit();
    void canSaveIsFalseAfterCancel();
    void bridgeMapsOverlappingRights();
    void bridgeLeavesAdminFalse();
};

void ChatMemberPermissionsModelTest::defaultsDenyEverything()
{
    const MemberRights rights;
    QVERIFY(!rights.canSendMessages);
    QVERIFY(!rights.canSendMedia);
    QVERIFY(!rights.canSendPolls);
    QVERIFY(!rights.canAddMembers);
    QVERIFY(!rights.canPinMessages);
    QVERIFY(!rights.canManageTopics);
    QVERIFY(!rights.isAdmin);
}

void ChatMemberPermissionsModelTest::startsInactive()
{
    ChatMemberPermissionsModel model;
    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(model.userId(), qint64(0));
    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
    QVERIFY(!model.rights().canSendMessages);
}

void ChatMemberPermissionsModelTest::beginActivatesSession()
{
    ChatMemberPermissionsModel model;
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    beginBasic(model);

    QVERIFY(model.active());
    QCOMPARE(model.chatId(), kChatId);
    QCOMPARE(model.userId(), kUserId);
    QCOMPARE(spy.count(), 1);
}

void ChatMemberPermissionsModelTest::beginStartsClean()
{
    // What TDLib reported is both the draft and the baseline, so opening the
    // dialog never looks like an unsaved edit.
    ChatMemberPermissionsModel model;
    model.begin(kChatId, kUserId, allRights());

    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
    QVERIFY(model.rights().canSendMessages);
    QVERIFY(model.rights().canManageTopics);
}

void ChatMemberPermissionsModelTest::beginKeepsAdminFlag()
{
    MemberRights rights = basicMember();
    rights.isAdmin = true;

    ChatMemberPermissionsModel model;
    model.begin(kChatId, kUserId, rights);

    QVERIFY(model.rights().isAdmin);
    QVERIFY(!model.dirty());
}

void ChatMemberPermissionsModelTest::beginRejectsZeroChatId()
{
    ChatMemberPermissionsModel model;
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    model.begin(0, kUserId, allRights());

    QVERIFY(!model.active());
    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(spy.count(), 0);
}

void ChatMemberPermissionsModelTest::beginRejectsZeroUserId()
{
    ChatMemberPermissionsModel model;
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    model.begin(kChatId, 0, allRights());

    QVERIFY(!model.active());
    QCOMPARE(model.userId(), qint64(0));
    QCOMPARE(spy.count(), 0);
}

void ChatMemberPermissionsModelTest::beginRejectsNegativeIds()
{
    ChatMemberPermissionsModel model;
    model.begin(-5, -7, allRights());

    QVERIFY(!model.active());
    QVERIFY(!model.dirty());
}

void ChatMemberPermissionsModelTest::beginRejectionLeavesSessionUntouched()
{
    // A half-loaded view must not be able to drop or move an open session.
    ChatMemberPermissionsModel model;
    beginBasic(model);
    model.setRight(QStringLiteral("canSendPolls"), true);

    model.begin(0, 999, allRights());

    QVERIFY(model.active());
    QCOMPARE(model.chatId(), kChatId);
    QCOMPARE(model.userId(), kUserId);
    QVERIFY(model.rights().canSendPolls);
    QVERIFY(model.dirty());
}

void ChatMemberPermissionsModelTest::beginReplacesSession()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);

    model.begin(kChatId, 777, allRights());

    QCOMPARE(model.userId(), qint64(777));
    QVERIFY(model.rights().canPinMessages);
    QVERIFY(!model.dirty());
}

void ChatMemberPermissionsModelTest::beginDropsPendingEdit()
{
    // A reload of the member replaces the draft instead of merging with it.
    ChatMemberPermissionsModel model;
    beginBasic(model);
    model.setRight(QStringLiteral("canSendPolls"), true);
    QVERIFY(model.dirty());

    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);
    beginBasic(model);

    QVERIFY(!model.dirty());
    QCOMPARE(spy.count(), 1);
}

void ChatMemberPermissionsModelTest::beginIsSilentForIdenticalState()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);

    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);
    beginBasic(model);

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.dirty());
}

void ChatMemberPermissionsModelTest::beginSignalsWhenAdminFlagChanges()
{
    // isAdmin cannot be edited, but a reload that reports a different status is
    // still a state the view has to show.
    MemberRights promoted = basicMember();
    promoted.isAdmin = true;

    ChatMemberPermissionsModel model;
    beginBasic(model);

    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);
    model.begin(kChatId, kUserId, promoted);

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.rights().isAdmin);
    QVERIFY(!model.dirty());
}

void ChatMemberPermissionsModelTest::cancelDeactivates()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);

    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);
    model.cancel();

    QVERIFY(!model.active());
    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
    QCOMPARE(spy.count(), 1);
}

void ChatMemberPermissionsModelTest::cancelClearsState()
{
    ChatMemberPermissionsModel model;
    model.begin(kChatId, kUserId, allRights());
    model.setRight(QStringLiteral("canPinMessages"), false);

    model.cancel();

    QCOMPARE(model.chatId(), qint64(0));
    QCOMPARE(model.userId(), qint64(0));
    QVERIFY(!model.rights().canSendMessages);
    QVERIFY(!model.rights().canSendMedia);
    QVERIFY(!model.rights().canPinMessages);
    QVERIFY(!model.rights().isAdmin);
}

void ChatMemberPermissionsModelTest::cancelIsSilentWhenInactive()
{
    ChatMemberPermissionsModel model;
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    model.cancel();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.active());
}

void ChatMemberPermissionsModelTest::setRightAppliesEveryEditableRight()
{
    const QStringList names = editableNames();
    QVERIFY(names.size() == 6);

    for (const QString& name : names) {
        ChatMemberPermissionsModel model;
        // Start from a member with every right denied so each toggle is a real
        // edit, including canSendMessages which basicMember() already grants.
        model.begin(kChatId, kUserId, MemberRights{});

        QVERIFY(model.setRight(name, true));
        QVERIFY(fieldOf(model.rights(), name));
        QVERIFY(model.dirty());
        QVERIFY(model.canSave());

        QVERIFY(model.setRight(name, false));
        QVERIFY(!fieldOf(model.rights(), name));
        QVERIFY(!model.dirty());
    }
}

void ChatMemberPermissionsModelTest::setRightRejectsUnknownName()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    const MemberRights before = model.rights();
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    QVERIFY(!model.setRight(QStringLiteral("canFly"), true));
    QVERIFY(!model.setRight(QStringLiteral("canSendMessage"), true)); // typo
    QVERIFY(!model.setRight(QString(), true));
    QVERIFY(!model.setRight(QStringLiteral("can_send_messages"), true));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.dirty());
    QCOMPARE(model.rights().canSendMessages, before.canSendMessages);
}

void ChatMemberPermissionsModelTest::setRightRejectsIsAdmin()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    QVERIFY(!model.setRight(QStringLiteral("isAdmin"), true));

    QVERIFY(!model.rights().isAdmin);
    QVERIFY(!model.dirty());
    QCOMPARE(spy.count(), 0);
}

void ChatMemberPermissionsModelTest::setRightRejectsIsAdminSpellingVariants()
{
    MemberRights rights = basicMember();
    rights.isAdmin = true;

    ChatMemberPermissionsModel model;
    model.begin(kChatId, kUserId, rights);
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    QVERIFY(!model.setRight(QStringLiteral("isAdmin"), false));
    QVERIFY(!model.setRight(QStringLiteral("isadmin"), false));

    QVERIFY(model.rights().isAdmin);
    QVERIFY(!model.dirty());
    QCOMPARE(spy.count(), 0);
}

void ChatMemberPermissionsModelTest::setRightRequiresActiveSession()
{
    ChatMemberPermissionsModel model;
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    QVERIFY(!model.setRight(QStringLiteral("canSendMedia"), true));

    QVERIFY(!model.rights().canSendMedia);
    QCOMPARE(spy.count(), 0);

    // The same call works once the dialog has a member.
    beginBasic(model);
    QVERIFY(model.setRight(QStringLiteral("canSendMedia"), true));
    QVERIFY(model.rights().canSendMedia);
}

void ChatMemberPermissionsModelTest::setRightAcceptsStoredValueSilently()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    // canSendMessages is already true; the name is valid, so the call is
    // accepted, but nothing changed and no view has to repaint.
    QVERIFY(model.setRight(QStringLiteral("canSendMessages"), true));
    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.dirty());
}

void ChatMemberPermissionsModelTest::setRightSignalsRealChangesOnly()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    QVERIFY(model.setRight(QStringLiteral("canSendPolls"), true));
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.dirty());

    QVERIFY(model.setRight(QStringLiteral("canSendPolls"), true));
    QCOMPARE(spy.count(), 1);

    QVERIFY(model.setRight(QStringLiteral("canSendPolls"), false));
    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.dirty());

    QVERIFY(model.setRight(QStringLiteral("canSendPolls"), false));
    QCOMPARE(spy.count(), 2);
}

void ChatMemberPermissionsModelTest::dirtyTracksEveryRight()
{
    // Each right on its own is enough to make the draft differ from the loaded
    // member, so none of them may be left out of the comparison.
    const QStringList names = editableNames();
    for (const QString& name : names) {
        ChatMemberPermissionsModel model;
        model.begin(kChatId, kUserId, allRights());
        QVERIFY(!model.dirty());

        QVERIFY(model.setRight(name, false));
        QVERIFY(model.dirty());
        QVERIFY(model.canSave());
    }
}

void ChatMemberPermissionsModelTest::revertingByHandClearsDirty()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);

    model.setRight(QStringLiteral("canSendPolls"), true);
    model.setRight(QStringLiteral("canAddMembers"), true);
    QVERIFY(model.dirty());

    model.setRight(QStringLiteral("canSendPolls"), false);
    QVERIFY(model.dirty());
    model.setRight(QStringLiteral("canAddMembers"), false);

    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
}

void ChatMemberPermissionsModelTest::dirtyIsFalseWithoutSession()
{
    ChatMemberPermissionsModel model;
    model.setRight(QStringLiteral("canSendMessages"), true);

    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
}

void ChatMemberPermissionsModelTest::markSavedClearsDirty()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    model.setRight(QStringLiteral("canManageTopics"), true);
    QVERIFY(model.canSave());

    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);
    model.markSaved();

    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
    QVERIFY(model.rights().canManageTopics);
    QCOMPARE(spy.count(), 1);
}

void ChatMemberPermissionsModelTest::markSavedKeepsDraftForFurtherEdits()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    model.setRight(QStringLiteral("canSendPolls"), true);
    model.markSaved();

    // The saved value is the new baseline: undoing it is a fresh edit, and
    // re-saving it is not.
    model.setRight(QStringLiteral("canSendPolls"), false);
    QVERIFY(model.dirty());

    model.setRight(QStringLiteral("canSendPolls"), true);
    QVERIFY(!model.dirty());
}

void ChatMemberPermissionsModelTest::markSavedIsSilentWhenClean()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    model.markSaved();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.dirty());
    QVERIFY(model.active());
}

void ChatMemberPermissionsModelTest::markSavedIsNoOpWithoutSession()
{
    ChatMemberPermissionsModel model;
    QSignalSpy spy(&model, &ChatMemberPermissionsModel::changed);

    model.markSaved();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.active());
    QVERIFY(!model.rights().isAdmin);
}

void ChatMemberPermissionsModelTest::canSaveNeedsSessionAndEdit()
{
    ChatMemberPermissionsModel model;
    QVERIFY(!model.canSave()); // no session

    beginBasic(model);
    QVERIFY(!model.canSave()); // session, no edit

    model.setRight(QStringLiteral("canAddMembers"), true);
    QVERIFY(model.canSave()); // both

    model.setRight(QStringLiteral("canAddMembers"), false);
    QVERIFY(!model.canSave()); // edit reverted
}

void ChatMemberPermissionsModelTest::canSaveIsFalseAfterCancel()
{
    ChatMemberPermissionsModel model;
    beginBasic(model);
    model.setRight(QStringLiteral("canSendPolls"), true);
    QVERIFY(model.canSave());

    model.cancel();

    QVERIFY(!model.canSave());
    QVERIFY(!model.active());
}

void ChatMemberPermissionsModelTest::bridgeMapsOverlappingRights()
{
    ChatPermissions permissions;
    permissions.canSendMessages = true;
    permissions.canSendMedia = true;
    permissions.canSendPolls = false;
    permissions.canInviteUsers = true;
    permissions.canPinMessages = true;
    permissions.canManageTopics = true;
    // Rights the per-member editor does not model are simply not carried over.
    permissions.canSendOther = true;
    permissions.canChangeInfo = true;
    permissions.canAddWebPreviews = true;

    const MemberRights rights = beygram::telegram::memberRightsFromChatPermissions(permissions);

    QVERIFY(rights.canSendMessages);
    QVERIFY(rights.canSendMedia);
    QVERIFY(!rights.canSendPolls);
    QVERIFY(rights.canAddMembers); // from canInviteUsers
    QVERIFY(rights.canPinMessages);
    QVERIFY(rights.canManageTopics);
}

void ChatMemberPermissionsModelTest::bridgeLeavesAdminFalse()
{
    // chatPermissions describes regular members, so it can never grant admin.
    ChatPermissions permissions;
    permissions.canSendMessages = true;
    permissions.canChangeInfo = true;

    const MemberRights rights = beygram::telegram::memberRightsFromChatPermissions(permissions);
    QVERIFY(!rights.isAdmin);

    // The bridge result starts a clean session like any other loaded member.
    ChatMemberPermissionsModel model;
    model.begin(kChatId, kUserId, rights);
    QVERIFY(model.active());
    QVERIFY(!model.dirty());
}

QTEST_GUILESS_MAIN(ChatMemberPermissionsModelTest)
#include "tst_chat_member_permissions_model.moc"
