#include "telegram/ProfileEditController.h"

#include <QJsonObject>
#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using beygram::telegram::ProfileEditController;

namespace {

QString typeOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("@type")).toString();
}

QString firstNameOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("first_name")).toString();
}

QString lastNameOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("last_name")).toString();
}

QString bioOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("bio")).toString();
}

QString usernameOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("username")).toString();
}

/// Loads a profile as TDLib would report it for the current user.
void loadProfile(ProfileEditController& controller)
{
    controller.setOriginal(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                           QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
}

} // namespace

class ProfileEditControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsClean();
    void loadedProfileIsClean();
    void matchingDraftIsClean();
    void emptyProfileStaysClean();
    void detectsFirstNameEdit();
    void detectsLastNameEdit();
    void sendsNameOnlyOnce();
    void sendsOnlyChangedFields();
    void ignoresWhitespaceOnlyEdits();
    void ignoresUsernameDecoration();
    void trimsRequestValues();
    void allowsEmptyBio();
    void allowsEmptyUsername();
    void sendsAllChangedFieldsInOrder();
    void markSavedClearsDirty();
    void markSavedKeepsDraftForFurtherEdits();
    void markSavedIsSilentWhenClean();
    void setDraftSignalsRealEditsOnly();
    void reloadReplacesDraft();
    void reloadSignalsWhenPendingEditDropped();
    void requestsAreRepeatable();
    void requestsCarryNoExtra();
};

void ProfileEditControllerTest::startsClean()
{
    ProfileEditController controller;
    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
}

void ProfileEditControllerTest::loadedProfileIsClean()
{
    // Loading a profile without touching the form must not look edited, and a
    // profile that was merely read may not produce a request.
    ProfileEditController controller;
    QSignalSpy spy(&controller, &ProfileEditController::changed);
    loadProfile(controller);

    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
    QCOMPARE(spy.count(), 1);
}

void ProfileEditControllerTest::matchingDraftIsClean()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));

    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
}

void ProfileEditControllerTest::emptyProfileStaysClean()
{
    ProfileEditController controller;
    controller.setOriginal(QString(), QString(), QString(), QString());
    QVERIFY(!controller.dirty());

    controller.setDraft(QString(), QString(), QString(), QString());
    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
}

void ProfileEditControllerTest::detectsFirstNameEdit()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));

    QVERIFY(controller.dirty());
    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 1);
    QCOMPARE(typeOf(requests.first()), QStringLiteral("setName"));
    QCOMPARE(firstNameOf(requests.first()), QStringLiteral("Grace"));
}

void ProfileEditControllerTest::detectsLastNameEdit()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Byron"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));

    QVERIFY(controller.dirty());
    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 1);
    QCOMPARE(typeOf(requests.first()), QStringLiteral("setName"));
    QCOMPARE(lastNameOf(requests.first()), QStringLiteral("Byron"));
}

void ProfileEditControllerTest::sendsNameOnlyOnce()
{
    // setName carries both parts, so editing the first and the last name is one
    // request and never two.
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Hopper"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 1);
    QCOMPARE(typeOf(requests.first()), QStringLiteral("setName"));
    QCOMPARE(firstNameOf(requests.first()), QStringLiteral("Grace"));
    QCOMPARE(lastNameOf(requests.first()), QStringLiteral("Hopper"));
}

void ProfileEditControllerTest::sendsOnlyChangedFields()
{
    ProfileEditController bioOnly;
    loadProfile(bioOnly);
    bioOnly.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                     QStringLiteral("ada_lovelace"), QStringLiteral("Coffee"));
    const QVector<QJsonObject> bioRequests = bioOnly.requests();
    QCOMPARE(bioRequests.size(), 1);
    QCOMPARE(typeOf(bioRequests.first()), QStringLiteral("setBio"));
    QCOMPARE(bioOf(bioRequests.first()), QStringLiteral("Coffee"));

    ProfileEditController usernameOnly;
    usernameOnly.setOriginal(QStringLiteral("Ada"), QStringLiteral("Lovelace"), QString(),
                             QStringLiteral("Coffee and compilers"));
    usernameOnly.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                          QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    const QVector<QJsonObject> usernameRequests = usernameOnly.requests();
    QCOMPARE(usernameRequests.size(), 1);
    QCOMPARE(typeOf(usernameRequests.first()), QStringLiteral("setUsername"));
    QCOMPARE(usernameOf(usernameRequests.first()), QStringLiteral("ada_lovelace"));
}

void ProfileEditControllerTest::ignoresWhitespaceOnlyEdits()
{
    // The builders trim the names, fold the bio and drop whitespace from the
    // username, so none of this is an edit worth a request.
    ProfileEditController controller;
    loadProfile(controller);
    QSignalSpy spy(&controller, &ProfileEditController::changed);
    controller.setDraft(QStringLiteral("  Ada  "), QStringLiteral("\tLovelace \n"),
                        QStringLiteral("  ada_lovelace  "),
                        QStringLiteral("  Coffee   and\ncompilers "));

    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void ProfileEditControllerTest::ignoresUsernameDecoration()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                        QStringLiteral(" @ada_lovelace "),
                        QStringLiteral("Coffee and compilers"));

    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
}

void ProfileEditControllerTest::trimsRequestValues()
{
    // The draft keeps what the user typed; the payload is the normalised text.
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("  Grace "), QStringLiteral(" Hopper\t"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("  Navy\nblue  "));

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 2);
    QCOMPARE(typeOf(requests.at(0)), QStringLiteral("setName"));
    QCOMPARE(firstNameOf(requests.at(0)), QStringLiteral("Grace"));
    QCOMPARE(lastNameOf(requests.at(0)), QStringLiteral("Hopper"));
    QCOMPARE(typeOf(requests.at(1)), QStringLiteral("setBio"));
    QCOMPARE(bioOf(requests.at(1)), QStringLiteral("Navy blue"));
}

void ProfileEditControllerTest::allowsEmptyBio()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QString());

    QVERIFY(controller.dirty());
    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 1);
    QCOMPARE(typeOf(requests.first()), QStringLiteral("setBio"));
    QVERIFY(requests.first().contains(QStringLiteral("bio")));
    QCOMPARE(bioOf(requests.first()), QString());
}

void ProfileEditControllerTest::allowsEmptyUsername()
{
    // "@" on its own normalises to nothing, which is how Telegram removes a
    // username, so it is a real change worth sending.
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                        QStringLiteral(" @ "), QStringLiteral("Coffee and compilers"));

    QVERIFY(controller.dirty());
    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 1);
    QCOMPARE(typeOf(requests.first()), QStringLiteral("setUsername"));
    QVERIFY(requests.first().contains(QStringLiteral("username")));
    QCOMPARE(usernameOf(requests.first()), QString());
}

void ProfileEditControllerTest::sendsAllChangedFieldsInOrder()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Hopper"),
                        QStringLiteral("@grace"), QStringLiteral("Navy"));

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 3);
    QCOMPARE(typeOf(requests.at(0)), QStringLiteral("setName"));
    QCOMPARE(typeOf(requests.at(1)), QStringLiteral("setBio"));
    QCOMPARE(typeOf(requests.at(2)), QStringLiteral("setUsername"));
    QCOMPARE(bioOf(requests.at(1)), QStringLiteral("Navy"));
    QCOMPARE(usernameOf(requests.at(2)), QStringLiteral("grace"));
}

void ProfileEditControllerTest::markSavedClearsDirty()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Hopper"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    QVERIFY(controller.dirty());

    controller.markSaved();

    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
}

void ProfileEditControllerTest::markSavedKeepsDraftForFurtherEdits()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    controller.markSaved();

    // The form still shows "Grace", and the next edit is measured against the
    // values that were just saved, not the ones loaded at the beginning.
    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    QVERIFY(controller.dirty());
    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 1);
    QCOMPARE(typeOf(requests.first()), QStringLiteral("setName"));
    QCOMPARE(firstNameOf(requests.first()), QStringLiteral("Ada"));
}

void ProfileEditControllerTest::markSavedIsSilentWhenClean()
{
    ProfileEditController controller;
    loadProfile(controller);
    QSignalSpy spy(&controller, &ProfileEditController::changed);

    controller.markSaved();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!controller.dirty());
}

void ProfileEditControllerTest::setDraftSignalsRealEditsOnly()
{
    ProfileEditController controller;
    loadProfile(controller);
    QSignalSpy spy(&controller, &ProfileEditController::changed);

    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    QCOMPARE(spy.count(), 0);

    controller.setDraft(QStringLiteral(" Ada "), QStringLiteral("Lovelace "),
                        QStringLiteral(" @ada_lovelace "),
                        QStringLiteral("Coffee\nand  compilers"));
    QCOMPARE(spy.count(), 0);

    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    QCOMPARE(spy.count(), 1);

    controller.setDraft(QStringLiteral("Ada"), QStringLiteral("Lovelace"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.dirty());
}

void ProfileEditControllerTest::reloadReplacesDraft()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Hopper"), QString(),
                        QStringLiteral("Navy"));
    QVERIFY(controller.dirty());

    // A reloaded profile replaces the form instead of merging with the edits.
    controller.setOriginal(QStringLiteral("Grace"), QStringLiteral("Hopper"), QString(),
                           QStringLiteral("Navy"));

    QVERIFY(!controller.dirty());
    QVERIFY(controller.requests().isEmpty());
}

void ProfileEditControllerTest::reloadSignalsWhenPendingEditDropped()
{
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Hopper"),
                        QStringLiteral("ada_lovelace"), QStringLiteral("Coffee and compilers"));
    QVERIFY(controller.dirty());

    QSignalSpy spy(&controller, &ProfileEditController::changed);
    loadProfile(controller);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!controller.dirty());
}

void ProfileEditControllerTest::requestsAreRepeatable()
{
    // requests() is a query: asking twice must not consume or mutate the edit.
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Lovelace"),
                        QStringLiteral("grace"), QStringLiteral("Coffee and compilers"));

    const QVector<QJsonObject> first = controller.requests();
    const QVector<QJsonObject> second = controller.requests();
    QCOMPARE(first.size(), 2);
    QCOMPARE(second.size(), 2);
    QCOMPARE(typeOf(first.at(0)), typeOf(second.at(0)));
    QCOMPARE(typeOf(first.at(1)), typeOf(second.at(1)));
    QVERIFY(controller.dirty());

    controller.markSaved();
    QVERIFY(controller.requests().isEmpty());
}

void ProfileEditControllerTest::requestsCarryNoExtra()
{
    // The transport adds @extra, and photo edits belong to the media layer, so
    // nothing here may mention either of them.
    ProfileEditController controller;
    loadProfile(controller);
    controller.setDraft(QStringLiteral("Grace"), QStringLiteral("Hopper"),
                        QStringLiteral("@grace"), QStringLiteral("Navy"));

    const QVector<QJsonObject> requests = controller.requests();
    QCOMPARE(requests.size(), 3);
    for (const QJsonObject& request : requests) {
        const QString type = typeOf(request);
        QVERIFY(type == QLatin1String("setName") || type == QLatin1String("setBio")
                || type == QLatin1String("setUsername"));
        QVERIFY(!request.contains(QStringLiteral("@extra")));
    }
}

QTEST_GUILESS_MAIN(ProfileEditControllerTest)
#include "tst_profile_edit_controller.moc"
