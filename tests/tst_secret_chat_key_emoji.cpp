#include "telegram/SecretChatKeyEmoji.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// Emoji are built from universal character names so the tests do not depend on
// the source encoding the compiler assumes.
QString character(const char16_t* value)
{
    return QString::fromUtf16(value);
}

// A realistic 36-byte key hash (bytes 0x00..0x23) in standard base64.
QString sampleHash()
{
    return QStringLiteral("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIj");
}

// A second 36-byte key hash (36 bytes of 0xFF) in standard base64.
QString otherHash()
{
    return QString(48, QLatin1Char('/'));
}

QStringList emojisOf(const QVector<KeyEmoji>& entries)
{
    QStringList emojis;
    emojis.reserve(entries.size());
    for (const KeyEmoji& entry : entries)
        emojis.append(entry.emoji);
    return emojis;
}

QStringList labelsOf(const QVector<KeyEmoji>& entries)
{
    QStringList labels;
    labels.reserve(entries.size());
    for (const KeyEmoji& entry : entries)
        labels.append(entry.label);
    return labels;
}

KeyEmoji grinning()
{
    return KeyEmoji{character(u"\U0001F600"), QStringLiteral("Grinning face")};
}

KeyEmoji joy()
{
    return KeyEmoji{character(u"\U0001F602"), QStringLiteral("Face with tears of joy")};
}

} // namespace

class SecretChatKeyEmojiTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyHashYieldsNothing();
    void whitespaceHashYieldsNothing();
    void invalidBase64YieldsNothing();
    void surroundingWhitespaceIsIgnored();

    void producesExactlyFourEmoji();
    void keepsDuplicateEmoji();
    void labelsAreEmojiNames();
    void isDeterministic();
    void mapsKnownHashToKnownEmoji();
    void differentHashesProduceDifferentEmoji();

    void fingerprintIsFirstFourSha256Bytes();
    void fingerprintIsLowercaseHex();
    void fingerprintMatchesKnownHash();
    void fingerprintIgnoresSurroundingWhitespace();
    void fingerprintIsEmptyForInvalidHash();

    void stateReadsKeyHash();
    void stateWithoutKeyHashYieldsNothing();
    void stateWithNonStringKeyHashYieldsNothing();
    void stateWithInvalidKeyHashYieldsNothing();
    void stateIgnoresSurroundingWhitespace();

    void matchesEqualSequences();
    void matchesRejectsEmptySequences();
    void matchesRejectsDifferentSizes();
    void matchesIsOrderSensitive();
    void matchesRejectsDifferentEmoji();
    void matchesIgnoresLabels();
};

void SecretChatKeyEmojiTest::emptyHashYieldsNothing()
{
    QVERIFY(keyEmojiFromHash(QString()).isEmpty());
    QVERIFY(keyEmojiFromHash(QStringLiteral("")).isEmpty());
    QVERIFY(keyHashFingerprint(QString()).isEmpty());
}

void SecretChatKeyEmojiTest::whitespaceHashYieldsNothing()
{
    QVERIFY(keyEmojiFromHash(QStringLiteral("   ")).isEmpty());
    QVERIFY(keyEmojiFromHash(QStringLiteral("\n\t")).isEmpty());
    QVERIFY(keyHashFingerprint(QStringLiteral(" \t\n")).isEmpty());
}

void SecretChatKeyEmojiTest::invalidBase64YieldsNothing()
{
    const QStringList invalid = {
        QStringLiteral("not base64!"),
        QStringLiteral("!!!!"),
        QStringLiteral("@@@@"),
        QStringLiteral("abc$def"),
    };

    for (const QString& hash : invalid) {
        QVERIFY2(keyEmojiFromHash(hash).isEmpty(), qPrintable(hash));
        QVERIFY2(keyHashFingerprint(hash).isEmpty(), qPrintable(hash));
    }
}

void SecretChatKeyEmojiTest::surroundingWhitespaceIsIgnored()
{
    const QString padded = QStringLiteral("  ") + sampleHash() + QStringLiteral("\n");

    QCOMPARE(emojisOf(keyEmojiFromHash(padded)), emojisOf(keyEmojiFromHash(sampleHash())));
    QCOMPARE(labelsOf(keyEmojiFromHash(padded)), labelsOf(keyEmojiFromHash(sampleHash())));
    QCOMPARE(keyHashFingerprint(padded), keyHashFingerprint(sampleHash()));
}

void SecretChatKeyEmojiTest::producesExactlyFourEmoji()
{
    for (const QString& hash : {sampleHash(), otherHash()}) {
        const QVector<KeyEmoji> key = keyEmojiFromHash(hash);
        QCOMPARE(key.size(), 4);
        for (const KeyEmoji& entry : key) {
            QVERIFY(!entry.emoji.isEmpty());
            QVERIFY(!entry.label.isEmpty());
        }
    }
}

void SecretChatKeyEmojiTest::keepsDuplicateEmoji()
{
    // "a2V5LTI0" is base64 for "key-24"; its first four SHA-256 bytes are
    // 0F 0A 6D CF, i.e. the table indexes 15, 10, 45 and 15. The repeated
    // index must stay a repeated emoji: the sequence is never deduplicated.
    const QVector<KeyEmoji> key = keyEmojiFromHash(QStringLiteral("a2V5LTI0"));

    QCOMPARE(key.size(), 4);
    QCOMPARE(emojisOf(key),
             QStringList({character(u"\U0001F482"), character(u"\U0001F62C"),
                          character(u"\U0001F450"), character(u"\U0001F482")}));
}

void SecretChatKeyEmojiTest::labelsAreEmojiNames()
{
    const QVector<KeyEmoji> key = keyEmojiFromHash(sampleHash());

    QCOMPARE(labelsOf(key),
             QStringList({QStringLiteral("Woman dancing"), QStringLiteral("Tongue"),
                          QStringLiteral("Flexed biceps"), QStringLiteral("Woman's hat")}));
    for (const KeyEmoji& entry : key)
        QVERIFY(entry.emoji != entry.label);
}

void SecretChatKeyEmojiTest::isDeterministic()
{
    const QVector<KeyEmoji> first = keyEmojiFromHash(sampleHash());
    const QVector<KeyEmoji> second = keyEmojiFromHash(sampleHash());

    QCOMPARE(emojisOf(first), emojisOf(second));
    QCOMPARE(labelsOf(first), labelsOf(second));
}

void SecretChatKeyEmojiTest::mapsKnownHashToKnownEmoji()
{
    // The first four SHA-256 bytes of sampleHash() are B7 65 F4 7F, i.e. the
    // table indexes 55, 37, 52 and 63.
    QCOMPARE(emojisOf(keyEmojiFromHash(sampleHash())),
             QStringList({character(u"\U0001F483"), character(u"\U0001F445"),
                          character(u"\U0001F4AA"), character(u"\U0001F452")}));

    // The first four SHA-256 bytes of otherHash() are 53 07 FC 03, i.e. the
    // table indexes 19, 7, 60 and 3.
    QCOMPARE(emojisOf(keyEmojiFromHash(otherHash())),
             QStringList({character(u"\U0001F474"), character(u"\U0001F634"),
                          character(u"\U0001F485"), character(u"\U0001F62D")}));
}

void SecretChatKeyEmojiTest::differentHashesProduceDifferentEmoji()
{
    QVERIFY(emojisOf(keyEmojiFromHash(sampleHash()))
            != emojisOf(keyEmojiFromHash(otherHash())));
}

void SecretChatKeyEmojiTest::fingerprintIsFirstFourSha256Bytes()
{
    const QByteArray digest =
        QCryptographicHash::hash(sampleHash().toUtf8(), QCryptographicHash::Sha256);
    const QString expected = QString::fromLatin1(digest.left(4).toHex());

    QCOMPARE(keyHashFingerprint(sampleHash()), expected);
    QCOMPARE(expected.size(), 8);
}

void SecretChatKeyEmojiTest::fingerprintIsLowercaseHex()
{
    const QString fingerprint = keyHashFingerprint(sampleHash());
    const QRegularExpression hex(QStringLiteral("^[0-9a-f]{8}$"));

    QCOMPARE(fingerprint.size(), 8);
    QVERIFY(hex.match(fingerprint).hasMatch());
    QCOMPARE(fingerprint, fingerprint.toLower());
}

void SecretChatKeyEmojiTest::fingerprintMatchesKnownHash()
{
    QCOMPARE(keyHashFingerprint(sampleHash()), QStringLiteral("b765f47f"));
    QCOMPARE(keyHashFingerprint(otherHash()), QStringLiteral("5307fc03"));
}

void SecretChatKeyEmojiTest::fingerprintIgnoresSurroundingWhitespace()
{
    QCOMPARE(keyHashFingerprint(QStringLiteral("\t") + sampleHash() + QStringLiteral("  ")),
             keyHashFingerprint(sampleHash()));
}

void SecretChatKeyEmojiTest::fingerprintIsEmptyForInvalidHash()
{
    QVERIFY(keyHashFingerprint(QString()).isEmpty());
    QVERIFY(keyHashFingerprint(QStringLiteral("   ")).isEmpty());
    QVERIFY(keyHashFingerprint(QStringLiteral("not base64!")).isEmpty());
    QVERIFY(keyHashFingerprint(QStringLiteral("@@@@")).isEmpty());
}

void SecretChatKeyEmojiTest::stateReadsKeyHash()
{
    QJsonObject state;
    state.insert(QStringLiteral("@type"), QStringLiteral("secretChat"));
    state.insert(QStringLiteral("key_hash"), sampleHash());

    QCOMPARE(emojisOf(keyEmojiFromState(state)), emojisOf(keyEmojiFromHash(sampleHash())));
    QCOMPARE(labelsOf(keyEmojiFromState(state)), labelsOf(keyEmojiFromHash(sampleHash())));
}

void SecretChatKeyEmojiTest::stateWithoutKeyHashYieldsNothing()
{
    QVERIFY(keyEmojiFromState(QJsonObject()).isEmpty());

    QJsonObject state;
    state.insert(QStringLiteral("@type"), QStringLiteral("secretChat"));
    state.insert(QStringLiteral("secret_chat_id"), 7);
    QVERIFY(keyEmojiFromState(state).isEmpty());
}

void SecretChatKeyEmojiTest::stateWithNonStringKeyHashYieldsNothing()
{
    const QJsonArray values = {
        QJsonValue(), QJsonValue(0), QJsonValue(false), QJsonValue(QJsonObject()),
        QJsonValue(QJsonArray()),
    };

    for (const QJsonValue& value : values) {
        QJsonObject state;
        state.insert(QStringLiteral("key_hash"), value);
        QVERIFY(keyEmojiFromState(state).isEmpty());
    }
}

void SecretChatKeyEmojiTest::stateWithInvalidKeyHashYieldsNothing()
{
    QJsonObject state;

    state.insert(QStringLiteral("key_hash"), QString());
    QVERIFY(keyEmojiFromState(state).isEmpty());

    state.insert(QStringLiteral("key_hash"), QStringLiteral("   "));
    QVERIFY(keyEmojiFromState(state).isEmpty());

    state.insert(QStringLiteral("key_hash"), QStringLiteral("not base64!"));
    QVERIFY(keyEmojiFromState(state).isEmpty());
}

void SecretChatKeyEmojiTest::stateIgnoresSurroundingWhitespace()
{
    QJsonObject state;
    state.insert(QStringLiteral("key_hash"),
                 QStringLiteral(" ") + sampleHash() + QStringLiteral("\n"));

    QCOMPARE(emojisOf(keyEmojiFromState(state)), emojisOf(keyEmojiFromHash(sampleHash())));
}

void SecretChatKeyEmojiTest::matchesEqualSequences()
{
    const QVector<KeyEmoji> left = keyEmojiFromHash(sampleHash());
    const QVector<KeyEmoji> right = keyEmojiFromHash(sampleHash());

    QVERIFY(keyEmojiMatches(left, right));
    QVERIFY(keyEmojiMatches(left, left));
}

void SecretChatKeyEmojiTest::matchesRejectsEmptySequences()
{
    const QVector<KeyEmoji> key = keyEmojiFromHash(sampleHash());

    QVERIFY(!keyEmojiMatches(QVector<KeyEmoji>(), QVector<KeyEmoji>()));
    QVERIFY(!keyEmojiMatches(key, QVector<KeyEmoji>()));
    QVERIFY(!keyEmojiMatches(QVector<KeyEmoji>(), key));
}

void SecretChatKeyEmojiTest::matchesRejectsDifferentSizes()
{
    const QVector<KeyEmoji> key = keyEmojiFromHash(sampleHash());
    QVector<KeyEmoji> shorter = key;
    shorter.removeLast();

    QVERIFY(!keyEmojiMatches(key, shorter));
    QVERIFY(!keyEmojiMatches(shorter, key));
}

void SecretChatKeyEmojiTest::matchesIsOrderSensitive()
{
    const KeyEmoji first = grinning();
    const KeyEmoji second = joy();

    QVERIFY(keyEmojiMatches({first, second}, {first, second}));
    QVERIFY(!keyEmojiMatches({first, second}, {second, first}));
}

void SecretChatKeyEmojiTest::matchesRejectsDifferentEmoji()
{
    const KeyEmoji first = grinning();
    const KeyEmoji second = joy();
    const KeyEmoji renamed{second.emoji, first.label};

    QVERIFY(!keyEmojiMatches({first}, {second}));
    QVERIFY(!keyEmojiMatches({first}, {renamed}));
}

void SecretChatKeyEmojiTest::matchesIgnoresLabels()
{
    const KeyEmoji first = grinning();
    const KeyEmoji sameEmoji{first.emoji, QStringLiteral("Different label")};

    QVERIFY(keyEmojiMatches({first}, {sameEmoji}));
}

QTEST_GUILESS_MAIN(SecretChatKeyEmojiTest)
#include "tst_secret_chat_key_emoji.moc"
