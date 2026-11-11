#include "telegram/StickerInfo.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

class StickerInfoTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesStaticSticker();
    void parsesAnimatedSticker();
    void parsesVideoSticker();
    void parsesMaskAndCustomEmojiTypes();
    void parsesPremiumSticker();
    void ignoresNonStickerMessage();
    void readsIdsFromStrings();
    void buildsGetStickerSetRequest();
};

void StickerInfoTest::parsesStaticSticker()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 10,
        "chat_id": 42,
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 100,
                "set_id": 200,
                "emoji": "🙂",
                "width": 512,
                "height": 512,
                "format": { "@type": "stickerFormatStatic" },
                "type": { "@type": "stickerTypeRegular" },
                "full_type": { "@type": "stickerFullTypeRegular", "premium_animation": null },
                "thumbnail": { "file": { "id": 5 } },
                "sticker": { "id": 9 }
            },
            "is_premium": false
        }
    })")
                                    .object();

    const StickerInfo info = stickerFromMessage(message);
    QCOMPARE(info.stickerId, qint64(100));
    QCOMPARE(info.setId, qint64(200));
    QCOMPARE(info.emoji, QStringLiteral("🙂"));
    QCOMPARE(info.width, 512);
    QCOMPARE(info.height, 512);
    QCOMPARE(info.thumbnailFileId, 5);
    QCOMPARE(info.fullFileId, 9);
    QVERIFY(!info.isAnimated);
    QVERIFY(!info.isVideo);
    QVERIFY(!info.isMask);
    QVERIFY(!info.isPremium);
}

void StickerInfoTest::parsesAnimatedSticker()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 101,
                "set_id": 201,
                "emoji": "🎉",
                "width": 512,
                "height": 512,
                "format": { "@type": "stickerFormatAnimated" },
                "type": { "@type": "stickerTypeRegular" },
                "thumbnail": { "file": { "id": 11 } },
                "sticker": { "id": 12 }
            }
        }
    })")
                                    .object();

    const StickerInfo info = stickerFromMessage(message);
    QVERIFY(info.isAnimated);
    QVERIFY(!info.isVideo);
    QVERIFY(!info.isMask);
    QCOMPARE(info.thumbnailFileId, 11);
    QCOMPARE(info.fullFileId, 12);

    // Newer TDLib spelling for the same format.
    const QJsonObject tgs = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 102,
                "format": { "@type": "stickerFormatTgs" },
                "sticker": { "id": 13 }
            }
        }
    })")
                                .object();
    const StickerInfo tgsInfo = stickerFromMessage(tgs);
    QVERIFY(tgsInfo.isAnimated);
    QVERIFY(!tgsInfo.isVideo);
    QCOMPARE(tgsInfo.thumbnailFileId, 13);
    QCOMPARE(tgsInfo.fullFileId, 13);
}

void StickerInfoTest::parsesVideoSticker()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 103,
                "set_id": 203,
                "emoji": "🕺",
                "width": 512,
                "height": 384,
                "format": { "@type": "stickerFormatVideo" },
                "type": { "@type": "stickerTypeRegular" },
                "thumbnail": { "file": { "id": 14 } },
                "sticker": { "id": 15 }
            }
        }
    })")
                                    .object();

    const StickerInfo info = stickerFromMessage(message);
    QVERIFY(info.isVideo);
    QVERIFY(!info.isAnimated);
    QVERIFY(!info.isMask);
    QCOMPARE(info.width, 512);
    QCOMPARE(info.height, 384);
    QCOMPARE(info.thumbnailFileId, 14);
    QCOMPARE(info.fullFileId, 15);
}

void StickerInfoTest::parsesMaskAndCustomEmojiTypes()
{
    const QJsonObject mask = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 104,
                "set_id": 204,
                "emoji": "",
                "width": 512,
                "height": 512,
                "format": { "@type": "stickerFormatStatic" },
                "type": { "@type": "stickerTypeMask" },
                "sticker": { "id": 16 }
            }
        }
    })")
                                 .object();
    const StickerInfo maskInfo = stickerFromMessage(mask);
    QVERIFY(maskInfo.isMask);
    QVERIFY(!maskInfo.isAnimated);
    QVERIFY(!maskInfo.isVideo);

    // Newer TDLib carries the same information in full_type.
    const QJsonObject fullTypeMask = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 105,
                "format": { "@type": "stickerFormatWebp" },
                "full_type": { "@type": "stickerFullTypeMask", "mask_position": null },
                "sticker": { "id": 17 }
            }
        }
    })")
                                         .object();
    QVERIFY(stickerFromMessage(fullTypeMask).isMask);

    const QJsonObject customEmoji = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 106,
                "set_id": 206,
                "emoji": "⭐",
                "width": 100,
                "height": 100,
                "format": { "@type": "stickerFormatAnimated" },
                "type": { "@type": "stickerTypeCustomEmoji" },
                "thumbnail": { "file": { "id": 18 } },
                "sticker": { "id": 19 }
            }
        }
    })")
                                       .object();
    const StickerInfo emojiInfo = stickerFromMessage(customEmoji);
    QVERIFY(!emojiInfo.isMask);
    QVERIFY(emojiInfo.isAnimated);
    QCOMPARE(emojiInfo.emoji, QStringLiteral("⭐"));
    QCOMPARE(emojiInfo.thumbnailFileId, 18);
    QCOMPARE(emojiInfo.fullFileId, 19);
}

void StickerInfoTest::parsesPremiumSticker()
{
    const QJsonObject premium = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 107,
                "set_id": 207,
                "format": { "@type": "stickerFormatAnimated" },
                "type": { "@type": "stickerTypeRegular" },
                "premium_animation": { "id": 20 },
                "sticker": { "id": 21 }
            }
        }
    })")
                                    .object();
    const StickerInfo premiumInfo = stickerFromMessage(premium);
    QVERIFY(premiumInfo.isPremium);
    QVERIFY(premiumInfo.isAnimated);

    const QJsonObject flagged = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 108,
                "format": { "@type": "stickerFormatStatic" },
                "sticker": { "id": 22 }
            },
            "is_premium": true
        }
    })")
                                    .object();
    QVERIFY(stickerFromMessage(flagged).isPremium);

    const QJsonObject plain = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": 109,
                "format": { "@type": "stickerFormatStatic" },
                "sticker": { "id": 23 }
            },
            "is_premium": false
        }
    })")
                                   .object();
    QVERIFY(!stickerFromMessage(plain).isPremium);
}

void StickerInfoTest::ignoresNonStickerMessage()
{
    const QJsonObject document = QJsonDocument::fromJson(R"({
        "id": 11,
        "chat_id": 42,
        "content": {
            "@type": "messageDocument",
            "document": {
                "file_name": "file.pdf",
                "document": { "id": 30 }
            }
        }
    })")
                                     .object();
    const StickerInfo info = stickerFromMessage(document);
    QCOMPARE(info.stickerId, qint64(0));
    QCOMPARE(info.setId, qint64(0));
    QVERIFY(info.emoji.isEmpty());
    QCOMPARE(info.width, 0);
    QCOMPARE(info.height, 0);
    QCOMPARE(info.thumbnailFileId, 0);
    QCOMPARE(info.fullFileId, 0);
    QVERIFY(!info.isAnimated);
    QVERIFY(!info.isVideo);
    QVERIFY(!info.isMask);
    QVERIFY(!info.isPremium);

    const StickerInfo empty = stickerFromMessage(QJsonObject());
    QCOMPARE(empty.stickerId, qint64(0));
    QVERIFY(empty.emoji.isEmpty());
    QCOMPARE(empty.fullFileId, 0);

    const QJsonObject missingSticker = QJsonDocument::fromJson(R"({
        "content": { "@type": "messageSticker", "sticker": null }
    })")
                                           .object();
    const StickerInfo nullSticker = stickerFromMessage(missingSticker);
    QCOMPARE(nullSticker.stickerId, qint64(0));
    QCOMPARE(nullSticker.thumbnailFileId, 0);
    QVERIFY(!nullSticker.isPremium);
}

void StickerInfoTest::readsIdsFromStrings()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": "1234567890123",
                "set_id": "1234567890124",
                "emoji": "🔥",
                "width": 256,
                "height": 256,
                "format": { "@type": "stickerFormatStatic" },
                "type": { "@type": "stickerTypeRegular" },
                "thumbnail": { "file": { "id": "44" } },
                "sticker": { "id": "45" }
            }
        }
    })")
                                    .object();

    const StickerInfo info = stickerFromMessage(message);
    QCOMPARE(info.stickerId, qint64(1234567890123LL));
    QCOMPARE(info.setId, qint64(1234567890124LL));
    QCOMPARE(info.thumbnailFileId, 44);
    QCOMPARE(info.fullFileId, 45);
    QCOMPARE(info.emoji, QStringLiteral("🔥"));

    const QJsonObject hugeFileId = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messageSticker",
            "sticker": {
                "id": "1",
                "thumbnail": { "file": { "id": "9007199254740993" } },
                "sticker": { "id": "9007199254740994" }
            }
        }
    })")
                                       .object();
    const StickerInfo huge = stickerFromMessage(hugeFileId);
    QCOMPARE(huge.fullFileId, 0);
    QCOMPARE(huge.thumbnailFileId, 0);
}

void StickerInfoTest::buildsGetStickerSetRequest()
{
    const QJsonObject request = getStickerSetRequest(200);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStickerSet"));
    QCOMPARE(request.value(QStringLiteral("set_id")).toVariant().toLongLong(),
             qint64(200));
    QCOMPARE(request.size(), 2);

    const QJsonObject large = getStickerSetRequest(1234567890124LL);
    QCOMPARE(large.value(QStringLiteral("set_id")).toVariant().toLongLong(),
             qint64(1234567890124LL));

    const QJsonObject encoded = QJsonDocument(large).object();
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStickerSet"));
}

QTEST_GUILESS_MAIN(StickerInfoTest)
#include "tst_sticker_info.moc"
