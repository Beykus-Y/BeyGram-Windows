#include "telegram/AlbumSend.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

AlbumItem photoItem(const QString& path, const QString& caption = QString(),
                    int width = 0, int height = 0)
{
    AlbumItem item;
    item.path = path;
    item.caption = caption;
    item.width = width;
    item.height = height;
    item.video = false;
    return item;
}

AlbumItem videoItem(const QString& path, const QString& caption = QString(),
                    int width = 0, int height = 0)
{
    AlbumItem item;
    item.path = path;
    item.caption = caption;
    item.width = width;
    item.height = height;
    item.video = true;
    return item;
}

QJsonArray contentsOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_message_contents")).toArray();
}

QString typeOf(const QJsonObject& content)
{
    return content.value(QStringLiteral("@type")).toString();
}

/// Reads the local path out of either inputMessagePhoto or inputMessageVideo,
/// following the same inputPhoto/inputVideo wrapper the send builder uses.
QString pathOf(const QJsonObject& content)
{
    const QString key = typeOf(content) == QLatin1String("inputMessageVideo")
        ? QStringLiteral("video")
        : QStringLiteral("photo");
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

class AlbumSendTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsAlbumOfExpectedSize_data();
    void buildsAlbumOfExpectedSize();
    void keepsItemOrderAndLocalFiles();
    void mixesPhotosAndVideos();
    void keepsDimensions();
    void putsCaptionOnFirstItemOnly();
    void addsReplyTarget();
    void omitsReplyWithoutTarget();
    void rejectsEmptyAlbum();
    void rejectsBlankPath();
    void rejectsMoreThanTenItems();
};

void AlbumSendTest::buildsAlbumOfExpectedSize_data()
{
    QTest::addColumn<int>("itemCount");

    QTest::newRow("2 items") << 2;
    QTest::newRow("3 items") << 3;
    QTest::newRow("5 items") << 5;
}

void AlbumSendTest::buildsAlbumOfExpectedSize()
{
    QFETCH(int, itemCount);

    QVector<AlbumItem> items;
    for (int i = 0; i < itemCount; ++i)
        items.append(photoItem(QStringLiteral("C:/album/photo%1.jpg").arg(i)));

    const QJsonObject request = sendAlbumRequest(42, items);
    QVERIFY(!request.isEmpty());
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessageAlbum"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("options")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_to")).isNull());
    QCOMPARE(contentsOf(request).size(), itemCount);
}

void AlbumSendTest::keepsItemOrderAndLocalFiles()
{
    const QVector<AlbumItem> items{
        photoItem(QStringLiteral("C:/album/first.jpg")),
        photoItem(QStringLiteral("C:/album/second.png")),
        photoItem(QStringLiteral("C:/album/third.webp")),
    };

    const QJsonObject request = sendAlbumRequest(7, items);
    const QJsonArray contents = contentsOf(request);
    QCOMPARE(contents.size(), 3);

    const QStringList expected{
        QStringLiteral("C:/album/first.jpg"),
        QStringLiteral("C:/album/second.png"),
        QStringLiteral("C:/album/third.webp"),
    };
    for (int i = 0; i < expected.size(); ++i) {
        const QJsonObject content = contents.at(i).toObject();
        QCOMPARE(typeOf(content), QStringLiteral("inputMessagePhoto"));
        QCOMPARE(pathOf(content), expected.at(i));

        const QJsonObject file = content.value(QStringLiteral("photo"))
                                     .toObject()
                                     .value(QStringLiteral("photo"))
                                     .toObject();
        QCOMPARE(file.value(QStringLiteral("@type")).toString(),
                 QStringLiteral("inputFileLocal"));
        QCOMPARE(file.value(QStringLiteral("path")).toString(), expected.at(i));
    }
}

void AlbumSendTest::mixesPhotosAndVideos()
{
    const QVector<AlbumItem> items{
        photoItem(QStringLiteral("C:/album/a.jpg")),
        videoItem(QStringLiteral("C:/album/b.mp4")),
        photoItem(QStringLiteral("C:/album/c.png")),
        videoItem(QStringLiteral("C:/album/d.mov")),
    };

    const QJsonObject request = sendAlbumRequest(7, items);
    const QJsonArray contents = contentsOf(request);
    QCOMPARE(contents.size(), 4);

    const QStringList expectedTypes{
        QStringLiteral("inputMessagePhoto"),
        QStringLiteral("inputMessageVideo"),
        QStringLiteral("inputMessagePhoto"),
        QStringLiteral("inputMessageVideo"),
    };
    for (int i = 0; i < expectedTypes.size(); ++i)
        QCOMPARE(typeOf(contents.at(i).toObject()), expectedTypes.at(i));

    const QJsonObject video = contents.at(1).toObject().value(QStringLiteral("video")).toObject();
    QCOMPARE(video.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputVideo"));
    QVERIFY(video.value(QStringLiteral("thumbnail")).isNull());
    QVERIFY(video.value(QStringLiteral("cover")).isNull());
    QCOMPARE(video.value(QStringLiteral("start_timestamp")).toInt(), 0);
    QCOMPARE(video.value(QStringLiteral("duration")).toInt(), 0);
    QVERIFY(!video.value(QStringLiteral("supports_streaming")).toBool());
    QCOMPARE(video.value(QStringLiteral("added_sticker_file_ids")).toArray().size(), 0);

    const QJsonObject photo = contents.at(0).toObject().value(QStringLiteral("photo")).toObject();
    QCOMPARE(photo.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputPhoto"));
    QVERIFY(photo.value(QStringLiteral("thumbnail")).isNull());
    QVERIFY(photo.value(QStringLiteral("video")).isNull());
    QCOMPARE(photo.value(QStringLiteral("added_sticker_file_ids")).toArray().size(), 0);
}

void AlbumSendTest::keepsDimensions()
{
    const QVector<AlbumItem> items{
        photoItem(QStringLiteral("C:/album/a.jpg"), QString(), 1920, 1080),
        videoItem(QStringLiteral("C:/album/b.mp4"), QString(), 1280, 720),
        photoItem(QStringLiteral("C:/album/c.jpg"), QString(), -4, -8),
    };

    const QJsonObject request = sendAlbumRequest(7, items);
    const QJsonArray contents = contentsOf(request);
    QCOMPARE(contents.size(), 3);

    const QJsonObject first = contents.at(0).toObject()
                                  .value(QStringLiteral("photo")).toObject();
    QCOMPARE(first.value(QStringLiteral("width")).toInt(), 1920);
    QCOMPARE(first.value(QStringLiteral("height")).toInt(), 1080);

    const QJsonObject second = contents.at(1).toObject()
                                   .value(QStringLiteral("video")).toObject();
    QCOMPARE(second.value(QStringLiteral("width")).toInt(), 1280);
    QCOMPARE(second.value(QStringLiteral("height")).toInt(), 720);

    // Unknown dimensions stay 0 instead of leaking a negative value into TDLib.
    const QJsonObject third = contents.at(2).toObject()
                                  .value(QStringLiteral("photo")).toObject();
    QCOMPARE(third.value(QStringLiteral("width")).toInt(), 0);
    QCOMPARE(third.value(QStringLiteral("height")).toInt(), 0);
}

void AlbumSendTest::putsCaptionOnFirstItemOnly()
{
    const QVector<AlbumItem> items{
        photoItem(QStringLiteral("C:/album/a.jpg"), QStringLiteral("  album caption  ")),
        videoItem(QStringLiteral("C:/album/b.mp4"), QStringLiteral("ignored video caption")),
        photoItem(QStringLiteral("C:/album/c.jpg"), QStringLiteral("ignored third caption")),
    };

    const QJsonObject request = sendAlbumRequest(7, items);
    const QJsonArray contents = contentsOf(request);
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

void AlbumSendTest::addsReplyTarget()
{
    const QVector<AlbumItem> items{
        photoItem(QStringLiteral("C:/album/a.jpg")),
        videoItem(QStringLiteral("C:/album/b.mp4")),
    };

    const QJsonObject request = sendAlbumRequest(42, items, 777);
    const QJsonObject reply = request.value(QStringLiteral("reply_to")).toObject();
    QCOMPARE(reply.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageReplyToMessage"));
    QCOMPARE(reply.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(777));
    QVERIFY(reply.value(QStringLiteral("quote")).isNull());
}

void AlbumSendTest::omitsReplyWithoutTarget()
{
    const QVector<AlbumItem> items{
        photoItem(QStringLiteral("C:/album/a.jpg")),
        photoItem(QStringLiteral("C:/album/b.jpg")),
    };

    QVERIFY(sendAlbumRequest(42, items).value(QStringLiteral("reply_to")).isNull());
    QVERIFY(sendAlbumRequest(42, items, 0).value(QStringLiteral("reply_to")).isNull());
    QVERIFY(sendAlbumRequest(42, items, -5).value(QStringLiteral("reply_to")).isNull());
}

void AlbumSendTest::rejectsEmptyAlbum()
{
    const QJsonObject request = sendAlbumRequest(42, QVector<AlbumItem>{});
    QVERIFY(request.isEmpty());
}

void AlbumSendTest::rejectsBlankPath()
{
    const QVector<AlbumItem> items{
        photoItem(QStringLiteral("C:/album/a.jpg")),
        photoItem(QStringLiteral("   ")),
    };
    QVERIFY(sendAlbumRequest(42, items).isEmpty());

    const QVector<AlbumItem> emptyPath{
        videoItem(QString()),
        videoItem(QStringLiteral("C:/album/b.mp4")),
    };
    QVERIFY(sendAlbumRequest(42, emptyPath).isEmpty());
}

void AlbumSendTest::rejectsMoreThanTenItems()
{
    QVector<AlbumItem> items;
    for (int i = 0; i < 11; ++i)
        items.append(photoItem(QStringLiteral("C:/album/photo%1.jpg").arg(i)));

    QVERIFY(sendAlbumRequest(42, items).isEmpty());

    items.removeLast();
    QCOMPARE(contentsOf(sendAlbumRequest(42, items)).size(), 10);
}

QTEST_GUILESS_MAIN(AlbumSendTest)
#include "tst_album_send.moc"
