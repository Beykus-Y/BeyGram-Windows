#include "telegram/ReplyPreviewBuilder.h"

#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject messageWithContent(const QJsonObject& content)
{
    return QJsonObject{{QStringLiteral("content"), content}};
}

QJsonObject textContent(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageText")},
        {QStringLiteral("text"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("formattedText")},
                     {QStringLiteral("text"), text}}},
    };
}

QJsonObject textMessage(const QString& text)
{
    return messageWithContent(textContent(text));
}

QJsonObject mediaContent(const QString& type, const QString& caption)
{
    return QJsonObject{
        {QStringLiteral("@type"), type},
        {QStringLiteral("caption"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("formattedText")},
                     {QStringLiteral("text"), caption}}},
    };
}

QJsonObject mediaMessage(const QString& type, const QString& caption)
{
    return messageWithContent(mediaContent(type, caption));
}

QJsonObject mediaMessageWithoutCaption(const QString& type)
{
    return messageWithContent(QJsonObject{{QStringLiteral("@type"), type}});
}

/// True when `text` contains a high surrogate without a low surrogate, or a
/// low surrogate without a high surrogate.
bool hasLoneSurrogate(const QString& text)
{
    for (qsizetype index = 0; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (character.isHighSurrogate()) {
            if (index + 1 >= text.size() || !text.at(index + 1).isLowSurrogate())
                return true;
            ++index;
        } else if (character.isLowSurrogate()) {
            return true;
        }
    }
    return false;
}

/// U+1F600 GRINNING FACE as an explicit UTF-16 surrogate pair, so the test does
/// not depend on the source encoding.
QString grinningFace()
{
    return QString(QChar(0xD83D)) + QChar(0xDE00);
}

} // namespace

class ReplyPreviewBuilderTest final : public QObject {
    Q_OBJECT

private slots:
    void previewsText();
    void collapsesWhitespace();
    void usesCaptionForMedia();
    void fallsBackToMediaLabel();
    void labelsKnownMedia_data();
    void labelsKnownMedia();
    void labelsUnknownContentAsMessage();
    void ignoresStickerEmojiAndPollQuestion();
    void capsPreview();
    void keepsSurrogatePairs();
    void rejectsNonPositiveLimit();
    void fallsBackToQuoteWhenContentMissing();
    void quoteDoesNotOverrideContent();
    void labelsOutgoingSender();
    void labelsIncomingSender();
    void senderFallsBackToSignature();
    void senderIsEmptyWhenUnknown();
};

void ReplyPreviewBuilderTest::previewsText()
{
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("Hello there"))),
             QStringLiteral("Hello there"));
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("  padded  "))),
             QStringLiteral("padded"));
    QCOMPARE(replyPreviewForMessage(textMessage(QString())), QStringLiteral("Message"));
}

void ReplyPreviewBuilderTest::collapsesWhitespace()
{
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("  one\n\t two   three  "))),
             QStringLiteral("one two three"));
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("\n\n\t  "))),
             QStringLiteral("Message"));
}

void ReplyPreviewBuilderTest::usesCaptionForMedia()
{
    QCOMPARE(replyPreviewForMessage(
                 mediaMessage(QStringLiteral("messagePhoto"),
                              QStringLiteral("  A  caption\nhere "))),
             QStringLiteral("A caption here"));
    QCOMPARE(replyPreviewForMessage(
                 mediaMessage(QStringLiteral("messageDocument"), QStringLiteral("report.pdf"))),
             QStringLiteral("report.pdf"));
}

void ReplyPreviewBuilderTest::fallsBackToMediaLabel()
{
    QCOMPARE(replyPreviewForMessage(mediaMessageWithoutCaption(QStringLiteral("messagePhoto"))),
             QStringLiteral("Photo"));
    QCOMPARE(replyPreviewForMessage(
                 mediaMessage(QStringLiteral("messagePhoto"), QStringLiteral(""))),
             QStringLiteral("Photo"));
    QCOMPARE(replyPreviewForMessage(
                 mediaMessage(QStringLiteral("messagePhoto"), QStringLiteral("  \n "))),
             QStringLiteral("Photo"));
    QCOMPARE(replyPreviewForMessage(messageWithContent(QJsonObject{})),
             QStringLiteral("Message"));
    QCOMPARE(replyPreviewForMessage(QJsonObject{}), QStringLiteral("Message"));
}

void ReplyPreviewBuilderTest::labelsKnownMedia_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("label");

    QTest::newRow("photo") << QStringLiteral("messagePhoto") << QStringLiteral("Photo");
    QTest::newRow("video") << QStringLiteral("messageVideo") << QStringLiteral("Video");
    QTest::newRow("animation") << QStringLiteral("messageAnimation") << QStringLiteral("GIF");
    QTest::newRow("audio") << QStringLiteral("messageAudio") << QStringLiteral("Audio");
    QTest::newRow("voice") << QStringLiteral("messageVoiceNote")
                           << QStringLiteral("Voice message");
    QTest::newRow("document") << QStringLiteral("messageDocument")
                              << QStringLiteral("Document");
    QTest::newRow("sticker") << QStringLiteral("messageSticker")
                             << QStringLiteral("Sticker");
    QTest::newRow("location") << QStringLiteral("messageLocation")
                              << QStringLiteral("Location");
    QTest::newRow("contact") << QStringLiteral("messageContact")
                             << QStringLiteral("Contact");
    QTest::newRow("poll") << QStringLiteral("messagePoll") << QStringLiteral("Poll");
    QTest::newRow("game") << QStringLiteral("messageGame") << QStringLiteral("Game");
    QTest::newRow("dice") << QStringLiteral("messageDice") << QStringLiteral("Dice");
}

void ReplyPreviewBuilderTest::labelsKnownMedia()
{
    QFETCH(QString, type);
    QFETCH(QString, label);

    QCOMPARE(mediaLabelForContent(mediaContent(type, QString())), label);
    QCOMPARE(replyPreviewForMessage(mediaMessageWithoutCaption(type)), label);
}

void ReplyPreviewBuilderTest::labelsUnknownContentAsMessage()
{
    QCOMPARE(mediaLabelForContent(QJsonObject{}), QStringLiteral("Message"));
    QCOMPARE(mediaLabelForContent(
                 QJsonObject{{QStringLiteral("@type"), QStringLiteral("messageFromTheFuture")}}),
             QStringLiteral("Message"));
    QCOMPARE(mediaLabelForContent(textContent(QStringLiteral("hello"))),
             QStringLiteral("Message"));
    QCOMPARE(replyPreviewForMessage(messageWithContent(
                 QJsonObject{{QStringLiteral("@type"),
                              QStringLiteral("messageFromTheFuture")}})),
             QStringLiteral("Message"));
}

void ReplyPreviewBuilderTest::ignoresStickerEmojiAndPollQuestion()
{
    const QJsonObject sticker = messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageSticker")},
        {QStringLiteral("sticker"),
         QJsonObject{{QStringLiteral("emoji"), QStringLiteral("smile")}}},
    });
    QCOMPARE(replyPreviewForMessage(sticker), QStringLiteral("Sticker"));

    const QJsonObject poll = messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messagePoll")},
        {QStringLiteral("poll"),
         QJsonObject{{QStringLiteral("question"),
                      QJsonObject{{QStringLiteral("text"), QStringLiteral("Best?")}}}}},
    });
    QCOMPARE(replyPreviewForMessage(poll), QStringLiteral("Poll"));

    const QJsonObject dice = messageWithContent(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("messageDice")},
        {QStringLiteral("emoji"), QStringLiteral("dice")},
    });
    QCOMPARE(replyPreviewForMessage(dice), QStringLiteral("Dice"));
}

void ReplyPreviewBuilderTest::capsPreview()
{
    const QString longText(200, QLatin1Char('a'));
    const QString defaultPreview = replyPreviewForMessage(textMessage(longText));
    QCOMPARE(defaultPreview.size(), 120);
    QCOMPARE(defaultPreview, longText.left(120));

    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("abcdef")), 4),
             QStringLiteral("abcd"));
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("abcd")), 4),
             QStringLiteral("abcd"));
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("aa   bb   cc")), 5),
             QStringLiteral("aa bb"));
    // Truncation must not leave the space it just exposed at the end.
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("ab cd")), 3),
             QStringLiteral("ab"));
    QCOMPARE(replyPreviewForMessage(mediaMessageWithoutCaption(QStringLiteral("messagePhoto")), 3),
             QStringLiteral("Pho"));
}

void ReplyPreviewBuilderTest::keepsSurrogatePairs()
{
    const QString emoji = grinningFace();
    const QString text = QStringLiteral("ab") + emoji + QStringLiteral("cd");

    const QString cutInsidePair = replyPreviewForMessage(textMessage(text), 3);
    QCOMPARE(cutInsidePair, QStringLiteral("ab"));
    QVERIFY(!hasLoneSurrogate(cutInsidePair));

    const QString cutAfterPair = replyPreviewForMessage(textMessage(text), 4);
    QCOMPARE(cutAfterPair, QStringLiteral("ab") + emoji);
    QVERIFY(!hasLoneSurrogate(cutAfterPair));

    const QString firstUnit = replyPreviewForMessage(textMessage(emoji + QStringLiteral("x")), 1);
    QVERIFY(firstUnit.isEmpty());
    QVERIFY(!hasLoneSurrogate(firstUnit));

    QCOMPARE(replyPreviewForMessage(textMessage(emoji + QStringLiteral("x")), 2), emoji);

    const QString manyEmoji = emoji.repeated(100);
    const QString capped = replyPreviewForMessage(textMessage(manyEmoji), 121);
    QCOMPARE(capped, emoji.repeated(60));
    QVERIFY(!hasLoneSurrogate(capped));
}

void ReplyPreviewBuilderTest::rejectsNonPositiveLimit()
{
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("hello")), 0), QString());
    QCOMPARE(replyPreviewForMessage(textMessage(QStringLiteral("hello")), -1), QString());
    QCOMPARE(replyPreviewForMessage(mediaMessageWithoutCaption(QStringLiteral("messagePhoto")), 0),
             QString());
}

void ReplyPreviewBuilderTest::fallsBackToQuoteWhenContentMissing()
{
    const QJsonObject message{
        {QStringLiteral("reply_to"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("messageReplyToMessage")},
             {QStringLiteral("message_id"), 900},
             {QStringLiteral("quote"),
              QJsonObject{
                  {QStringLiteral("text"),
                   QJsonObject{
                       {QStringLiteral("text"), QStringLiteral("  quoted\n words  ")}}},
              }},
         }},
    };

    QCOMPARE(replyPreviewForMessage(message), QStringLiteral("quoted words"));
    QCOMPARE(replyPreviewForMessage(message, 6), QStringLiteral("quoted"));
}

void ReplyPreviewBuilderTest::quoteDoesNotOverrideContent()
{
    QJsonObject message = mediaMessageWithoutCaption(QStringLiteral("messagePhoto"));
    message.insert(QStringLiteral("reply_to"),
                   QJsonObject{
                       {QStringLiteral("@type"), QStringLiteral("messageReplyToMessage")},
                       {QStringLiteral("message_id"), 900},
                       {QStringLiteral("quote"),
                        QJsonObject{
                            {QStringLiteral("text"),
                             QJsonObject{{QStringLiteral("text"),
                                          QStringLiteral("quoted words")}}},
                        }},
                   });

    QCOMPARE(replyPreviewForMessage(message), QStringLiteral("Photo"));
}

void ReplyPreviewBuilderTest::labelsOutgoingSender()
{
    const QJsonObject named{
        {QStringLiteral("sender_name"), QStringLiteral("Alice")}};
    QCOMPARE(replySenderLabel(named, true), QStringLiteral("You"));
    QCOMPARE(replySenderLabel(QJsonObject{}, true), QStringLiteral("You"));
}

void ReplyPreviewBuilderTest::labelsIncomingSender()
{
    const QJsonObject named{
        {QStringLiteral("sender_name"), QStringLiteral("  Alice \n Smith ")}};
    QCOMPARE(replySenderLabel(named, false), QStringLiteral("Alice Smith"));

    const QJsonObject both{
        {QStringLiteral("sender_name"), QStringLiteral("Alice")},
        {QStringLiteral("author_signature"), QStringLiteral("Admin")}};
    QCOMPARE(replySenderLabel(both, false), QStringLiteral("Alice"));
}

void ReplyPreviewBuilderTest::senderFallsBackToSignature()
{
    const QJsonObject signedMessage{
        {QStringLiteral("author_signature"), QStringLiteral("  Channel Admin  ")}};
    QCOMPARE(replySenderLabel(signedMessage, false), QStringLiteral("Channel Admin"));

    const QJsonObject blankName{
        {QStringLiteral("sender_name"), QStringLiteral(" \n ")},
        {QStringLiteral("author_signature"), QStringLiteral("Admin")}};
    QCOMPARE(replySenderLabel(blankName, false), QStringLiteral("Admin"));
}

void ReplyPreviewBuilderTest::senderIsEmptyWhenUnknown()
{
    QVERIFY(replySenderLabel(QJsonObject{}, false).isEmpty());

    const QJsonObject emptyFields{
        {QStringLiteral("sender_name"), QStringLiteral("")},
        {QStringLiteral("author_signature"), QStringLiteral("")}};
    QVERIFY(replySenderLabel(emptyFields, false).isEmpty());
}

QTEST_GUILESS_MAIN(ReplyPreviewBuilderTest)
#include "tst_reply_preview_builder.moc"
