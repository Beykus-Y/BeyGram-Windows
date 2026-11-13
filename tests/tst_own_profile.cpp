#include "telegram/OwnProfile.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject jsonObject(const QByteArray& json)
{
    return QJsonDocument::fromJson(json).object();
}

QJsonObject ownUserFixture()
{
    return jsonObject(R"JSON({
        "@type": "user",
        "id": 777000111,
        "first_name": "Ada",
        "last_name": "Lovelace",
        "usernames": {
            "@type": "usernames",
            "editable_username": "ada",
            "active_usernames": ["ada_alt"],
            "disabled_usernames": []
        },
        "phone_number": "+15550100",
        "is_premium": true,
        "is_verified": true
    })JSON");
}

QJsonObject ownFullInfoFixture()
{
    return jsonObject(R"JSON({
        "@type": "userFullInfo",
        "bio": {
            "@type": "formattedText",
            "text": "Mathematician",
            "entities": []
        },
        "profile_photo": {
            "@type": "profilePhoto",
            "id": 900,
            "small": {"@type": "file", "id": 4242},
            "big": {"@type": "file", "id": 4343}
        }
    })JSON");
}

} // namespace

class OwnProfileTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesOwnProfile();
    void prefersEditableUsername();
    void readsBioVariants();
    void missingFullInfoKeepsDefaults();
    void resolvesProfilePhoto();
    void rejectsInvalidPhotoId();
    void buildsDisplayName();
    void fallsBackForEmptyUser();
    void buildsOwnProfileRequests();
};

void OwnProfileTest::parsesOwnProfile()
{
    const OwnProfileState state = ownProfileFromUser(ownUserFixture(), ownFullInfoFixture());

    QCOMPARE(state.userId, qint64(777000111));
    QCOMPARE(state.firstName, QStringLiteral("Ada"));
    QCOMPARE(state.lastName, QStringLiteral("Lovelace"));
    QCOMPARE(state.username, QStringLiteral("ada"));
    QCOMPARE(state.phoneNumber, QStringLiteral("+15550100"));
    QCOMPARE(state.bio, QStringLiteral("Mathematician"));
    QCOMPARE(state.photoFileId, 4242);
    QVERIFY(state.isPremium);
    QVERIFY(state.isVerified);
}

void OwnProfileTest::prefersEditableUsername()
{
    // editable_username wins over the active list.
    QCOMPARE(ownProfileFromUser(ownUserFixture()).username, QStringLiteral("ada"));

    // Without an editable name the first active username is used.
    const QJsonObject activeOnly = jsonObject(R"JSON({
        "@type": "user",
        "id": 1,
        "usernames": {"@type": "usernames", "active_usernames": ["@trinity", "@neo"]}
    })JSON");
    QCOMPARE(ownProfileFromUser(activeOnly).username, QStringLiteral("trinity"));

    // A blank first entry is skipped rather than ending the search.
    const QJsonObject blankFirst = jsonObject(R"JSON({
        "@type": "user",
        "id": 2,
        "usernames": {"@type": "usernames", "active_usernames": ["", "neo"]}
    })JSON");
    QCOMPARE(ownProfileFromUser(blankFirst).username, QStringLiteral("neo"));

    // Older payloads may only carry the singular top-level field.
    const QJsonObject legacy = jsonObject(R"JSON({
        "@type": "user",
        "id": 3,
        "username": "@legacy"
    })JSON");
    QCOMPARE(ownProfileFromUser(legacy).username, QStringLiteral("legacy"));

    const QJsonObject none = jsonObject(R"JSON({"@type": "user", "id": 4})JSON");
    QVERIFY(ownProfileFromUser(none).username.isEmpty());
}

void OwnProfileTest::readsBioVariants()
{
    // A plain string bio is accepted as well as TDLib's formattedText object.
    const QJsonObject plain = jsonObject(R"JSON({
        "@type": "userFullInfo",
        "bio": "  Coffee and Qt  "
    })JSON");
    QCOMPARE(ownProfileFromUser(ownUserFixture(), plain).bio, QStringLiteral("Coffee and Qt"));

    // Entities do not leak markup into the plain-text bio.
    const QJsonObject formatted = jsonObject(R"JSON({
        "@type": "userFullInfo",
        "bio": {
            "@type": "formattedText",
            "text": "Ada & Charles",
            "entities": [
                {"@type": "textEntity", "offset": 0, "length": 3,
                 "type": {"@type": "textEntityTypeBold"}}
            ]
        }
    })JSON");
    QCOMPARE(ownProfileFromUser(ownUserFixture(), formatted).bio,
             QStringLiteral("Ada & Charles"));

    // Missing, null or text-less bios stay empty.
    QVERIFY(ownProfileFromUser(ownUserFixture(), jsonObject(R"JSON({})JSON")).bio.isEmpty());
    QVERIFY(ownProfileFromUser(ownUserFixture(),
                               jsonObject(R"JSON({"@type": "userFullInfo", "bio": null})JSON"))
                .bio.isEmpty());
    QVERIFY(ownProfileFromUser(ownUserFixture(),
                               jsonObject(R"JSON({
                                   "@type": "userFullInfo",
                                   "bio": {"@type": "formattedText", "entities": []}
                               })JSON"))
                .bio.isEmpty());
}

void OwnProfileTest::missingFullInfoKeepsDefaults()
{
    const OwnProfileState state = ownProfileFromUser(ownUserFixture());

    QCOMPARE(state.userId, qint64(777000111));
    QCOMPARE(state.firstName, QStringLiteral("Ada"));
    QCOMPARE(state.lastName, QStringLiteral("Lovelace"));
    QCOMPARE(state.username, QStringLiteral("ada"));
    QCOMPARE(state.phoneNumber, QStringLiteral("+15550100"));
    QVERIFY(state.isPremium);
    QVERIFY(state.isVerified);
    // No full info and no user photo: bio and photo keep their defaults.
    QVERIFY(state.bio.isEmpty());
    QCOMPARE(state.photoFileId, 0);

    // An empty object behaves the same as an omitted answer.
    const OwnProfileState emptyFull = ownProfileFromUser(ownUserFixture(), QJsonObject{});
    QVERIFY(emptyFull.bio.isEmpty());
    QCOMPARE(emptyFull.photoFileId, 0);
}

void OwnProfileTest::resolvesProfilePhoto()
{
    // getMe carries the profile photo, so a user payload is enough on its own.
    QJsonObject userWithPhoto = ownUserFixture();
    userWithPhoto.insert(QStringLiteral("profile_photo"),
                         jsonObject(R"JSON({
                             "@type": "profilePhoto",
                             "id": 901,
                             "small": {"@type": "file", "id": 5151},
                             "big": {"@type": "file", "id": 5252}
                         })JSON"));
    QCOMPARE(ownProfileFromUser(userWithPhoto).photoFileId, 5151);

    // The user copy wins when both payloads carry a photo.
    QJsonObject refreshed = ownFullInfoFixture();
    QJsonObject photo = refreshed.value(QStringLiteral("profile_photo")).toObject();
    QJsonObject small = photo.value(QStringLiteral("small")).toObject();
    small.insert(QStringLiteral("id"), 6161);
    photo.insert(QStringLiteral("small"), small);
    refreshed.insert(QStringLiteral("profile_photo"), photo);
    QCOMPARE(ownProfileFromUser(userWithPhoto, refreshed).photoFileId, 5151);

    // Without a user photo the full-info copy is still usable.
    QCOMPARE(ownProfileFromUser(ownUserFixture(), refreshed).photoFileId, 6161);

    // The small file is the avatar source; a big-only photo is not used.
    const QJsonObject bigOnly = jsonObject(R"JSON({
        "@type": "userFullInfo",
        "profile_photo": {
            "@type": "profilePhoto",
            "id": 902,
            "big": {"@type": "file", "id": 6262}
        }
    })JSON");
    QCOMPARE(ownProfileFromUser(ownUserFixture(), bigOnly).photoFileId, 0);
}

void OwnProfileTest::rejectsInvalidPhotoId()
{
    const auto photoIdFor = [](const char* smallJson) {
        const QJsonObject fullInfo = jsonObject(
            QByteArrayLiteral("{\"@type\":\"userFullInfo\",\"profile_photo\":{\"small\":")
            + QByteArray(smallJson) + QByteArrayLiteral("}}"));
        return ownProfileFromUser(ownUserFixture(), fullInfo).photoFileId;
    };

    QCOMPARE(photoIdFor(R"({"@type":"file","id":0})"), 0);
    QCOMPARE(photoIdFor(R"({"@type":"file","id":-5})"), 0);
    QCOMPARE(photoIdFor(R"({"@type":"file"})"), 0);
    // TDLib file ids are int32; anything larger cannot be represented.
    QCOMPARE(photoIdFor(R"({"@type":"file","id":4294967296})"), 0);
}

void OwnProfileTest::buildsDisplayName()
{
    OwnProfileState state;
    state.firstName = QStringLiteral("Ada");
    state.lastName = QStringLiteral("Lovelace");
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("Ada Lovelace"));

    state.firstName = QStringLiteral("  Ada  ");
    state.lastName = QStringLiteral("  Lovelace ");
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("Ada Lovelace"));

    state.lastName.clear();
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("Ada"));

    state.firstName.clear();
    state.lastName = QStringLiteral("Lovelace");
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("Lovelace"));

    // Whitespace-only names count as missing and fall through to username.
    state.firstName = QStringLiteral("   ");
    state.lastName = QStringLiteral(" ");
    state.username = QStringLiteral("ada");
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("ada"));

    state.username.clear();
    state.phoneNumber = QStringLiteral("+15550100");
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("+15550100"));

    state.phoneNumber.clear();
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("You"));

    // A name always wins over the other identifiers.
    state.firstName = QStringLiteral("Ada");
    state.lastName = QStringLiteral("Lovelace");
    state.username = QStringLiteral("ada");
    state.phoneNumber = QStringLiteral("+15550100");
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("Ada Lovelace"));
}

void OwnProfileTest::fallsBackForEmptyUser()
{
    const OwnProfileState state = ownProfileFromUser(jsonObject(R"JSON({})JSON"));

    QCOMPARE(state.userId, qint64(0));
    QVERIFY(state.firstName.isEmpty());
    QVERIFY(state.lastName.isEmpty());
    QVERIFY(state.username.isEmpty());
    QVERIFY(state.phoneNumber.isEmpty());
    QVERIFY(state.bio.isEmpty());
    QCOMPARE(state.photoFileId, 0);
    QVERIFY(!state.isPremium);
    QVERIFY(!state.isVerified);
    QCOMPARE(ownProfileDisplayName(state), QStringLiteral("You"));

    const QJsonObject regular = jsonObject(R"JSON({
        "@type": "user",
        "id": 5,
        "first_name": "Grace",
        "is_premium": false,
        "is_verified": false
    })JSON");
    const OwnProfileState regularState = ownProfileFromUser(regular);
    QVERIFY(!regularState.isPremium);
    QVERIFY(!regularState.isVerified);
    QCOMPARE(ownProfileDisplayName(regularState), QStringLiteral("Grace"));
}

void OwnProfileTest::buildsOwnProfileRequests()
{
    const QJsonObject me = getOwnUserRequest();
    QCOMPARE(me.value(QStringLiteral("@type")).toString(), QStringLiteral("getMe"));
    QCOMPARE(me.keys().size(), qsizetype(1));

    // The id is filled in by the caller from the getMe answer; the builder can
    // only emit the placeholder.
    const QJsonObject fullInfo = getOwnFullInfoRequest();
    QCOMPARE(fullInfo.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getUserFullInfo"));
    QCOMPARE(fullInfo.value(QStringLiteral("user_id")).toVariant().toLongLong(), qint64(0));
    QCOMPARE(fullInfo.keys().size(), qsizetype(2));
}

QTEST_GUILESS_MAIN(OwnProfileTest)
#include "tst_own_profile.moc"
