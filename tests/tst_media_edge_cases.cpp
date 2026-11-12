#include "telegram/MediaEdgeCases.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject contentFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonObject contentFromJson(const QString& json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class MediaEdgeCasesTest final : public QObject {
    Q_OBJECT

private slots:
    void mapsMediaKinds_data();
    void mapsMediaKinds();
    void acceptsMessageWrappers();
    void probesCompleteVideoNote();
    void probesLossyVideoNotes_data();
    void probesLossyVideoNotes();
    void readsBadgeState_data();
    void readsBadgeState();
    void labelsBadgeKinds_data();
    void labelsBadgeKinds();
    void decidesDownload_data();
    void decidesDownload();
    void toleratesMalformedInput();
    void roundTripsThroughJson();
};

void MediaEdgeCasesTest::mapsMediaKinds_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("kind");

    QTest::newRow("photo") << QStringLiteral("messagePhoto") << QStringLiteral("photo");
    QTest::newRow("video") << QStringLiteral("messageVideo") << QStringLiteral("video");
    QTest::newRow("video note") << QStringLiteral("messageVideoNote") << QStringLiteral("video");
    QTest::newRow("animation") << QStringLiteral("messageAnimation") << QStringLiteral("animation");
    QTest::newRow("audio") << QStringLiteral("messageAudio") << QStringLiteral("audio");
    QTest::newRow("voice") << QStringLiteral("messageVoiceNote") << QStringLiteral("voice");
    QTest::newRow("document") << QStringLiteral("messageDocument") << QStringLiteral("document");
    QTest::newRow("sticker") << QStringLiteral("messageSticker") << QStringLiteral("sticker");
    QTest::newRow("animated emoji") << QStringLiteral("messageAnimatedEmoji")
                                    << QStringLiteral("sticker");
    QTest::newRow("location") << QStringLiteral("messageLocation") << QStringLiteral("location");
    QTest::newRow("live location") << QStringLiteral("messageLiveLocation")
                                   << QStringLiteral("location");
    QTest::newRow("venue") << QStringLiteral("messageVenue") << QStringLiteral("location");
    QTest::newRow("contact") << QStringLiteral("messageContact") << QStringLiteral("contact");
    QTest::newRow("poll") << QStringLiteral("messagePoll") << QStringLiteral("poll");
    QTest::newRow("text") << QStringLiteral("messageText") << QStringLiteral("none");
    QTest::newRow("dice") << QStringLiteral("messageDice") << QStringLiteral("none");
    QTest::newRow("paid media") << QStringLiteral("messagePaidMedia") << QStringLiteral("none");
    QTest::newRow("invoice") << QStringLiteral("messageInvoice") << QStringLiteral("none");
    QTest::newRow("call") << QStringLiteral("messageCall") << QStringLiteral("none");
    QTest::newRow("expired photo") << QStringLiteral("messageExpiredPhoto")
                                   << QStringLiteral("photo");
    QTest::newRow("expired video") << QStringLiteral("messageExpiredVideo")
                                   << QStringLiteral("video");
    QTest::newRow("expired video note") << QStringLiteral("messageExpiredVideoNote")
                                        << QStringLiteral("video");
    QTest::newRow("expired voice") << QStringLiteral("messageExpiredVoiceNote")
                                   << QStringLiteral("voice");
    QTest::newRow("unknown") << QStringLiteral("messageFutureThing") << QStringLiteral("none");
    QTest::newRow("missing type") << QString() << QStringLiteral("none");
}

void MediaEdgeCasesTest::mapsMediaKinds()
{
    QFETCH(QString, type);
    QFETCH(QString, kind);

    QJsonObject content;
    if (!type.isEmpty())
        content.insert(QStringLiteral("@type"), type);

    QCOMPARE(mediaKindFromContent(content), kind);
}

void MediaEdgeCasesTest::acceptsMessageWrappers()
{
    const QJsonObject message = contentFromJson(R"({
        "@type": "message",
        "content": {
            "@type": "messagePhoto",
            "has_spoiler": true,
            "ttl": 15,
            "photo": {
                "sizes": [
                    { "width": 800, "height": 600, "photo": { "id": 41 } }
                ]
            }
        }
    })");

    QCOMPARE(mediaKindFromContent(message), QStringLiteral("photo"));

    const MediaBadge badge = mediaBadgeFromContent(message);
    QCOMPARE(badge.kind, QStringLiteral("photo"));
    QCOMPARE(badge.label, QStringLiteral("Photo"));
    QVERIFY(badge.spoiler);
    QVERIFY(!badge.expired);
    QCOMPARE(badge.ttlSeconds, qint64(15));

    QVERIFY(mediaNeedsDownload(message));
}

void MediaEdgeCasesTest::probesCompleteVideoNote()
{
    const QJsonObject message = contentFromJson(R"({
        "@type": "message",
        "content": {
            "@type": "messageVideoNote",
            "video_note": {
                "video": {
                    "id": 4242,
                    "local": {
                        "is_downloading_completed": true,
                        "path": "C:/media/note.mp4"
                    }
                },
                "duration": 12,
                "length": 240,
                "waveform": "AQIDBA=="
            }
        }
    })");

    const VideoNoteProbe probe = videoNoteProbe(message);
    QVERIFY(probe.hasVideo);
    QVERIFY(probe.hasWaveform);
    QCOMPARE(probe.durationSeconds, 12);
    QCOMPARE(probe.fileId, qint64(4242));
    QCOMPARE(probe.width, 240);
    QCOMPARE(probe.height, 240);
    QVERIFY(!probe.likelyLossy);

    // A bare content object is accepted too. width/height win over length when
    // the wrapper carries them, and an int64 id may arrive as a JSON string.
    const QJsonObject direct = contentFromJson(R"({
        "@type": "messageVideoNote",
        "video_note": {
            "video": { "id": "1234567890123" },
            "duration": 7,
            "width": 320,
            "height": 180,
            "length": 240,
            "waveform": [1, 2, 3]
        }
    })");

    const VideoNoteProbe directProbe = videoNoteProbe(direct);
    QVERIFY(directProbe.hasVideo);
    QVERIFY(directProbe.hasWaveform);
    QCOMPARE(directProbe.durationSeconds, 7);
    QCOMPARE(directProbe.fileId, qint64(1234567890123LL));
    QCOMPARE(directProbe.width, 320);
    QCOMPARE(directProbe.height, 180);
    QVERIFY(!directProbe.likelyLossy);

    // Some stored wrappers name the media file "file" instead of "video".
    const QJsonObject fallback = contentFromJson(R"({
        "@type": "messageVideoNote",
        "video_note": {
            "file": { "id": 77 },
            "duration": 3,
            "waveform": "AQ=="
        }
    })");
    const VideoNoteProbe fallbackProbe = videoNoteProbe(fallback);
    QVERIFY(fallbackProbe.hasVideo);
    QCOMPARE(fallbackProbe.fileId, qint64(77));
    QVERIFY(!fallbackProbe.likelyLossy);
}

void MediaEdgeCasesTest::probesLossyVideoNotes_data()
{
    QTest::addColumn<QString>("json");
    QTest::addColumn<bool>("hasVideo");
    QTest::addColumn<bool>("hasWaveform");
    QTest::addColumn<int>("duration");
    QTest::addColumn<bool>("lossy");

    QTest::newRow("complete")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":5},"duration":10,"waveform":"AQ==","length":120}})")
        << true << true << 10 << false;
    QTest::newRow("no waveform")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":5},"duration":10,"length":120}})")
        << true << false << 10 << true;
    QTest::newRow("empty waveform")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":5},"duration":10,"waveform":""}})")
        << true << false << 10 << true;
    QTest::newRow("blank waveform")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":5},"duration":10,"waveform":"   "}})")
        << true << false << 10 << true;
    QTest::newRow("no video")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"duration":10,"waveform":"AQ=="}})")
        << false << true << 10 << true;
    QTest::newRow("zero video id")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":0},"duration":10,"waveform":"AQ=="}})")
        << false << true << 10 << true;
    QTest::newRow("no video and no waveform")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"duration":10}})")
        << false << false << 10 << true;
    QTest::newRow("zero duration")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{}})")
        << false << false << 0 << false;
    QTest::newRow("negative duration")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"duration":-4}})")
        << false << false << 0 << false;
    QTest::newRow("missing note")
        << QStringLiteral(R"({"@type":"messageVideoNote"})")
        << false << false << 0 << false;
    QTest::newRow("not a video note")
        << QStringLiteral(R"({"@type":"messagePhoto","video_note":{"video":{"id":5},"duration":10,"waveform":"AQ=="}})")
        << false << false << 0 << false;
    QTest::newRow("waveform number")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":5},"duration":10,"waveform":17}})")
        << true << false << 10 << true;
    QTest::newRow("video as string")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":"broken","duration":10,"waveform":"AQ=="}})")
        << false << true << 10 << true;
    QTest::newRow("string duration")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":5},"duration":"9","waveform":"AQ=="}})")
        << true << true << 9 << false;
}

void MediaEdgeCasesTest::probesLossyVideoNotes()
{
    QFETCH(QString, json);
    QFETCH(bool, hasVideo);
    QFETCH(bool, hasWaveform);
    QFETCH(int, duration);
    QFETCH(bool, lossy);

    const VideoNoteProbe probe = videoNoteProbe(contentFromJson(json));
    QCOMPARE(probe.hasVideo, hasVideo);
    QCOMPARE(probe.hasWaveform, hasWaveform);
    QCOMPARE(probe.durationSeconds, duration);
    QCOMPARE(probe.likelyLossy, lossy);
}

void MediaEdgeCasesTest::readsBadgeState_data()
{
    QTest::addColumn<QString>("json");
    QTest::addColumn<QString>("kind");
    QTest::addColumn<bool>("spoiler");
    QTest::addColumn<bool>("expired");
    QTest::addColumn<qint64>("ttl");

    QTest::newRow("photo spoiler with ttl")
        << QStringLiteral(R"({"@type":"messagePhoto","has_spoiler":true,"ttl":60})")
        << QStringLiteral("photo") << true << false << qint64(60);
    QTest::newRow("secret video")
        << QStringLiteral(R"({"@type":"messageVideo","is_secret":true})")
        << QStringLiteral("video") << true << false << qint64(0);
    QTest::newRow("expired placeholder")
        << QStringLiteral(R"({"@type":"messageExpiredPhoto"})")
        << QStringLiteral("photo") << false << true << qint64(0);
    QTest::newRow("expired flag with ttl_seconds")
        << QStringLiteral(R"({"@type":"messageAudio","ttl_seconds":30,"expired":true})")
        << QStringLiteral("audio") << false << true << qint64(30);
    QTest::newRow("is_expired alias")
        << QStringLiteral(R"({"@type":"messageVoiceNote","is_expired":true})")
        << QStringLiteral("voice") << false << true << qint64(0);
    QTest::newRow("negative ttl")
        << QStringLiteral(R"({"@type":"messageDocument","ttl":-5})")
        << QStringLiteral("document") << false << false << qint64(0);
    QTest::newRow("string ttl")
        << QStringLiteral(R"({"@type":"messageAnimation","ttl":"45"})")
        << QStringLiteral("animation") << false << false << qint64(45);
    QTest::newRow("malformed flags")
        << QStringLiteral(R"({"@type":"messageSticker","has_spoiler":"yes","expired":1,"ttl":{}})")
        << QStringLiteral("sticker") << false << false << qint64(0);
    QTest::newRow("text")
        << QStringLiteral(R"({"@type":"messageText"})")
        << QStringLiteral("none") << false << false << qint64(0);
    QTest::newRow("empty content")
        << QStringLiteral(R"({})")
        << QStringLiteral("none") << false << false << qint64(0);
    QTest::newRow("message wrapper")
        << QStringLiteral(R"({"@type":"message","content":{"@type":"messageVideo","has_spoiler":true,"ttl":15}})")
        << QStringLiteral("video") << true << false << qint64(15);
}

void MediaEdgeCasesTest::readsBadgeState()
{
    QFETCH(QString, json);
    QFETCH(QString, kind);
    QFETCH(bool, spoiler);
    QFETCH(bool, expired);
    QFETCH(qint64, ttl);

    const MediaBadge badge = mediaBadgeFromContent(contentFromJson(json));
    QCOMPARE(badge.kind, kind);
    QCOMPARE(badge.spoiler, spoiler);
    QCOMPARE(badge.expired, expired);
    QCOMPARE(badge.ttlSeconds, ttl);
}

void MediaEdgeCasesTest::labelsBadgeKinds_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("label");

    QTest::newRow("photo") << QStringLiteral("messagePhoto") << QStringLiteral("Photo");
    QTest::newRow("video") << QStringLiteral("messageVideo") << QStringLiteral("Video");
    QTest::newRow("video note") << QStringLiteral("messageVideoNote") << QStringLiteral("Video");
    QTest::newRow("animation") << QStringLiteral("messageAnimation") << QStringLiteral("GIF");
    QTest::newRow("audio") << QStringLiteral("messageAudio") << QStringLiteral("Audio");
    QTest::newRow("voice") << QStringLiteral("messageVoiceNote") << QStringLiteral("Voice");
    QTest::newRow("document") << QStringLiteral("messageDocument") << QStringLiteral("Document");
    QTest::newRow("sticker") << QStringLiteral("messageSticker") << QStringLiteral("Sticker");
    QTest::newRow("location") << QStringLiteral("messageLocation") << QStringLiteral("Location");
    QTest::newRow("contact") << QStringLiteral("messageContact") << QStringLiteral("Contact");
    QTest::newRow("poll") << QStringLiteral("messagePoll") << QStringLiteral("Poll");
    QTest::newRow("text") << QStringLiteral("messageText") << QString();
}

void MediaEdgeCasesTest::labelsBadgeKinds()
{
    QFETCH(QString, type);
    QFETCH(QString, label);

    const QJsonObject content{
        {QStringLiteral("@type"), type},
    };
    QCOMPARE(mediaBadgeFromContent(content).label, label);
}

void MediaEdgeCasesTest::decidesDownload_data()
{
    QTest::addColumn<QString>("json");
    QTest::addColumn<bool>("needs");

    QTest::newRow("photo without local file")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":100,"height":100,"photo":{"id":11}},{"width":800,"height":600,"photo":{"id":12}}]}})")
        << true;
    QTest::newRow("photo with completed largest file")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":100,"height":100,"photo":{"id":11}},{"width":800,"height":600,"photo":{"id":12,"local":{"is_downloading_completed":true,"path":"C:/photo.jpg"}}}]}})")
        << false;
    QTest::newRow("photo with only a smaller file complete")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":100,"height":100,"photo":{"id":11,"local":{"is_downloading_completed":true,"path":"C:/small.jpg"}}},{"width":800,"height":600,"photo":{"id":12}}]}})")
        << true;
    QTest::newRow("photo completed without a path")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":10,"height":10,"photo":{"id":11,"local":{"is_downloading_completed":true,"path":""}}}]}})")
        << true;
    QTest::newRow("photo with partial download")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":10,"height":10,"photo":{"id":11,"local":{"is_downloading_active":true,"path":"C:/partial.jpg","downloaded_size":10}}}]}})")
        << true;
    QTest::newRow("photo without sizes")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{}})")
        << false;
    QTest::newRow("photo size without id")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":10,"height":10,"photo":{}}]}})")
        << false;
    QTest::newRow("photo with string file id")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":10,"height":10,"photo":{"id":"77"}}]}})")
        << true;
    QTest::newRow("video")
        << QStringLiteral(R"({"@type":"messageVideo","video":{"video":{"id":21,"local":{"is_downloading_completed":false}}}})")
        << true;
    QTest::newRow("video complete")
        << QStringLiteral(R"({"@type":"messageVideo","video":{"video":{"id":21,"local":{"is_downloading_completed":true,"path":"C:/clip.mp4"}}}})")
        << false;
    QTest::newRow("video note")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":22}}})")
        << true;
    QTest::newRow("video note complete")
        << QStringLiteral(R"({"@type":"messageVideoNote","video_note":{"video":{"id":22,"local":{"is_downloading_completed":true,"path":"C:/note.mp4"}}}})")
        << false;
    QTest::newRow("animation")
        << QStringLiteral(R"({"@type":"messageAnimation","animation":{"animation":{"id":23}}})")
        << true;
    QTest::newRow("audio")
        << QStringLiteral(R"({"@type":"messageAudio","audio":{"audio":{"id":24}}})")
        << true;
    QTest::newRow("voice")
        << QStringLiteral(R"({"@type":"messageVoiceNote","voice_note":{"voice":{"id":25}}})")
        << true;
    QTest::newRow("document")
        << QStringLiteral(R"({"@type":"messageDocument","document":{"document":{"id":26}}})")
        << true;
    QTest::newRow("sticker")
        << QStringLiteral(R"({"@type":"messageSticker","sticker":{"sticker":{"id":27}}})")
        << true;
    QTest::newRow("animated emoji")
        << QStringLiteral(R"({"@type":"messageAnimatedEmoji","sticker":{"sticker":{"id":28}}})")
        << true;
    QTest::newRow("location")
        << QStringLiteral(R"({"@type":"messageLocation","location":{"latitude":1.0,"longitude":2.0}})")
        << false;
    QTest::newRow("contact")
        << QStringLiteral(R"({"@type":"messageContact","contact":{"phone_number":"123"}})")
        << false;
    QTest::newRow("poll")
        << QStringLiteral(R"({"@type":"messagePoll","poll":{"question":{"text":"Q?"}}})")
        << false;
    QTest::newRow("text")
        << QStringLiteral(R"({"@type":"messageText","text":{"text":"hello"}})")
        << false;
    QTest::newRow("empty content")
        << QStringLiteral(R"({})")
        << false;
    QTest::newRow("expired placeholder")
        << QStringLiteral(R"({"@type":"messageExpiredPhoto"})")
        << false;
    QTest::newRow("dice has no primary file")
        << QStringLiteral(R"({"@type":"messageDice","initial_state":{"@type":"diceStickersRegular","sticker":{"sticker":{"id":30}}}})")
        << false;
    QTest::newRow("video object is not an object")
        << QStringLiteral(R"({"@type":"messageVideo","video":"broken"})")
        << false;
    QTest::newRow("local is not an object")
        << QStringLiteral(R"({"@type":"messagePhoto","photo":{"sizes":[{"width":10,"height":10,"photo":{"id":11,"local":"done"}}]}})")
        << true;
    QTest::newRow("message wrapper")
        << QStringLiteral(R"({"@type":"message","content":{"@type":"messageDocument","document":{"document":{"id":31}}}})")
        << true;
}

void MediaEdgeCasesTest::decidesDownload()
{
    QFETCH(QString, json);
    QFETCH(bool, needs);

    QCOMPARE(mediaNeedsDownload(contentFromJson(json)), needs);
}

void MediaEdgeCasesTest::toleratesMalformedInput()
{
    // Wrong JSON types everywhere must not crash and must leave the documented
    // defaults: no file, no waveform, no duration and no state.
    const QJsonObject malformed = contentFromJson(R"({
        "@type": "messageVideoNote",
        "video_note": {
            "video": [],
            "file": "nope",
            "duration": { "seconds": 4 },
            "waveform": 17,
            "length": "tall",
            "width": [],
            "height": null
        },
        "has_spoiler": "yes",
        "ttl": {},
        "expired": []
    })");

    const VideoNoteProbe probe = videoNoteProbe(malformed);
    QVERIFY(!probe.hasVideo);
    QVERIFY(!probe.hasWaveform);
    QCOMPARE(probe.durationSeconds, 0);
    QCOMPARE(probe.fileId, qint64(0));
    QCOMPARE(probe.width, 0);
    QCOMPARE(probe.height, 0);
    QVERIFY(!probe.likelyLossy);

    const MediaBadge badge = mediaBadgeFromContent(malformed);
    QCOMPARE(badge.kind, QStringLiteral("video"));
    QVERIFY(!badge.spoiler);
    QVERIFY(!badge.expired);
    QCOMPARE(badge.ttlSeconds, qint64(0));
    QVERIFY(!mediaNeedsDownload(malformed));

    // A missing or non-object content block is not unwrapped.
    QCOMPARE(mediaKindFromContent(QJsonObject()), QStringLiteral("none"));
    QCOMPARE(mediaKindFromContent(contentFromJson(R"({"@type":7})")), QStringLiteral("none"));
    QCOMPARE(mediaKindFromContent(contentFromJson(R"({"content":5})")), QStringLiteral("none"));
    QCOMPARE(mediaKindFromContent(contentFromJson(R"({"content":{"@type":5}})")),
             QStringLiteral("none"));
    QCOMPARE(mediaKindFromContent(contentFromJson(R"({"@type":"messagePhoto","content":null})")),
             QStringLiteral("photo"));

    QVERIFY(!mediaNeedsDownload(contentFromJson(R"({"@type":"messagePhoto","photo":{"sizes":{}}})")));
    QVERIFY(!mediaNeedsDownload(
        contentFromJson(R"({"@type":"messageDocument","document":{"document":{"id":"x"}}})")));
    QVERIFY(!mediaNeedsDownload(
        contentFromJson(R"({"@type":"messageDocument","document":{"document":{"id":0}}})")));

    const MediaBadge empty = mediaBadgeFromContent(QJsonObject());
    QCOMPARE(empty.kind, QStringLiteral("none"));
    QVERIFY(empty.label.isEmpty());
    QVERIFY(!empty.spoiler);
    QVERIFY(!empty.expired);
    QCOMPARE(empty.ttlSeconds, qint64(0));
}

void MediaEdgeCasesTest::roundTripsThroughJson()
{
    const QJsonObject note = serialized(contentFromJson(R"({
        "@type": "messageVideoNote",
        "video_note": {
            "video": {
                "id": 1234567890123,
                "local": { "is_downloading_completed": false }
            },
            "duration": 8,
            "length": 320,
            "waveform": "AQID"
        }
    })"));

    const VideoNoteProbe probe = videoNoteProbe(note);
    QVERIFY(probe.hasVideo);
    QVERIFY(probe.hasWaveform);
    QCOMPARE(probe.durationSeconds, 8);
    QCOMPARE(probe.fileId, qint64(1234567890123LL));
    QCOMPARE(probe.width, 320);
    QCOMPARE(probe.height, 320);
    QVERIFY(!probe.likelyLossy);
    QVERIFY(mediaNeedsDownload(note));

    const QJsonObject photo = serialized(contentFromJson(R"({
        "@type": "messagePhoto",
        "photo": {
            "sizes": [
                {
                    "width": 640,
                    "height": 480,
                    "photo": {
                        "id": 99,
                        "local": {
                            "is_downloading_completed": true,
                            "path": "C:/photo.jpg"
                        }
                    }
                }
            ]
        }
    })"));

    QVERIFY(!mediaNeedsDownload(photo));
}

QTEST_GUILESS_MAIN(MediaEdgeCasesTest)
#include "tst_media_edge_cases.moc"
