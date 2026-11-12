#include "telegram/ProfileMedia.h"

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

QJsonObject fileObject(qint64 id)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("file")},
        {QStringLiteral("id"), id},
    };
}

QJsonObject audioObject(qint64 fileId, const QString& title,
                        const QString& performer, int durationSeconds)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("audio")},
        {QStringLiteral("duration"), durationSeconds},
        {QStringLiteral("title"), title},
        {QStringLiteral("performer"), performer},
        {QStringLiteral("audio"), fileObject(fileId)},
    };
}

qint64 int64Of(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

} // namespace

class ProfileMediaTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesFirstProfileAudio();
    void prefersFirstProfileAudioOverAlias();
    void parsesProfileAudioAlias();
    void parsesBareAudioObject();
    void missingProfileAudioIsEmpty();
    void invalidAudioFileIdIsEmpty();
    void clampsNegativeAudioDuration();
    void preservesAudioTagsAsReported();

    void buildsSetProfileAudioRequest();
    void rejectsNonPositiveProfileAudioFileId();
    void buildsRemoveProfileAudioRequest();

    void buildsGetProfilePhotosRequest();
    void clampsProfilePhotosOffsetAndLimit();
    void rejectsNonPositiveProfilePhotosUserId();

    void buildsSetProfilePhotoRequest();
    void carriesPublicProfilePhotoFlag();
    void rejectsEmptyProfilePhotoPath();
    void keepsProfilePhotoPathUnchanged();

    void buildsDeleteProfilePhotoRequest();
    void sharedDeleteBuilderPassesIdsThrough();
    void carriesLargePhotoId();

    void requestsCarryNoExtra();
};

void ProfileMediaTest::parsesFirstProfileAudio()
{
    const QJsonObject user{
        {QStringLiteral("id"), 7},
        {QStringLiteral("first_profile_audio"),
         audioObject(4242, QStringLiteral("Song"), QStringLiteral("Artist"), 245)},
    };

    const ProfileAudioInfo info = profileAudioFromUser(user);
    QCOMPARE(info.fileId, qint64(4242));
    QCOMPARE(info.title, QStringLiteral("Song"));
    QCOMPARE(info.performer, QStringLiteral("Artist"));
    QCOMPARE(info.durationSeconds, 245);
}

void ProfileMediaTest::prefersFirstProfileAudioOverAlias()
{
    const QJsonObject user{
        {QStringLiteral("first_profile_audio"),
         audioObject(11, QStringLiteral("First"), QStringLiteral("A"), 1)},
        {QStringLiteral("profile_audio"),
         audioObject(22, QStringLiteral("Alias"), QStringLiteral("B"), 2)},
    };

    const ProfileAudioInfo info = profileAudioFromUser(user);
    QCOMPARE(info.fileId, qint64(11));
    QCOMPARE(info.title, QStringLiteral("First"));
}

void ProfileMediaTest::parsesProfileAudioAlias()
{
    const QJsonObject user{
        {QStringLiteral("profile_audio"),
         audioObject(5150, QStringLiteral("Alias"), QStringLiteral("B"), 30)},
    };

    const ProfileAudioInfo info = profileAudioFromUser(user);
    QCOMPARE(info.fileId, qint64(5150));
    QCOMPARE(info.title, QStringLiteral("Alias"));
    QCOMPARE(info.performer, QStringLiteral("B"));
    QCOMPARE(info.durationSeconds, 30);
}

void ProfileMediaTest::parsesBareAudioObject()
{
    const ProfileAudioInfo info =
        profileAudioFromUser(audioObject(99, QStringLiteral("Bare"), QStringLiteral("C"), 3));
    QCOMPARE(info.fileId, qint64(99));
    QCOMPARE(info.title, QStringLiteral("Bare"));
    QCOMPARE(info.performer, QStringLiteral("C"));
    QCOMPARE(info.durationSeconds, 3);
}

void ProfileMediaTest::missingProfileAudioIsEmpty()
{
    const QJsonObject user{
        {QStringLiteral("id"), 7},
        {QStringLiteral("first_name"), QStringLiteral("Ada")},
    };

    const ProfileAudioInfo info = profileAudioFromUser(user);
    QCOMPARE(info.fileId, qint64(0));
    QVERIFY(info.title.isEmpty());
    QVERIFY(info.performer.isEmpty());
    QCOMPARE(info.durationSeconds, 0);
}

void ProfileMediaTest::invalidAudioFileIdIsEmpty()
{
    QJsonObject user{
        {QStringLiteral("first_profile_audio"),
         audioObject(0, QStringLiteral("No file"), QStringLiteral("A"), 10)},
    };
    ProfileAudioInfo info = profileAudioFromUser(user);
    QCOMPARE(info.fileId, qint64(0));
    QVERIFY(info.title.isEmpty());

    user.insert(QStringLiteral("first_profile_audio"),
                audioObject(-8, QStringLiteral("Negative"), QStringLiteral("A"), 10));
    info = profileAudioFromUser(user);
    QCOMPARE(info.fileId, qint64(0));
    QVERIFY(info.title.isEmpty());
}

void ProfileMediaTest::clampsNegativeAudioDuration()
{
    const QJsonObject user{
        {QStringLiteral("first_profile_audio"),
         audioObject(5, QStringLiteral("Song"), QStringLiteral("Artist"), -12)},
    };

    QCOMPARE(profileAudioFromUser(user).durationSeconds, 0);
}

void ProfileMediaTest::preservesAudioTagsAsReported()
{
    // Track tags are not names or bios; the parser does not trim them.
    const QJsonObject user{
        {QStringLiteral("first_profile_audio"),
         audioObject(5, QStringLiteral("  Song  "), QStringLiteral(" Artist "), 1)},
    };

    const ProfileAudioInfo info = profileAudioFromUser(user);
    QCOMPARE(info.title, QStringLiteral("  Song  "));
    QCOMPARE(info.performer, QStringLiteral(" Artist "));
}

void ProfileMediaTest::buildsSetProfileAudioRequest()
{
    const QJsonObject request = setProfileAudioRequest(4242);
    QCOMPARE(typeOf(request), QStringLiteral("addProfileAudio"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("audio")}));
    QCOMPARE(request.size(), 2);

    const QJsonObject audio = request.value(QStringLiteral("audio")).toObject();
    QCOMPARE(typeOf(audio), QStringLiteral("inputAudio"));
    QCOMPARE(audio.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("audio"),
                           QStringLiteral("album_cover_thumbnail"),
                           QStringLiteral("duration"), QStringLiteral("title"),
                           QStringLiteral("performer")}));
    QCOMPARE(audio.size(), 6);

    const QJsonObject file = audio.value(QStringLiteral("audio")).toObject();
    QCOMPARE(typeOf(file), QStringLiteral("inputFileId"));
    QCOMPARE(int64Of(file, QStringLiteral("id")), qint64(4242));
    QVERIFY(audio.value(QStringLiteral("album_cover_thumbnail")).isNull());
    QCOMPARE(audio.value(QStringLiteral("duration")).toInt(), 0);
    QCOMPARE(audio.value(QStringLiteral("title")).toString(), QString());
    QCOMPARE(audio.value(QStringLiteral("performer")).toString(), QString());
}

void ProfileMediaTest::rejectsNonPositiveProfileAudioFileId()
{
    QVERIFY(setProfileAudioRequest(0).isEmpty());
    QVERIFY(setProfileAudioRequest(-1).isEmpty());
}

void ProfileMediaTest::buildsRemoveProfileAudioRequest()
{
    const QJsonObject request = removeProfileAudioRequest();
    QCOMPARE(typeOf(request), QStringLiteral("removeProfileAudio"));
    QCOMPARE(request.keys(), expectedKeys({QStringLiteral("@type")}));
    QCOMPARE(request.size(), 1);
    // The requested no-argument interface cannot supply TDLib's required
    // file_id; the test pins the envelope that callers receive.
    QVERIFY(!request.contains(QStringLiteral("file_id")));
}

void ProfileMediaTest::buildsGetProfilePhotosRequest()
{
    const QJsonObject request = getProfilePhotosRequest(777, 20, 50);
    QCOMPARE(typeOf(request), QStringLiteral("getUserProfilePhotos"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("user_id"),
                           QStringLiteral("offset"), QStringLiteral("limit")}));
    QCOMPARE(request.size(), 4);
    QCOMPARE(int64Of(request, QStringLiteral("user_id")), qint64(777));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 20);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);
}

void ProfileMediaTest::clampsProfilePhotosOffsetAndLimit()
{
    const QJsonObject low = getProfilePhotosRequest(1, -5, 0);
    QCOMPARE(low.value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(low.value(QStringLiteral("limit")).toInt(), 1);

    const QJsonObject high = getProfilePhotosRequest(1, 0, 101);
    QCOMPARE(high.value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(high.value(QStringLiteral("limit")).toInt(), 100);

    const QJsonObject negativeLimit = getProfilePhotosRequest(1, 0, -7);
    QCOMPARE(negativeLimit.value(QStringLiteral("limit")).toInt(), 1);
}

void ProfileMediaTest::rejectsNonPositiveProfilePhotosUserId()
{
    QVERIFY(getProfilePhotosRequest(0, 0, 50).isEmpty());
    QVERIFY(getProfilePhotosRequest(-4, 0, 50).isEmpty());
}

void ProfileMediaTest::buildsSetProfilePhotoRequest()
{
    const QJsonObject request =
        setProfilePhotoRequest(QStringLiteral("C:/photos/avatar.jpg"), false);
    QCOMPARE(typeOf(request), QStringLiteral("setProfilePhoto"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("photo"),
                           QStringLiteral("is_public")}));
    QCOMPARE(request.size(), 3);
    QCOMPARE(request.value(QStringLiteral("is_public")).toBool(), false);

    const QJsonObject chatPhoto = request.value(QStringLiteral("photo")).toObject();
    QCOMPARE(typeOf(chatPhoto), QStringLiteral("inputChatPhotoStatic"));
    QCOMPARE(chatPhoto.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("photo")}));
    QCOMPARE(chatPhoto.size(), 2);

    const QJsonObject file = chatPhoto.value(QStringLiteral("photo")).toObject();
    QCOMPARE(typeOf(file), QStringLiteral("inputFileLocal"));
    QCOMPARE(file.keys(),
             expectedKeys({QStringLiteral("@type"), QStringLiteral("path")}));
    QCOMPARE(file.value(QStringLiteral("path")).toString(),
             QStringLiteral("C:/photos/avatar.jpg"));
}

void ProfileMediaTest::carriesPublicProfilePhotoFlag()
{
    const QJsonObject request =
        setProfilePhotoRequest(QStringLiteral("/tmp/avatar.png"), true);
    QCOMPARE(request.value(QStringLiteral("is_public")).toBool(), true);
}

void ProfileMediaTest::rejectsEmptyProfilePhotoPath()
{
    QVERIFY(setProfilePhotoRequest(QString(), true).isEmpty());
    QVERIFY(setProfilePhotoRequest(QStringLiteral("   "), false).isEmpty());
    QVERIFY(setProfilePhotoRequest(QStringLiteral("\t\n"), true).isEmpty());
}

void ProfileMediaTest::keepsProfilePhotoPathUnchanged()
{
    // A filesystem path is not trimmed; only whitespace-only input is rejected.
    const QString padded = QStringLiteral("  C:/photos/avatar.jpg  ");
    const QJsonObject request = setProfilePhotoRequest(padded, false);
    const QJsonObject chatPhoto = request.value(QStringLiteral("photo")).toObject();
    const QJsonObject file = chatPhoto.value(QStringLiteral("photo")).toObject();
    QCOMPARE(file.value(QStringLiteral("path")).toString(), padded);
}

void ProfileMediaTest::buildsDeleteProfilePhotoRequest()
{
    const QJsonObject request = deleteProfilePhotoRequest(987654321);
    QCOMPARE(typeOf(request), QStringLiteral("deleteProfilePhoto"));
    QCOMPARE(request.keys(),
             expectedKeys({QStringLiteral("@type"),
                           QStringLiteral("profile_photo_id")}));
    QCOMPARE(request.size(), 2);
    QCOMPARE(int64Of(request, QStringLiteral("profile_photo_id")),
             qint64(987654321));
}

void ProfileMediaTest::sharedDeleteBuilderPassesIdsThrough()
{
    // deleteProfilePhotoRequest lives in ProfileEdit and passes the id through;
    // this module reuses it instead of defining a second copy.
    QCOMPARE(int64Of(deleteProfilePhotoRequest(0), QStringLiteral("profile_photo_id")), qint64(0));
    QCOMPARE(int64Of(deleteProfilePhotoRequest(-3), QStringLiteral("profile_photo_id")), qint64(-3));
}

void ProfileMediaTest::carriesLargePhotoId()
{
    const qint64 largeId = qint64(1) << 40;
    const QJsonObject request = deleteProfilePhotoRequest(largeId);
    QCOMPARE(int64Of(request, QStringLiteral("profile_photo_id")), largeId);
}

void ProfileMediaTest::requestsCarryNoExtra()
{
    const QList<QJsonObject> requests{
        setProfileAudioRequest(42),
        removeProfileAudioRequest(),
        getProfilePhotosRequest(7, 0, 50),
        setProfilePhotoRequest(QStringLiteral("/tmp/avatar.jpg"), false),
        deleteProfilePhotoRequest(42),
    };

    for (const QJsonObject& request : requests) {
        QVERIFY(!typeOf(request).isEmpty());
        QVERIFY(!request.contains(QStringLiteral("@extra")));
    }
}

QTEST_GUILESS_MAIN(ProfileMediaTest)
#include "tst_profile_media.moc"
