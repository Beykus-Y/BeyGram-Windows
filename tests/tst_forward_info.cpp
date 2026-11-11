#include "telegram/ForwardInfo.h"

#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

class ForwardInfoTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesUserOrigin();
    void parsesHiddenUserOrigin();
    void parsesChatOrigin();
    void parsesChannelOrigin();
    void parsesAnonymousOrigin();
    void ignoresMissingForwardInfo();
    void ignoresUnknownOrigin();
    void readsIdsFromNumbersAndStrings();
    void readsAuthorSignature();
};

void ForwardInfoTest::parsesUserOrigin()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 11,
        "chat_id": 42,
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginUser",
                "sender_user_id": 4242
            },
            "date": 1700000000,
            "source": {
                "@type": "messageSourceUser",
                "sender_user_id": 4242
            }
        }
    })")
                                    .object();

    const ForwardInfo info = forwardInfoFromMessage(message);
    QCOMPARE(info.origin, ForwardInfo::Origin::User);
    QCOMPARE(info.senderUserId, qint64(4242));
    QCOMPARE(info.date, qint64(1700000000));
    QCOMPARE(info.sourceChatId, qint64(0));
    QCOMPARE(info.sourceMessageId, qint64(0));
    // TDLib carries no name for a visible user forward; nothing is invented.
    QVERIFY(info.senderName.isEmpty());
    QVERIFY(info.chatTitle.isEmpty());
    QVERIFY(info.authorSignature.isEmpty());
}

void ForwardInfoTest::parsesHiddenUserOrigin()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 12,
        "chat_id": 42,
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginHiddenUser",
                "sender_name": "Mystery Reader"
            },
            "date": 1700000123,
            "source": {
                "@type": "messageSourceChatHistory"
            }
        }
    })")
                                    .object();

    const ForwardInfo info = forwardInfoFromMessage(message);
    QCOMPARE(info.origin, ForwardInfo::Origin::HiddenUser);
    QCOMPARE(info.senderName, QStringLiteral("Mystery Reader"));
    QCOMPARE(info.date, qint64(1700000123));
    QCOMPARE(info.senderUserId, qint64(0));
    QCOMPARE(info.sourceChatId, qint64(0));
    QCOMPARE(info.sourceMessageId, qint64(0));
    QVERIFY(info.chatTitle.isEmpty());
    QVERIFY(info.authorSignature.isEmpty());
}

void ForwardInfoTest::parsesChatOrigin()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 13,
        "chat_id": 42,
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginChat",
                "sender_chat_id": -1009876543210,
                "author_signature": "Group Admin"
            },
            "date": 1700000456,
            "source": {
                "@type": "messageSourceChat",
                "chat_id": -1009876543210
            }
        }
    })")
                                    .object();

    const ForwardInfo info = forwardInfoFromMessage(message);
    QCOMPARE(info.origin, ForwardInfo::Origin::Chat);
    QCOMPARE(info.sourceChatId, qint64(-1009876543210));
    // Chat origins do not identify the original message.
    QCOMPARE(info.sourceMessageId, qint64(0));
    QCOMPARE(info.senderUserId, qint64(0));
    QVERIFY(info.senderName.isEmpty());
    QVERIFY(info.chatTitle.isEmpty());
    QCOMPARE(info.date, qint64(1700000456));
}

void ForwardInfoTest::parsesChannelOrigin()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 14,
        "chat_id": 42,
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginChannel",
                "chat_id": -1001234567890,
                "message_id": 777,
                "author_signature": "Editor"
            },
            "date": 1700000789,
            "source": {
                "@type": "messageSourceChannel",
                "chat_id": -1001234567890,
                "message_id": 777
            }
        }
    })")
                                    .object();

    const ForwardInfo info = forwardInfoFromMessage(message);
    QCOMPARE(info.origin, ForwardInfo::Origin::Channel);
    QCOMPARE(info.sourceChatId, qint64(-1001234567890));
    QCOMPARE(info.sourceMessageId, qint64(777));
    QCOMPARE(info.date, qint64(1700000789));
    QCOMPARE(info.senderUserId, qint64(0));
    QVERIFY(info.senderName.isEmpty());
    QVERIFY(info.chatTitle.isEmpty());
}

void ForwardInfoTest::parsesAnonymousOrigin()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "id": 15,
        "chat_id": 42,
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginAnonymous"
            },
            "date": 1700000999,
            "source": {
                "@type": "messageSourceOther"
            }
        }
    })")
                                    .object();

    const ForwardInfo info = forwardInfoFromMessage(message);
    QCOMPARE(info.origin, ForwardInfo::Origin::Anonymous);
    QCOMPARE(info.date, qint64(1700000999));
    QCOMPARE(info.sourceChatId, qint64(0));
    QCOMPARE(info.sourceMessageId, qint64(0));
    QCOMPARE(info.senderUserId, qint64(0));
    QVERIFY(info.senderName.isEmpty());
    QVERIFY(info.chatTitle.isEmpty());
    QVERIFY(info.authorSignature.isEmpty());
}

void ForwardInfoTest::ignoresMissingForwardInfo()
{
    const QJsonObject withoutKey = QJsonDocument::fromJson(R"({
        "id": 16,
        "chat_id": 42,
        "date": 1700000000
    })")
                                       .object();
    const ForwardInfo missing = forwardInfoFromMessage(withoutKey);
    QCOMPARE(missing.origin, ForwardInfo::Origin::None);
    QCOMPARE(missing.date, qint64(0));
    QCOMPARE(missing.sourceChatId, qint64(0));
    QCOMPARE(missing.sourceMessageId, qint64(0));
    QCOMPARE(missing.senderUserId, qint64(0));
    QVERIFY(missing.senderName.isEmpty());
    QVERIFY(missing.chatTitle.isEmpty());
    QVERIFY(missing.authorSignature.isEmpty());

    const QJsonObject nullForward = QJsonDocument::fromJson(R"({
        "id": 17,
        "chat_id": 42,
        "forward_info": null
    })")
                                        .object();
    QCOMPARE(forwardInfoFromMessage(nullForward).origin, ForwardInfo::Origin::None);

    // An empty object is still not a usable forward.
    const QJsonObject emptyForward = QJsonDocument::fromJson(R"({
        "id": 18,
        "chat_id": 42,
        "forward_info": {}
    })")
                                         .object();
    const ForwardInfo empty = forwardInfoFromMessage(emptyForward);
    QCOMPARE(empty.origin, ForwardInfo::Origin::None);
    QCOMPARE(empty.date, qint64(0));
}

void ForwardInfoTest::ignoresUnknownOrigin()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginFromTheFuture",
                "sender_user_id": 4242
            },
            "date": 1700000000
        }
    })")
                                    .object();

    const ForwardInfo info = forwardInfoFromMessage(message);
    QCOMPARE(info.origin, ForwardInfo::Origin::None);
    QCOMPARE(info.senderUserId, qint64(0));
    QCOMPARE(info.date, qint64(1700000000));
}

void ForwardInfoTest::readsIdsFromNumbersAndStrings()
{
    const QJsonObject numeric = QJsonDocument::fromJson(R"({
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginChannel",
                "chat_id": -1001234567890,
                "message_id": 1234567890123,
                "author_signature": ""
            },
            "date": 1700000000
        }
    })")
                                    .object();
    const QJsonObject textual = QJsonDocument::fromJson(R"({
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginChannel",
                "chat_id": "-1001234567890",
                "message_id": "1234567890123",
                "author_signature": ""
            },
            "date": "1700000000"
        }
    })")
                                    .object();

    const ForwardInfo fromNumbers = forwardInfoFromMessage(numeric);
    const ForwardInfo fromStrings = forwardInfoFromMessage(textual);
    QCOMPARE(fromNumbers.sourceChatId, qint64(-1001234567890));
    QCOMPARE(fromNumbers.sourceMessageId, qint64(1234567890123));
    QCOMPARE(fromNumbers.date, qint64(1700000000));
    QCOMPARE(fromStrings.origin, ForwardInfo::Origin::Channel);
    QCOMPARE(fromStrings.sourceChatId, fromNumbers.sourceChatId);
    QCOMPARE(fromStrings.sourceMessageId, fromNumbers.sourceMessageId);
    QCOMPARE(fromStrings.date, fromNumbers.date);

    const QJsonObject userStrings = QJsonDocument::fromJson(R"({
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginUser",
                "sender_user_id": "4242"
            }
        }
    })")
                                        .object();
    const ForwardInfo user = forwardInfoFromMessage(userStrings);
    QCOMPARE(user.origin, ForwardInfo::Origin::User);
    QCOMPARE(user.senderUserId, qint64(4242));
}

void ForwardInfoTest::readsAuthorSignature()
{
    const QJsonObject channel = QJsonDocument::fromJson(R"({
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginChannel",
                "chat_id": -1001234567890,
                "message_id": 777,
                "author_signature": "Editor"
            },
            "date": 1700000789
        }
    })")
                                    .object();
    const ForwardInfo channelInfo = forwardInfoFromMessage(channel);
    QCOMPARE(channelInfo.authorSignature, QStringLiteral("Editor"));

    const QJsonObject chat = QJsonDocument::fromJson(R"({
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginChat",
                "sender_chat_id": -1009876543210,
                "author_signature": "Group Admin"
            },
            "date": 1700000456
        }
    })")
                                 .object();
    QCOMPARE(forwardInfoFromMessage(chat).authorSignature, QStringLiteral("Group Admin"));

    const QJsonObject unsignedChannel = QJsonDocument::fromJson(R"({
        "forward_info": {
            "origin": {
                "@type": "messageForwardOriginChannel",
                "chat_id": -1001234567890,
                "message_id": 778
            },
            "date": 1700000790
        }
    })")
                                            .object();
    const ForwardInfo unsignedInfo = forwardInfoFromMessage(unsignedChannel);
    QCOMPARE(unsignedInfo.origin, ForwardInfo::Origin::Channel);
    QVERIFY(unsignedInfo.authorSignature.isEmpty());
}

QTEST_GUILESS_MAIN(ForwardInfoTest)
#include "tst_forward_info.moc"
