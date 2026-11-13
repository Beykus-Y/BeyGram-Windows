#include "telegram/AlbumSendPlanner.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

AlbumSendItem photoItem(const QString& path, const QString& caption = QString(),
                        int width = 0, int height = 0)
{
    AlbumSendItem item;
    item.localPath = path;
    item.caption = caption;
    item.video = false;
    item.width = width;
    item.height = height;
    return item;
}

AlbumSendItem videoItem(const QString& path, const QString& caption = QString(),
                        int width = 0, int height = 0, int durationSeconds = 0)
{
    AlbumSendItem item;
    item.localPath = path;
    item.caption = caption;
    item.video = true;
    item.width = width;
    item.height = height;
    item.durationSeconds = durationSeconds;
    return item;
}

AlbumSendPlan makePlan(const QVector<AlbumSendItem>& items, qint64 chatId = 42,
                       qint64 replyTo = 0)
{
    AlbumSendPlan plan;
    plan.chatId = chatId;
    plan.items = items;
    plan.replyTo = replyTo;
    return plan;
}

QVector<AlbumSendItem> repeatedPhotos(int count)
{
    QVector<AlbumSendItem> items;
    for (int i = 0; i < count; ++i)
        items.append(photoItem(QStringLiteral("C:/album/photo%1.jpg").arg(i)));
    return items;
}

QJsonArray contentsOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_message_contents")).toArray();
}

QString typeOf(const QJsonObject& content)
{
    return content.value(QStringLiteral("@type")).toString();
}

QString mediaKey(const QJsonObject& content)
{
    return typeOf(content) == QLatin1String("inputMessageVideo")
        ? QStringLiteral("video")
        : QStringLiteral("photo");
}

/// The inputPhoto/inputVideo wrapper, which holds width, height and duration.
QJsonObject mediaOf(const QJsonObject& content)
{
    return content.value(mediaKey(content)).toObject();
}

QString pathOf(const QJsonObject& content)
{
    const QString key = mediaKey(content);
    return content.value(key)
        .toObject()
        .value(key)
        .toObject()
        .value(QStringLiteral("path"))
        .toString();
}

QJsonObject captionOf(const QJsonObject& content)
{
    return content.value(QStringLiteral("caption")).toObject();
}

} // namespace

class AlbumSendPlannerTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsMinimalPlan();
    void acceptsTenItemPlan();
    void acceptsBoundaryValues();
    void acceptsNegativeChatId();
    void rejectsZeroChat();
    void rejectsTooFewItems();
    void rejectsTooManyItems();
    void rejectsEmptyOrBlankPath();
    void rejectsOverlongCaption();
    void rejectsNegativeDimensions();
    void rejectsDurationOutOfRange();
    void validationOrderMatchesContract();
    void buildsOneAlbumRequest();
    void keepsOrderPathsAndMediaTypes();
    void putsCaptionOnFirstItemOnly();
    void videoFirstKeepsCaptionAndDuration();
    void carriesReplyTargetOnlyWhenPositive();
    void keepsDimensionsAndVideoDurations();
    void returnsNoRequestsForInvalidPlan();
    void tenItemPlanStaysOneRequest();
};

void AlbumSendPlannerTest::acceptsMinimalPlan()
{
    const AlbumSendPlan plan = makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg")),
        photoItem(QStringLiteral("C:/album/b.jpg")),
    });
    QCOMPARE(validateAlbumPlan(plan), QString());
}

void AlbumSendPlannerTest::acceptsTenItemPlan()
{
    QCOMPARE(validateAlbumPlan(makePlan(repeatedPhotos(10))), QString());
}

void AlbumSendPlannerTest::acceptsBoundaryValues()
{
    const QString maxCaption(1024, QLatin1Char('x'));
    const AlbumSendPlan plan = makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg"), maxCaption, 0, 0),
        videoItem(QStringLiteral("C:/album/b.mp4"), maxCaption, 1920, 1080, 600),
    });
    QCOMPARE(validateAlbumPlan(plan), QString());
}

void AlbumSendPlannerTest::acceptsNegativeChatId()
{
    const AlbumSendPlan plan = makePlan(repeatedPhotos(2), qint64(-1001234567890));
    QCOMPARE(validateAlbumPlan(plan), QString());

    const QVector<QJsonObject> requests = albumSendRequests(plan);
    QCOMPARE(requests.size(), 1);
    QCOMPARE(requests.first().value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890));
}

void AlbumSendPlannerTest::rejectsZeroChat()
{
    QCOMPARE(validateAlbumPlan(makePlan(repeatedPhotos(2), 0)),
             QStringLiteral("no-chat"));
    // The chat is checked before the item count.
    QCOMPARE(validateAlbumPlan(makePlan(QVector<AlbumSendItem>{}, 0)),
             QStringLiteral("no-chat"));
}

void AlbumSendPlannerTest::rejectsTooFewItems()
{
    QCOMPARE(validateAlbumPlan(makePlan(QVector<AlbumSendItem>{})),
             QStringLiteral("too-few"));
    QCOMPARE(validateAlbumPlan(makePlan({photoItem(QStringLiteral("C:/album/a.jpg"))})),
             QStringLiteral("too-few"));
}

void AlbumSendPlannerTest::rejectsTooManyItems()
{
    QCOMPARE(validateAlbumPlan(makePlan(repeatedPhotos(11))),
             QStringLiteral("too-many"));
}

void AlbumSendPlannerTest::rejectsEmptyOrBlankPath()
{
    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QString()),
                 photoItem(QStringLiteral("C:/album/b.jpg")),
             })),
             QStringLiteral("empty-path"));

    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg")),
                 videoItem(QStringLiteral("   ")),
             })),
             QStringLiteral("empty-path"));

    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg")),
                 photoItem(QStringLiteral("\t\n")),
             })),
             QStringLiteral("empty-path"));
}

void AlbumSendPlannerTest::rejectsOverlongCaption()
{
    const QString tooLong(1025, QLatin1Char('x'));

    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg"), tooLong),
                 photoItem(QStringLiteral("C:/album/b.jpg")),
             })),
             QStringLiteral("caption-too-long"));

    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg")),
                 videoItem(QStringLiteral("C:/album/b.mp4"), tooLong),
             })),
             QStringLiteral("caption-too-long"));
}

void AlbumSendPlannerTest::rejectsNegativeDimensions()
{
    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg"), QString(), -1, 0),
                 photoItem(QStringLiteral("C:/album/b.jpg")),
             })),
             QStringLiteral("bad-size"));

    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg")),
                 videoItem(QStringLiteral("C:/album/b.mp4"), QString(), 0, -1),
             })),
             QStringLiteral("bad-size"));

    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg"), QString(), -8, -9),
                 photoItem(QStringLiteral("C:/album/b.jpg")),
             })),
             QStringLiteral("bad-size"));
}

void AlbumSendPlannerTest::rejectsDurationOutOfRange()
{
    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg")),
                 videoItem(QStringLiteral("C:/album/b.mp4"), QString(), 0, 0, -1),
             })),
             QStringLiteral("bad-duration"));

    QCOMPARE(validateAlbumPlan(makePlan({
                 photoItem(QStringLiteral("C:/album/a.jpg")),
                 videoItem(QStringLiteral("C:/album/b.mp4"), QString(), 0, 0, 601),
             })),
             QStringLiteral("bad-duration"));

    // The range applies to every item, even a photo whose duration is ignored.
    AlbumSendPlan photoWithDuration = makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg")),
        photoItem(QStringLiteral("C:/album/b.jpg")),
    });
    photoWithDuration.items[1].durationSeconds = 601;
    QCOMPARE(validateAlbumPlan(photoWithDuration), QStringLiteral("bad-duration"));
}

void AlbumSendPlannerTest::validationOrderMatchesContract()
{
    // The chat is checked before the item count.
    QCOMPARE(validateAlbumPlan(makePlan(QVector<AlbumSendItem>{}, 0)),
             QStringLiteral("no-chat"));

    // The count is checked before any item's contents.
    QCOMPARE(validateAlbumPlan(makePlan({photoItem(QString())})),
             QStringLiteral("too-few"));

    QVector<AlbumSendItem> tooMany = repeatedPhotos(11);
    tooMany[0].localPath.clear();
    QCOMPARE(validateAlbumPlan(makePlan(tooMany)), QStringLiteral("too-many"));

    // Within one item, the documented reason order wins.
    const QString tooLong(1025, QLatin1Char('x'));

    AlbumSendItem broken = photoItem(QString(), tooLong, -1, -1);
    broken.durationSeconds = 601;
    QCOMPARE(validateAlbumPlan(makePlan({broken, photoItem(QStringLiteral("C:/album/b.jpg"))})),
             QStringLiteral("empty-path"));

    AlbumSendItem longCaption = photoItem(QStringLiteral("C:/album/a.jpg"), tooLong, -1, -1);
    longCaption.durationSeconds = 601;
    QCOMPARE(validateAlbumPlan(makePlan({longCaption, photoItem(QStringLiteral("C:/album/b.jpg"))})),
             QStringLiteral("caption-too-long"));

    AlbumSendItem badSize = photoItem(QStringLiteral("C:/album/a.jpg"), QString(), -1, 0);
    badSize.durationSeconds = 601;
    QCOMPARE(validateAlbumPlan(makePlan({badSize, photoItem(QStringLiteral("C:/album/b.jpg"))})),
             QStringLiteral("bad-size"));
}

void AlbumSendPlannerTest::buildsOneAlbumRequest()
{
    const AlbumSendPlan plan = makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg")),
        photoItem(QStringLiteral("C:/album/b.png")),
    });

    const QVector<QJsonObject> requests = albumSendRequests(plan);
    QCOMPARE(requests.size(), 1);

    const QJsonObject request = requests.first();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessageAlbum"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("options")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_to")).isNull());
    QCOMPARE(contentsOf(request).size(), 2);
}

void AlbumSendPlannerTest::keepsOrderPathsAndMediaTypes()
{
    const AlbumSendPlan plan = makePlan({
        photoItem(QStringLiteral("C:/album/first.jpg")),
        videoItem(QStringLiteral("C:/album/second.mp4")),
        photoItem(QStringLiteral("C:/album/third.webp")),
        videoItem(QStringLiteral("C:/album/fourth.mov")),
    });

    const QVector<QJsonObject> requests = albumSendRequests(plan);
    QCOMPARE(requests.size(), 1);
    const QJsonArray contents = contentsOf(requests.first());
    QCOMPARE(contents.size(), 4);

    const QStringList expectedPaths{
        QStringLiteral("C:/album/first.jpg"),
        QStringLiteral("C:/album/second.mp4"),
        QStringLiteral("C:/album/third.webp"),
        QStringLiteral("C:/album/fourth.mov"),
    };
    const QStringList expectedTypes{
        QStringLiteral("inputMessagePhoto"),
        QStringLiteral("inputMessageVideo"),
        QStringLiteral("inputMessagePhoto"),
        QStringLiteral("inputMessageVideo"),
    };

    for (int i = 0; i < expectedPaths.size(); ++i) {
        const QJsonObject content = contents.at(i).toObject();
        QCOMPARE(typeOf(content), expectedTypes.at(i));
        QCOMPARE(pathOf(content), expectedPaths.at(i));

        const QString key = mediaKey(content);
        const QJsonObject wrapper = content.value(key).toObject();
        QCOMPARE(wrapper.value(QStringLiteral("@type")).toString(),
                 key == QLatin1String("video") ? QStringLiteral("inputVideo")
                                               : QStringLiteral("inputPhoto"));

        const QJsonObject file = wrapper.value(key).toObject();
        QCOMPARE(file.value(QStringLiteral("@type")).toString(),
                 QStringLiteral("inputFileLocal"));
        QCOMPARE(file.value(QStringLiteral("path")).toString(), expectedPaths.at(i));
    }
}

void AlbumSendPlannerTest::putsCaptionOnFirstItemOnly()
{
    const AlbumSendPlan plan = makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg"),
                  QStringLiteral("  album caption  ")),
        videoItem(QStringLiteral("C:/album/b.mp4"),
                  QStringLiteral("ignored video caption")),
        photoItem(QStringLiteral("C:/album/c.jpg"),
                  QStringLiteral("ignored third caption")),
    });

    const QVector<QJsonObject> requests = albumSendRequests(plan);
    QCOMPARE(requests.size(), 1);
    const QJsonArray contents = contentsOf(requests.first());
    QCOMPARE(contents.size(), 3);

    const QJsonObject firstCaption = captionOf(contents.at(0).toObject());
    QCOMPARE(firstCaption.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
    QCOMPARE(firstCaption.value(QStringLiteral("text")).toString(),
             QStringLiteral("  album caption  "));
    QCOMPARE(firstCaption.value(QStringLiteral("entities")).toArray().size(), 0);

    for (int i = 1; i < contents.size(); ++i) {
        const QJsonObject caption = captionOf(contents.at(i).toObject());
        QCOMPARE(caption.value(QStringLiteral("@type")).toString(),
                 QStringLiteral("formattedText"));
        QCOMPARE(caption.value(QStringLiteral("text")).toString(), QString());
        QCOMPARE(caption.value(QStringLiteral("entities")).toArray().size(), 0);
    }
}

void AlbumSendPlannerTest::videoFirstKeepsCaptionAndDuration()
{
    const AlbumSendPlan plan = makePlan({
        videoItem(QStringLiteral("C:/album/first.mp4"),
                  QStringLiteral("video caption"), 1920, 1080, 12),
        photoItem(QStringLiteral("C:/album/second.jpg")),
    });

    const QVector<QJsonObject> requests = albumSendRequests(plan);
    QCOMPARE(requests.size(), 1);
    const QJsonArray contents = contentsOf(requests.first());
    QCOMPARE(contents.size(), 2);

    const QJsonObject first = contents.at(0).toObject();
    QCOMPARE(typeOf(first), QStringLiteral("inputMessageVideo"));
    QCOMPARE(captionOf(first).value(QStringLiteral("text")).toString(),
             QStringLiteral("video caption"));

    const QJsonObject video = mediaOf(first);
    QCOMPARE(video.value(QStringLiteral("width")).toInt(), 1920);
    QCOMPARE(video.value(QStringLiteral("height")).toInt(), 1080);
    QCOMPARE(video.value(QStringLiteral("duration")).toInt(), 12);

    QCOMPARE(captionOf(contents.at(1).toObject()).value(QStringLiteral("text")).toString(),
             QString());
}

void AlbumSendPlannerTest::carriesReplyTargetOnlyWhenPositive()
{
    const QVector<AlbumSendItem> items{
        photoItem(QStringLiteral("C:/album/a.jpg")),
        videoItem(QStringLiteral("C:/album/b.mp4")),
    };

    const QVector<QJsonObject> replied = albumSendRequests(makePlan(items, 42, 777));
    QCOMPARE(replied.size(), 1);
    const QJsonObject reply = replied.first().value(QStringLiteral("reply_to")).toObject();
    QCOMPARE(reply.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageReplyToMessage"));
    QCOMPARE(reply.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(777));
    QVERIFY(reply.value(QStringLiteral("quote")).isNull());

    const QVector<QJsonObject> notReplied = albumSendRequests(makePlan(items, 42, 0));
    QCOMPARE(notReplied.size(), 1);
    QVERIFY(notReplied.first().value(QStringLiteral("reply_to")).isNull());

    const QVector<QJsonObject> negativeReply = albumSendRequests(makePlan(items, 42, -5));
    QCOMPARE(negativeReply.size(), 1);
    QVERIFY(negativeReply.first().value(QStringLiteral("reply_to")).isNull());
}

void AlbumSendPlannerTest::keepsDimensionsAndVideoDurations()
{
    const AlbumSendPlan plan = makePlan({
        photoItem(QStringLiteral("C:/album/photo.jpg"), QString(), 1920, 1080),
        videoItem(QStringLiteral("C:/album/clip.mp4"), QString(), 1280, 720, 37),
        videoItem(QStringLiteral("C:/album/long.mp4"), QString(), 640, 480, 600),
        videoItem(QStringLiteral("C:/album/unknown.mp4"), QString(), 320, 240, 0),
        photoItem(QStringLiteral("C:/album/small.jpg"), QString(), 800, 600),
    });

    const QVector<QJsonObject> requests = albumSendRequests(plan);
    QCOMPARE(requests.size(), 1);
    const QJsonArray contents = contentsOf(requests.first());
    QCOMPARE(contents.size(), 5);

    const QJsonObject photo = mediaOf(contents.at(0).toObject());
    QCOMPARE(photo.value(QStringLiteral("width")).toInt(), 1920);
    QCOMPARE(photo.value(QStringLiteral("height")).toInt(), 1080);
    QVERIFY(!photo.contains(QStringLiteral("duration")));

    const QJsonObject clip = mediaOf(contents.at(1).toObject());
    QCOMPARE(clip.value(QStringLiteral("width")).toInt(), 1280);
    QCOMPARE(clip.value(QStringLiteral("height")).toInt(), 720);
    QCOMPARE(clip.value(QStringLiteral("duration")).toInt(), 37);

    const QJsonObject longClip = mediaOf(contents.at(2).toObject());
    QCOMPARE(longClip.value(QStringLiteral("duration")).toInt(), 600);

    const QJsonObject unknownDuration = mediaOf(contents.at(3).toObject());
    QCOMPARE(unknownDuration.value(QStringLiteral("duration")).toInt(), 0);

    const QJsonObject small = mediaOf(contents.at(4).toObject());
    QCOMPARE(small.value(QStringLiteral("width")).toInt(), 800);
    QCOMPARE(small.value(QStringLiteral("height")).toInt(), 600);
}

void AlbumSendPlannerTest::returnsNoRequestsForInvalidPlan()
{
    QVERIFY(albumSendRequests(makePlan(repeatedPhotos(2), 0)).isEmpty());
    QVERIFY(albumSendRequests(makePlan(QVector<AlbumSendItem>{})).isEmpty());
    QVERIFY(albumSendRequests(makePlan(repeatedPhotos(1))).isEmpty());
    QVERIFY(albumSendRequests(makePlan(repeatedPhotos(11))).isEmpty());

    QVERIFY(albumSendRequests(makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg")),
        photoItem(QString()),
    })).isEmpty());

    QVERIFY(albumSendRequests(makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg")),
        photoItem(QStringLiteral("C:/album/b.jpg"), QString(1025, QLatin1Char('x'))),
    })).isEmpty());

    QVERIFY(albumSendRequests(makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg"), QString(), -1, 0),
        photoItem(QStringLiteral("C:/album/b.jpg")),
    })).isEmpty());

    QVERIFY(albumSendRequests(makePlan({
        photoItem(QStringLiteral("C:/album/a.jpg")),
        videoItem(QStringLiteral("C:/album/b.mp4"), QString(), 0, 0, 601),
    })).isEmpty());
}

void AlbumSendPlannerTest::tenItemPlanStaysOneRequest()
{
    const QVector<QJsonObject> requests = albumSendRequests(makePlan(repeatedPhotos(10)));
    QCOMPARE(requests.size(), 1);
    QCOMPARE(contentsOf(requests.first()).size(), 10);
}

QTEST_GUILESS_MAIN(AlbumSendPlannerTest)
#include "tst_album_send_planner.moc"
