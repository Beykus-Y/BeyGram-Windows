#include "telegram/ProfileEdit.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QString typeOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("@type")).toString();
}

QStringList expectedKeys(const QStringList& keys)
{
    QStringList sorted = keys;
    sorted.sort();
    return sorted;
}

} // namespace

class ProfileEditTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsSetNameRequest();
    void trimsNamesAndAllowsEmptyLastName();
    void passesEmptyFirstNameThrough();
    void buildsSetBioRequest();
    void foldsBioWhitespace();
    void keepsBioAsPlainText();
    void buildsSetUsernameRequest();
    void stripsAtSignAndSpaces();
    void clearsUsernameWithEmptyValue();
    void buildsDeleteProfilePhotoRequest();
    void carriesLargePhotoId();
    void requestsCarryNoExtra();
};

void ProfileEditTest::buildsSetNameRequest()
{
    const QJsonObject request =
        setNameRequest(QStringLiteral("Ada"), QStringLiteral("Lovelace"));
    QCOMPARE(typeOf(request), QStringLiteral("setName"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("first_name"),
                           QStringLiteral("last_name")}));
    QCOMPARE(request.size(), 3);
    QCOMPARE(request.value(QStringLiteral("first_name")).toString(),
             QStringLiteral("Ada"));
    QCOMPARE(request.value(QStringLiteral("last_name")).toString(),
             QStringLiteral("Lovelace"));
}

void ProfileEditTest::trimsNamesAndAllowsEmptyLastName()
{
    const QJsonObject trimmed =
        setNameRequest(QStringLiteral("  Ada  "), QStringLiteral("\tLovelace \n"));
    QCOMPARE(trimmed.value(QStringLiteral("first_name")).toString(),
             QStringLiteral("Ada"));
    QCOMPARE(trimmed.value(QStringLiteral("last_name")).toString(),
             QStringLiteral("Lovelace"));

    const QJsonObject noLastName =
        setNameRequest(QStringLiteral("Ada"), QStringLiteral("   "));
    QCOMPARE(noLastName.value(QStringLiteral("first_name")).toString(),
             QStringLiteral("Ada"));
    QCOMPARE(noLastName.value(QStringLiteral("last_name")).toString(), QString());

    const QJsonObject defaultLastName =
        setNameRequest(QStringLiteral("Ada"), QString());
    QCOMPARE(defaultLastName.value(QStringLiteral("last_name")).toString(), QString());
}

void ProfileEditTest::passesEmptyFirstNameThrough()
{
    // TDLib requires 1-64 characters in the first name and answers
    // FIRSTNAME_INVALID for an empty one. The builder does not invent a name and
    // does not drop the field, so the error surfaces to the caller as TDLib
    // reported it.
    const QJsonObject request =
        setNameRequest(QStringLiteral("   "), QStringLiteral("Lovelace"));
    QCOMPARE(typeOf(request), QStringLiteral("setName"));
    QVERIFY(request.contains(QStringLiteral("first_name")));
    QCOMPARE(request.value(QStringLiteral("first_name")).toString(), QString());
    QCOMPARE(request.value(QStringLiteral("last_name")).toString(),
             QStringLiteral("Lovelace"));
}

void ProfileEditTest::buildsSetBioRequest()
{
    const QJsonObject request = setBioRequest(QStringLiteral("Coffee and compilers"));
    QCOMPARE(typeOf(request), QStringLiteral("setBio"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("bio")}));
    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("bio")).toString(),
             QStringLiteral("Coffee and compilers"));

    const QJsonObject cleared = setBioRequest(QString());
    QCOMPARE(typeOf(cleared), QStringLiteral("setBio"));
    QCOMPARE(cleared.value(QStringLiteral("bio")).toString(), QString());
}

void ProfileEditTest::foldsBioWhitespace()
{
    const QJsonObject request = setBioRequest(
        QStringLiteral("  Line one\nLine two\r\n\r\nLine three\t "));
    QCOMPARE(request.value(QStringLiteral("bio")).toString(),
             QStringLiteral("Line one Line two Line three"));

    const QJsonObject onlyWhitespace = setBioRequest(QStringLiteral(" \n\t\r "));
    QCOMPARE(onlyWhitespace.value(QStringLiteral("bio")).toString(), QString());
}

void ProfileEditTest::keepsBioAsPlainText()
{
    // No HTML handling and no formattedText: a bio that looks like markup is
    // stored exactly as typed, and '<' is not escaped.
    const QJsonObject request = setBioRequest(QStringLiteral("<b>bold</b> & <3"));
    const QJsonValue bio = request.value(QStringLiteral("bio"));
    QVERIFY(bio.isString());
    QCOMPARE(bio.toString(), QStringLiteral("<b>bold</b> & <3"));
    QVERIFY(!bio.isObject());
}

void ProfileEditTest::buildsSetUsernameRequest()
{
    const QJsonObject request = setUsernameRequest(QStringLiteral("@ada_lovelace"));
    QCOMPARE(typeOf(request), QStringLiteral("setUsername"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("username")}));
    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("username")).toString(),
             QStringLiteral("ada_lovelace"));
}

void ProfileEditTest::stripsAtSignAndSpaces()
{
    const QJsonObject padded =
        setUsernameRequest(QStringLiteral("  @ada lovelace  "));
    QCOMPARE(padded.value(QStringLiteral("username")).toString(),
             QStringLiteral("adalovelace"));

    const QJsonObject repeatedAt = setUsernameRequest(QStringLiteral("@@ada"));
    QCOMPARE(repeatedAt.value(QStringLiteral("username")).toString(),
             QStringLiteral("ada"));

    const QJsonObject plain = setUsernameRequest(QStringLiteral("ada"));
    QCOMPARE(plain.value(QStringLiteral("username")).toString(),
             QStringLiteral("ada"));

    // Too short for Telegram, but validation belongs to TDLib; only the '@' and
    // the surrounding whitespace are removed here.
    const QJsonObject shortName = setUsernameRequest(QStringLiteral(" @a "));
    QCOMPARE(shortName.value(QStringLiteral("username")).toString(),
             QStringLiteral("a"));
}

void ProfileEditTest::clearsUsernameWithEmptyValue()
{
    const QJsonObject empty = setUsernameRequest(QString());
    QCOMPARE(typeOf(empty), QStringLiteral("setUsername"));
    QCOMPARE(empty.value(QStringLiteral("username")).toString(), QString());

    const QJsonObject spaces = setUsernameRequest(QStringLiteral("   "));
    QCOMPARE(spaces.value(QStringLiteral("username")).toString(), QString());

    const QJsonObject bareAt = setUsernameRequest(QStringLiteral(" @ "));
    QCOMPARE(bareAt.value(QStringLiteral("username")).toString(), QString());
}

void ProfileEditTest::buildsDeleteProfilePhotoRequest()
{
    const QJsonObject request = deleteProfilePhotoRequest(987654321);
    QCOMPARE(typeOf(request), QStringLiteral("deleteProfilePhoto"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("profile_photo_id")}));
    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("profile_photo_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(987654321));
}

void ProfileEditTest::carriesLargePhotoId()
{
    const qint64 largeId = qint64(1) << 40;
    const QJsonObject request = deleteProfilePhotoRequest(largeId);
    QCOMPARE(request.value(QStringLiteral("profile_photo_id"))
                 .toVariant()
                 .toLongLong(),
             largeId);

    const QJsonObject zero = deleteProfilePhotoRequest(0);
    QCOMPARE(zero.value(QStringLiteral("profile_photo_id")).toVariant().toLongLong(),
             qint64(0));
}

void ProfileEditTest::requestsCarryNoExtra()
{
    const QList<QJsonObject> requests{
        setNameRequest(QStringLiteral("Ada"), QStringLiteral("Lovelace")),
        setBioRequest(QStringLiteral("Coffee")),
        setUsernameRequest(QStringLiteral("@ada")),
        deleteProfilePhotoRequest(42),
    };
    for (const QJsonObject& request : requests) {
        QVERIFY(!request.value(QStringLiteral("@type")).toString().isEmpty());
        QVERIFY(!request.contains(QStringLiteral("@extra")));
    }
}

QTEST_GUILESS_MAIN(ProfileEditTest)
#include "tst_profile_edit.moc"
