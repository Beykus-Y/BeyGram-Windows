#include "telegram/MediaSend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QString writeFile(const QTemporaryDir& dir, const QString& name)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return QString();
    file.write("payload");
    file.close();
    return path;
}

QJsonObject contentOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_message_content")).toObject();
}

QJsonObject innerOf(const QJsonObject& content, const QString& key)
{
    return content.value(key).toObject();
}

} // namespace

class MediaSendTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsPhotoRequest();
    void buildsVideoRequest();
    void buildsDocumentRequest();
    void keepsCaptionAndReplyTarget();
    void omitsReplyWithoutTarget();
    void validatesPhotoExtensions();
    void validatesVideoExtensions();
    void validatesDocumentExtensions();
    void rejectsMissingAndEmptyPaths();
    void requestContainsNoNetworkUrl();
};

void MediaSendTest::buildsPhotoRequest()
{
    const QString path = QStringLiteral("C:/media/holiday.JPG");
    const QJsonObject request =
        sendMediaMessageRequest(42, OutgoingMediaKind::Photo, path,
                                QStringLiteral("Sunset"));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessage"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_to")).isNull());
    QVERIFY(request.value(QStringLiteral("options")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_markup")).isNull());

    const QJsonObject content = contentOf(request);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessagePhoto"));
    QVERIFY(!content.value(QStringLiteral("show_caption_above_media")).toBool());
    QVERIFY(!content.value(QStringLiteral("has_spoiler")).toBool());
    QVERIFY(content.value(QStringLiteral("self_destruct_type")).isNull());

    const QJsonObject photo = innerOf(content, QStringLiteral("photo"));
    QCOMPARE(photo.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputPhoto"));
    QVERIFY(photo.value(QStringLiteral("thumbnail")).isNull());
    QVERIFY(photo.value(QStringLiteral("video")).isNull());
    QCOMPARE(photo.value(QStringLiteral("added_sticker_file_ids")).toArray().size(), 0);
    QCOMPARE(photo.value(QStringLiteral("width")).toInt(), 0);
    QCOMPARE(photo.value(QStringLiteral("height")).toInt(), 0);

    const QJsonObject file = innerOf(photo, QStringLiteral("photo"));
    QCOMPARE(file.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileLocal"));
    QCOMPARE(file.value(QStringLiteral("path")).toString(), path);
}

void MediaSendTest::buildsVideoRequest()
{
    const QJsonObject request =
        sendMediaMessageRequest(7, OutgoingMediaKind::Video,
                                QStringLiteral("C:/media/clip.mp4"),
                                QStringLiteral("Clip"));
    const QJsonObject content = contentOf(request);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageVideo"));
    QVERIFY(!content.value(QStringLiteral("show_caption_above_media")).toBool());
    QVERIFY(!content.value(QStringLiteral("has_spoiler")).toBool());
    QVERIFY(content.value(QStringLiteral("self_destruct_type")).isNull());

    const QJsonObject video = innerOf(content, QStringLiteral("video"));
    QCOMPARE(video.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputVideo"));
    QVERIFY(video.value(QStringLiteral("thumbnail")).isNull());
    QVERIFY(video.value(QStringLiteral("cover")).isNull());
    QCOMPARE(video.value(QStringLiteral("start_timestamp")).toInt(), 0);
    QCOMPARE(video.value(QStringLiteral("duration")).toInt(), 0);
    QCOMPARE(video.value(QStringLiteral("width")).toInt(), 0);
    QCOMPARE(video.value(QStringLiteral("height")).toInt(), 0);
    QVERIFY(!video.value(QStringLiteral("supports_streaming")).toBool());
    QCOMPARE(video.value(QStringLiteral("added_sticker_file_ids")).toArray().size(), 0);

    const QJsonObject file = innerOf(video, QStringLiteral("video"));
    QCOMPARE(file.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileLocal"));
    QCOMPARE(file.value(QStringLiteral("path")).toString(),
             QStringLiteral("C:/media/clip.mp4"));
}

void MediaSendTest::buildsDocumentRequest()
{
    const QJsonObject request =
        sendMediaMessageRequest(7, OutgoingMediaKind::Document,
                                QStringLiteral("C:/media/report.pdf"),
                                QStringLiteral("Report"));
    const QJsonObject content = contentOf(request);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageDocument"));

    const QJsonObject document = innerOf(content, QStringLiteral("document"));
    QCOMPARE(document.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputDocument"));
    QVERIFY(document.value(QStringLiteral("thumbnail")).isNull());
    QVERIFY(!document.value(QStringLiteral("disable_content_type_detection")).toBool());

    const QJsonObject file = innerOf(document, QStringLiteral("document"));
    QCOMPARE(file.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileLocal"));
    QCOMPARE(file.value(QStringLiteral("path")).toString(),
             QStringLiteral("C:/media/report.pdf"));
}

void MediaSendTest::keepsCaptionAndReplyTarget()
{
    const QJsonObject request =
        sendMediaMessageRequest(42, OutgoingMediaKind::Photo,
                                QStringLiteral("C:/media/a.png"),
                                QStringLiteral("  spaced caption  "), 777);
    const QJsonObject content = contentOf(request);
    const QJsonObject caption = innerOf(content, QStringLiteral("caption"));
    QCOMPARE(caption.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
    QCOMPARE(caption.value(QStringLiteral("text")).toString(),
             QStringLiteral("  spaced caption  "));
    QCOMPARE(caption.value(QStringLiteral("entities")).toArray().size(), 0);

    const QJsonObject reply = request.value(QStringLiteral("reply_to")).toObject();
    QCOMPARE(reply.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageReplyToMessage"));
    QCOMPARE(reply.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(777));
    QVERIFY(reply.value(QStringLiteral("quote")).isNull());

    const QJsonObject emptyCaption =
        contentOf(sendMediaMessageRequest(42, OutgoingMediaKind::Video,
                                          QStringLiteral("C:/media/a.mp4"), QString()));
    QCOMPARE(innerOf(emptyCaption, QStringLiteral("caption"))
                 .value(QStringLiteral("text"))
                 .toString(),
             QString());
    QCOMPARE(innerOf(emptyCaption, QStringLiteral("caption"))
                 .value(QStringLiteral("entities"))
                 .toArray()
                 .size(),
             0);
}

void MediaSendTest::omitsReplyWithoutTarget()
{
    const QJsonObject noReply =
        sendMediaMessageRequest(42, OutgoingMediaKind::Document,
                                QStringLiteral("C:/media/a.zip"), QStringLiteral("x"));
    QVERIFY(noReply.value(QStringLiteral("reply_to")).isNull());

    const QJsonObject negative =
        sendMediaMessageRequest(42, OutgoingMediaKind::Document,
                                QStringLiteral("C:/media/a.zip"), QStringLiteral("x"), -5);
    QVERIFY(negative.value(QStringLiteral("reply_to")).isNull());
}

void MediaSendTest::validatesPhotoExtensions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    for (const QString& name : {QStringLiteral("a.jpg"), QStringLiteral("b.jpeg"),
                                QStringLiteral("c.PNG"), QStringLiteral("d.WebP")}) {
        const QString path = writeFile(dir, name);
        QVERIFY(!path.isEmpty());
        QVERIFY2(isSupportedMediaPath(path, OutgoingMediaKind::Photo), qPrintable(name));
    }

    for (const QString& name : {QStringLiteral("a.gif"), QStringLiteral("b.heic"),
                                QStringLiteral("noext")}) {
        const QString path = writeFile(dir, name);
        QVERIFY(!path.isEmpty());
        QVERIFY2(!isSupportedMediaPath(path, OutgoingMediaKind::Photo), qPrintable(name));
    }

    QVERIFY(!isSupportedMediaPath(dir.path(), OutgoingMediaKind::Photo));
}

void MediaSendTest::validatesVideoExtensions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    for (const QString& name : {QStringLiteral("a.mp4"), QStringLiteral("b.MKV"),
                                QStringLiteral("c.mov"), QStringLiteral("d.WEBM")}) {
        const QString path = writeFile(dir, name);
        QVERIFY(!path.isEmpty());
        QVERIFY2(isSupportedMediaPath(path, OutgoingMediaKind::Video), qPrintable(name));
    }

    for (const QString& name : {QStringLiteral("a.avi"), QStringLiteral("b.m4v"),
                                QStringLiteral("noext")}) {
        const QString path = writeFile(dir, name);
        QVERIFY(!path.isEmpty());
        QVERIFY2(!isSupportedMediaPath(path, OutgoingMediaKind::Video), qPrintable(name));
    }
}

void MediaSendTest::validatesDocumentExtensions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    for (const QString& name : {QStringLiteral("a.pdf"), QStringLiteral("b.ZIP"),
                                QStringLiteral("c.txt"), QStringLiteral("d.bin"),
                                QStringLiteral("e.docx")}) {
        const QString path = writeFile(dir, name);
        QVERIFY(!path.isEmpty());
        QVERIFY2(isSupportedMediaPath(path, OutgoingMediaKind::Document), qPrintable(name));
    }

    // A document still needs some extension; an extension-less path is refused.
    const QString noExtension = writeFile(dir, QStringLiteral("noext"));
    QVERIFY(!noExtension.isEmpty());
    QVERIFY(!isSupportedMediaPath(noExtension, OutgoingMediaKind::Document));
}

void MediaSendTest::rejectsMissingAndEmptyPaths()
{
    QVERIFY(!isSupportedMediaPath(QString(), OutgoingMediaKind::Photo));
    QVERIFY(!isSupportedMediaPath(QStringLiteral(""), OutgoingMediaKind::Document));
    QVERIFY(!isSupportedMediaPath(QStringLiteral("   "), OutgoingMediaKind::Document));
    QVERIFY(!isSupportedMediaPath(QStringLiteral("C:/does/not/exist/photo.jpg"),
                                  OutgoingMediaKind::Photo));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(!isSupportedMediaPath(dir.path(), OutgoingMediaKind::Document));

    const QString path = writeFile(dir, QStringLiteral("clip.mp4"));
    QVERIFY(!path.isEmpty());
    QVERIFY(!isSupportedMediaPath(path, OutgoingMediaKind::Photo));
    QVERIFY(isSupportedMediaPath(path, OutgoingMediaKind::Video));
}

void MediaSendTest::requestContainsNoNetworkUrl()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeFile(dir, QStringLiteral("photo.png"));
    QVERIFY(!path.isEmpty());

    const QJsonObject request = sendMediaMessageRequest(
        42, OutgoingMediaKind::Photo, path, QStringLiteral("caption"));
    const QByteArray json = QJsonDocument(request).toJson(QJsonDocument::Compact);
    QVERIFY(!json.isEmpty());

    const QByteArray lower = json.toLower();
    QVERIFY(!lower.contains("http://"));
    QVERIFY(!lower.contains("https://"));
    QVERIFY(!lower.contains("ftp://"));
    QVERIFY(!lower.contains("file://"));
    QVERIFY(json.contains(QFileInfo(path).fileName().toUtf8()));

    QCOMPARE(contentOf(request)
                 .value(QStringLiteral("photo"))
                 .toObject()
                 .value(QStringLiteral("photo"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("inputFileLocal"));
}

QTEST_GUILESS_MAIN(MediaSendTest)
#include "tst_media_send.moc"
