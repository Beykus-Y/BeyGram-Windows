#include "ui/ProfileFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <QtTest>

using beygram::ui::ProfileFacade;

namespace {

/// The profile as TDLib would report it for the current user.
void loadAda(ProfileFacade& facade)
{
    facade.load(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
}

QString firstOf(const QVariantList& arguments)
{
    return arguments.at(0).toString();
}

} // namespace

class ProfileFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void loadsProfile();
    void loadDropsStaleUsernameAnswer();
    void storesRawEdits();
    void repeatedValueIsSilent();
    void becomesDirtyOnEdit();
    void ignoresWhitespaceOnlyEdit();
    void canSaveNeedsFirstName();
    void saveEmitsTrimmedPayload();
    void saveWithoutFirstNameIsBlocked();
    void markSavedClearsDirty();
    void invalidUsernameIsNotRequested();
    void emptyUsernameStaysIdle();
    void validUsernameIsCanonicalised();
    void availableAndTakenStates();
    void lateAnswerIsIgnored();
    void editingUsernameDropsAnswer();
    void decorationKeepsAnswer();
    void repeatedCheckSendsOnce();
    void presenceTexts();
    void presenceAcceptsTdlibNames();
    void presenceReportsOnlyLabelChanges();
    void unknownPresenceClearsLine();
};

void ProfileFacadeTest::startsEmpty()
{
    ProfileFacade facade;

    QCOMPARE(facade.firstName(), QString());
    QCOMPARE(facade.lastName(), QString());
    QCOMPARE(facade.username(), QString());
    QCOMPARE(facade.bio(), QString());
    QVERIFY(!facade.dirty());
    QVERIFY(!facade.canSave());
    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));
    QCOMPARE(facade.statusText(), QString());
}

void ProfileFacadeTest::loadsProfile()
{
    ProfileFacade facade;
    loadAda(facade);

    QCOMPARE(facade.firstName(), QStringLiteral("Ada"));
    QCOMPARE(facade.lastName(), QStringLiteral("Lovelace"));
    QCOMPARE(facade.username(), QStringLiteral("ada_lovelace"));
    QCOMPARE(facade.bio(), QStringLiteral("Coffee and compilers"));
    QVERIFY(!facade.dirty());
    QVERIFY(facade.canSave());
    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));
}

void ProfileFacadeTest::loadDropsStaleUsernameAnswer()
{
    ProfileFacade facade;
    loadAda(facade);

    facade.checkUsername();
    QCOMPARE(facade.usernameStatus(), QStringLiteral("checking"));
    facade.setUsernameAvailable(true);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));

    facade.load(QStringLiteral("Grace"), QStringLiteral("Hopper"),
                QStringLiteral("grace_hopper"), QString());
    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));
    QVERIFY(!facade.dirty());
}

void ProfileFacadeTest::storesRawEdits()
{
    ProfileFacade facade;
    loadAda(facade);

    QSignalSpy spy(&facade, &ProfileFacade::changed);

    facade.setFirstName(QStringLiteral("Grace"));
    facade.setLastName(QStringLiteral("Hopper"));
    facade.setUsername(QStringLiteral("grace_hopper"));
    facade.setBio(QStringLiteral("Navy"));

    QCOMPARE(facade.firstName(), QStringLiteral("Grace"));
    QCOMPARE(facade.lastName(), QStringLiteral("Hopper"));
    QCOMPARE(facade.username(), QStringLiteral("grace_hopper"));
    QCOMPARE(facade.bio(), QStringLiteral("Navy"));
    QCOMPARE(spy.count(), 4);
}

void ProfileFacadeTest::repeatedValueIsSilent()
{
    ProfileFacade facade;
    loadAda(facade);

    QSignalSpy spy(&facade, &ProfileFacade::changed);

    facade.setFirstName(QStringLiteral("Ada"));
    facade.setLastName(QStringLiteral("Lovelace"));
    facade.setUsername(QStringLiteral("ada_lovelace"));
    facade.setBio(QStringLiteral("Coffee and compilers"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.dirty());
}

void ProfileFacadeTest::becomesDirtyOnEdit()
{
    ProfileFacade facade;
    loadAda(facade);

    QSignalSpy spy(&facade, &ProfileFacade::changed);
    facade.setFirstName(QStringLiteral("Grace"));

    QVERIFY(facade.dirty());
    QCOMPARE(spy.count(), 1);
}

void ProfileFacadeTest::ignoresWhitespaceOnlyEdit()
{
    ProfileFacade facade;
    loadAda(facade);

    facade.setFirstName(QStringLiteral(" Ada "));
    QCOMPARE(facade.firstName(), QStringLiteral(" Ada "));
    QVERIFY(!facade.dirty());

    facade.setUsername(QStringLiteral("@ada_lovelace"));
    QVERIFY(!facade.dirty());

    facade.setBio(QStringLiteral("Coffee   and compilers"));
    QVERIFY(!facade.dirty());
}

void ProfileFacadeTest::canSaveNeedsFirstName()
{
    ProfileFacade facade;
    loadAda(facade);
    QVERIFY(facade.canSave());

    facade.setFirstName(QString());
    QVERIFY(!facade.canSave());
    QVERIFY(facade.dirty());

    facade.setFirstName(QStringLiteral("   "));
    QVERIFY(!facade.canSave());
}

void ProfileFacadeTest::saveEmitsTrimmedPayload()
{
    ProfileFacade facade;
    loadAda(facade);

    facade.setFirstName(QStringLiteral("  Grace "));
    facade.setLastName(QStringLiteral(" Hopper "));
    facade.setUsername(QStringLiteral(" grace_hopper "));
    facade.setBio(QStringLiteral(" COBOL "));

    QSignalSpy spy(&facade, &ProfileFacade::saveRequested);
    QVERIFY(facade.save());

    QCOMPARE(spy.count(), 1);
    const QVariantList arguments = spy.takeFirst();
    QCOMPARE(arguments.size(), 4);
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("Grace"));
    QCOMPARE(arguments.at(1).toString(), QStringLiteral("Hopper"));
    QCOMPARE(arguments.at(2).toString(), QStringLiteral("grace_hopper"));
    QCOMPARE(arguments.at(3).toString(), QStringLiteral("COBOL"));

    // save() only announces; the draft is still dirty until the caller
    // confirms the request succeeded.
    QVERIFY(facade.dirty());
    QCOMPARE(facade.firstName(), QStringLiteral("  Grace "));
}

void ProfileFacadeTest::saveWithoutFirstNameIsBlocked()
{
    ProfileFacade facade;
    loadAda(facade);
    facade.setFirstName(QStringLiteral(" "));

    QSignalSpy spy(&facade, &ProfileFacade::saveRequested);
    QVERIFY(!facade.save());
    QCOMPARE(spy.count(), 0);
}

void ProfileFacadeTest::markSavedClearsDirty()
{
    ProfileFacade facade;
    loadAda(facade);
    facade.setFirstName(QStringLiteral("Grace"));

    QSignalSpy spy(&facade, &ProfileFacade::changed);
    facade.markSaved();

    QVERIFY(!facade.dirty());
    QCOMPARE(facade.firstName(), QStringLiteral("Grace"));
    QCOMPARE(spy.count(), 1);

    // Saving again when nothing is dirty is not an observable change.
    spy.clear();
    facade.markSaved();
    QCOMPARE(spy.count(), 0);
}

void ProfileFacadeTest::invalidUsernameIsNotRequested()
{
    ProfileFacade facade;
    loadAda(facade);

    QSignalSpy spy(&facade, &ProfileFacade::usernameCheckRequested);

    facade.setUsername(QStringLiteral("ab"));
    facade.checkUsername();
    QCOMPARE(facade.usernameStatus(), QStringLiteral("invalid"));

    facade.setUsername(QStringLiteral("1ada_lovelace"));
    facade.checkUsername();
    QCOMPARE(facade.usernameStatus(), QStringLiteral("invalid"));

    facade.setUsername(QStringLiteral("ada lovelace"));
    facade.checkUsername();
    QCOMPARE(facade.usernameStatus(), QStringLiteral("invalid"));

    QCOMPARE(spy.count(), 0);
}

void ProfileFacadeTest::emptyUsernameStaysIdle()
{
    ProfileFacade facade;
    facade.load(QStringLiteral("Ada"), QString(), QString(), QString());

    QSignalSpy spy(&facade, &ProfileFacade::usernameCheckRequested);
    facade.checkUsername();

    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));
    QCOMPARE(spy.count(), 0);
}

void ProfileFacadeTest::validUsernameIsCanonicalised()
{
    ProfileFacade facade;
    loadAda(facade);
    facade.setUsername(QStringLiteral(" @Ada_Lovelace "));

    QSignalSpy spy(&facade, &ProfileFacade::usernameCheckRequested);
    facade.checkUsername();

    QCOMPARE(facade.usernameStatus(), QStringLiteral("checking"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(firstOf(spy.takeFirst()), QStringLiteral("ada_lovelace"));
}

void ProfileFacadeTest::availableAndTakenStates()
{
    ProfileFacade facade;
    loadAda(facade);

    facade.checkUsername();
    facade.setUsernameAvailable(true);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));

    // An answer only applies to the check that was pending.
    facade.setUsernameAvailable(false);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));

    facade.setUsername(QStringLiteral("grace_hopper"));
    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));

    facade.checkUsername();
    facade.setUsernameAvailable(false);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("taken"));
}

void ProfileFacadeTest::lateAnswerIsIgnored()
{
    ProfileFacade facade;
    loadAda(facade);

    facade.checkUsername();
    facade.setUsername(QStringLiteral("grace_hopper"));
    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));

    facade.setUsernameAvailable(true);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));
}

void ProfileFacadeTest::editingUsernameDropsAnswer()
{
    ProfileFacade facade;
    loadAda(facade);

    facade.checkUsername();
    facade.setUsernameAvailable(true);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));

    facade.setUsername(QStringLiteral("grace_hopper"));
    QCOMPARE(facade.usernameStatus(), QStringLiteral("idle"));

    facade.checkUsername();
    QCOMPARE(facade.usernameStatus(), QStringLiteral("checking"));
    facade.setUsernameAvailable(true);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));
}

void ProfileFacadeTest::decorationKeepsAnswer()
{
    ProfileFacade facade;
    loadAda(facade);

    facade.checkUsername();
    facade.setUsernameAvailable(true);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));

    // Same username, different spelling: the answer still belongs to it.
    facade.setUsername(QStringLiteral(" @ADA_LOVELACE "));
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));
}

void ProfileFacadeTest::repeatedCheckSendsOnce()
{
    ProfileFacade facade;
    loadAda(facade);

    QSignalSpy spy(&facade, &ProfileFacade::usernameCheckRequested);

    facade.checkUsername();
    facade.checkUsername();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("checking"));

    facade.setUsernameAvailable(true);
    facade.checkUsername();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.usernameStatus(), QStringLiteral("available"));
}

void ProfileFacadeTest::presenceTexts()
{
    ProfileFacade facade;

    facade.setPresence(QStringLiteral("online"), 0);
    QCOMPARE(facade.statusText(), QStringLiteral("online"));

    facade.setPresence(QStringLiteral("recently"), 0);
    QCOMPARE(facade.statusText(), QStringLiteral("last seen recently"));

    facade.setPresence(QStringLiteral("offline"), 1700000000);
    QCOMPARE(facade.statusText(), QStringLiteral("offline"));

    facade.setPresence(QStringLiteral("hidden"), 0);
    QCOMPARE(facade.statusText(), QString());

    facade.setPresence(QStringLiteral("unknown"), 0);
    QCOMPARE(facade.statusText(), QString());
}

void ProfileFacadeTest::presenceAcceptsTdlibNames()
{
    ProfileFacade facade;

    facade.setPresence(QStringLiteral("userStatusOnline"), 0);
    QCOMPARE(facade.statusText(), QStringLiteral("online"));

    facade.setPresence(QStringLiteral("userStatusLastWeek"), 0);
    QCOMPARE(facade.statusText(), QStringLiteral("last seen within a week"));

    facade.setPresence(QStringLiteral("userStatusLastMonth"), 0);
    QCOMPARE(facade.statusText(), QStringLiteral("last seen within a month"));

    facade.setPresence(QStringLiteral("userStatusEmpty"), 0);
    QCOMPARE(facade.statusText(), QString());
}

void ProfileFacadeTest::presenceReportsOnlyLabelChanges()
{
    ProfileFacade facade;

    QSignalSpy spy(&facade, &ProfileFacade::changed);

    facade.setPresence(QStringLiteral("offline"), 100);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.statusText(), QStringLiteral("offline"));

    // The timestamp does not reach the label, so this is not a change.
    facade.setPresence(QStringLiteral("offline"), 200);
    QCOMPARE(spy.count(), 1);

    // Unknown and Hidden both have nothing to show.
    facade.setPresence(QStringLiteral("hidden"), 0);
    QCOMPARE(spy.count(), 2);
    facade.setPresence(QStringLiteral("unknown"), 0);
    QCOMPARE(spy.count(), 2);
}

void ProfileFacadeTest::unknownPresenceClearsLine()
{
    ProfileFacade facade;

    facade.setPresence(QStringLiteral("online"), 0);
    QCOMPARE(facade.statusText(), QStringLiteral("online"));

    QSignalSpy spy(&facade, &ProfileFacade::changed);
    facade.setPresence(QStringLiteral("bogus"), 0);

    QCOMPARE(facade.statusText(), QString());
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(ProfileFacadeTest)
#include "tst_profile_facade.moc"
