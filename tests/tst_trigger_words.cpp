#include "telegram/TriggerWords.h"

#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

class TriggerWordsTest final : public QObject {
    Q_OBJECT

private slots:
    void encodesCompactLineFormat();
    void encodeTrimsTextAndDropsEmpty();
    void encodeDeduplicatesByTextKeepingFirst();
    void encodeKeepsCaseVariants();
    void encodeOmitsUnrepresentableFields();
    void encodeEmptyList();
    void decodeRoundTripsEncodedWords();
    void decodeIgnoresMalformedLines();
    void decodeAcceptsCrlf();
    void decodeEmptyInput();
    void decodeKeepsEmptyEmoji();
    void applyReplacesEveryOccurrence();
    void applyLongestMatchWins();
    void applyIgnoresCaseByDefault();
    void applyHonoursCaseSensitivity();
    void applyRespectsAsciiWordBoundaries();
    void applyMatchesAtNonAsciiAndPunctuation();
    void applyTrimsAndDeduplicatesWords();
    void applyDoesNotCascade();
    void applyDoesNotRescanItsOwnOutput();
    void applyEmptyInputs();
    void applyPreservesUnmatchedSurrogatePairs();
};

namespace {

QString waveEmoji()
{
    return QString::fromUtf8("\xF0\x9F\x91\x8B"); // U+1F44B
}

QString smileEmoji()
{
    return QString::fromUtf8("\xF0\x9F\x99\x82"); // U+1F642
}

QString fireEmoji()
{
    return QString::fromUtf8("\xF0\x9F\x94\xA5"); // U+1F525
}

} // namespace

void TriggerWordsTest::encodesCompactLineFormat()
{
    const QString encoded = encodeTriggerWords({
        {QStringLiteral("hi"), waveEmoji(), false},
        {QStringLiteral("bye"), smileEmoji(), true},
    });

    QCOMPARE(encoded, QStringLiteral("hi|") + waveEmoji() + QStringLiteral("|0\nbye|")
                          + smileEmoji() + QStringLiteral("|1"));
}

void TriggerWordsTest::encodeTrimsTextAndDropsEmpty()
{
    QCOMPARE(encodeTriggerWords({
                 {QStringLiteral("  hi  "), QStringLiteral("x"), false},
                 {QStringLiteral("   "), QStringLiteral("y"), false},
                 {QString(), QStringLiteral("z"), true},
             }),
             QStringLiteral("hi|x|0"));
}

void TriggerWordsTest::encodeDeduplicatesByTextKeepingFirst()
{
    const QVector<TriggerWord> decoded = decodeTriggerWords(encodeTriggerWords({
        {QStringLiteral("lol"), QStringLiteral("first"), false},
        {QStringLiteral(" lol "), QStringLiteral("second"), true},
        {QStringLiteral("lol"), QStringLiteral("third"), true},
    }));

    QCOMPARE(decoded.size(), 1);
    QCOMPARE(decoded.at(0),
             (TriggerWord{QStringLiteral("lol"), QStringLiteral("first"), false}));
}

void TriggerWordsTest::encodeKeepsCaseVariants()
{
    // Deduplication compares text exactly, so case variants are separate words.
    const QVector<TriggerWord> decoded = decodeTriggerWords(encodeTriggerWords({
        {QStringLiteral("Cat"), QStringLiteral("1"), false},
        {QStringLiteral("cat"), QStringLiteral("2"), true},
    }));

    QCOMPARE(decoded.size(), 2);
    QCOMPARE(decoded.at(0).emoji, QStringLiteral("1"));
    QCOMPARE(decoded.at(1).emoji, QStringLiteral("2"));
}

void TriggerWordsTest::encodeOmitsUnrepresentableFields()
{
    // The line format has no escaping, so these words cannot be decoded again.
    QCOMPARE(encodeTriggerWords({
                 {QStringLiteral("a|b"), QStringLiteral("x"), false},
                 {QStringLiteral("ok"), QStringLiteral("y"), false},
             }),
             QStringLiteral("ok|y|0"));
    QVERIFY(encodeTriggerWords({{QStringLiteral("ok"), QStringLiteral("x|y"), false}})
                .isEmpty());
    QVERIFY(encodeTriggerWords({{QStringLiteral("a\nb"), QStringLiteral("x"), false}})
                .isEmpty());
}

void TriggerWordsTest::encodeEmptyList()
{
    QVERIFY(encodeTriggerWords(QVector<TriggerWord>()).isEmpty());
    QVERIFY(encodeTriggerWords({{QString(), QString(), false}}).isEmpty());
}

void TriggerWordsTest::decodeRoundTripsEncodedWords()
{
    const QVector<TriggerWord> words = {
        {QStringLiteral("brb"), waveEmoji(), false},
        {QStringLiteral("Срочно"), fireEmoji(), true},
        {QStringLiteral("release day"), QString(), false},
    };

    QCOMPARE(decodeTriggerWords(encodeTriggerWords(words)), words);
}

void TriggerWordsTest::decodeIgnoresMalformedLines()
{
    const QString encoded = QStringLiteral("ok|a|0\n"
                                           "broken\n"
                                           "two|fields\n"
                                           "bad|b|2\n"
                                           "empty|e|\n"
                                           "extra|x|0|tail\n"
                                           "|no-text|1\n"
                                           "   |no-text|1\n"
                                           "last|c|1");

    const QVector<TriggerWord> decoded = decodeTriggerWords(encoded);
    QCOMPARE(decoded.size(), 2);
    QCOMPARE(decoded.at(0), (TriggerWord{QStringLiteral("ok"), QStringLiteral("a"), false}));
    QCOMPARE(decoded.at(1), (TriggerWord{QStringLiteral("last"), QStringLiteral("c"), true}));
}

void TriggerWordsTest::decodeAcceptsCrlf()
{
    const QVector<TriggerWord> expected = {
        {QStringLiteral("a"), QStringLiteral("1"), false},
        {QStringLiteral("b"), QStringLiteral("2"), true},
    };

    QCOMPARE(decodeTriggerWords(QStringLiteral("a|1|0\r\nb|2|1\r\n")), expected);
}

void TriggerWordsTest::decodeEmptyInput()
{
    QVERIFY(decodeTriggerWords(QString()).isEmpty());
    QVERIFY(decodeTriggerWords(QStringLiteral("\n\n")).isEmpty());
}

void TriggerWordsTest::decodeKeepsEmptyEmoji()
{
    QCOMPARE(decodeTriggerWords(QStringLiteral("del||0")),
             QVector<TriggerWord>({TriggerWord{QStringLiteral("del"), QString(), false}}));
}

void TriggerWordsTest::applyReplacesEveryOccurrence()
{
    QCOMPARE(applyTriggerWords(QStringLiteral("brb now, brb later"),
                               {{QStringLiteral("brb"), QStringLiteral("be right back")}}),
             QStringLiteral("be right back now, be right back later"));
}

void TriggerWordsTest::applyLongestMatchWins()
{
    const QVector<TriggerWord> words = {
        {QStringLiteral("a"), QStringLiteral("short"), false},
        {QStringLiteral("abc"), QStringLiteral("long"), false},
        {QStringLiteral("ab"), QStringLiteral("medium"), false},
    };

    QCOMPARE(applyTriggerWords(QStringLiteral("abc!"), words), QStringLiteral("long!"));
    QCOMPARE(applyTriggerWords(QStringLiteral("ab!"), words), QStringLiteral("medium!"));
    QCOMPARE(applyTriggerWords(QStringLiteral("a!"), words), QStringLiteral("short!"));

    // A longer word that would extend past a letter or digit does not match.
    QCOMPARE(applyTriggerWords(QStringLiteral("abcd"), words), QStringLiteral("abcd"));
}

void TriggerWordsTest::applyIgnoresCaseByDefault()
{
    const QVector<TriggerWord> words = {{QStringLiteral("brb"), waveEmoji(), false}};
    QCOMPARE(applyTriggerWords(QStringLiteral("BRB brb Brb"), words),
             waveEmoji() + QStringLiteral(" ") + waveEmoji() + QStringLiteral(" ")
                 + waveEmoji());

    QCOMPARE(applyTriggerWords(QStringLiteral("ПРИВЕТ"),
                               {{QStringLiteral("привет"), QStringLiteral("x")}}),
             QStringLiteral("x"));
}

void TriggerWordsTest::applyHonoursCaseSensitivity()
{
    const QVector<TriggerWord> words = {
        {QStringLiteral("Brb"), QStringLiteral("x"), true},
    };

    QCOMPARE(applyTriggerWords(QStringLiteral("Brb brb BRB"), words),
             QStringLiteral("x brb BRB"));
}

void TriggerWordsTest::applyRespectsAsciiWordBoundaries()
{
    const QVector<TriggerWord> words = {{QStringLiteral("cat"), QStringLiteral("dog")}};

    QCOMPARE(applyTriggerWords(QStringLiteral("cat"), words), QStringLiteral("dog"));
    QCOMPARE(applyTriggerWords(QStringLiteral("cat!"), words), QStringLiteral("dog!"));
    QCOMPARE(applyTriggerWords(QStringLiteral("(cat)"), words), QStringLiteral("(dog)"));
    QCOMPARE(applyTriggerWords(QStringLiteral("concatenate"), words),
             QStringLiteral("concatenate"));
    QCOMPARE(applyTriggerWords(QStringLiteral("cat1"), words), QStringLiteral("cat1"));
    QCOMPARE(applyTriggerWords(QStringLiteral("1cat"), words), QStringLiteral("1cat"));

    // Underscore is neither an ASCII letter nor a digit, so it is a boundary.
    QCOMPARE(applyTriggerWords(QStringLiteral("cat_1"), words), QStringLiteral("dog_1"));
    QCOMPARE(applyTriggerWords(QStringLiteral("a cat-like animal"), words),
             QStringLiteral("a dog-like animal"));

    const QVector<TriggerWord> phrase = {
        {QStringLiteral("release day"), QStringLiteral("x"), false},
    };
    QCOMPARE(applyTriggerWords(QStringLiteral("release day!"), phrase), QStringLiteral("x!"));
    QCOMPARE(applyTriggerWords(QStringLiteral("release days"), phrase),
             QStringLiteral("release days"));
}

void TriggerWordsTest::applyMatchesAtNonAsciiAndPunctuation()
{
    const QVector<TriggerWord> words = {{QStringLiteral("cat"), QStringLiteral("dog")}};

    // Only ASCII letters and digits extend a word.
    QCOMPARE(applyTriggerWords(QStringLiteral("caté"), words), QStringLiteral("dogé"));
    QCOMPARE(applyTriggerWords(QStringLiteral("écat"), words), QStringLiteral("édog"));
}

void TriggerWordsTest::applyTrimsAndDeduplicatesWords()
{
    QCOMPARE(applyTriggerWords(QStringLiteral("hi"),
                               {{QStringLiteral("  hi  "), QStringLiteral("first")},
                                {QStringLiteral("hi"), QStringLiteral("second")}}),
             QStringLiteral("first"));
}

void TriggerWordsTest::applyDoesNotCascade()
{
    const QVector<TriggerWord> words = {
        {QStringLiteral("a"), QStringLiteral("b"), false},
        {QStringLiteral("b"), QStringLiteral("c"), false},
    };

    QCOMPARE(applyTriggerWords(QStringLiteral("a"), words), QStringLiteral("b"));
    QCOMPARE(applyTriggerWords(QStringLiteral("b"), words), QStringLiteral("c"));
    QCOMPARE(applyTriggerWords(QStringLiteral("a b"), words), QStringLiteral("b c"));

    QCOMPARE(applyTriggerWords(QStringLiteral("x"),
                               {{QStringLiteral("x"), QStringLiteral("y")},
                                {QStringLiteral("y"), QStringLiteral("z")}}),
             QStringLiteral("y"));
}

void TriggerWordsTest::applyDoesNotRescanItsOwnOutput()
{
    const QVector<TriggerWord> words = {{QStringLiteral("ab"), QStringLiteral("abab")}};

    QCOMPARE(applyTriggerWords(QStringLiteral("ab"), words), QStringLiteral("abab"));
    QCOMPARE(applyTriggerWords(QStringLiteral("ab ab"), words), QStringLiteral("abab abab"));
}

void TriggerWordsTest::applyEmptyInputs()
{
    QVERIFY(applyTriggerWords(QString(), {{QStringLiteral("a"), QStringLiteral("b")}})
                .isEmpty());
    QCOMPARE(applyTriggerWords(QStringLiteral("unchanged"), QVector<TriggerWord>()),
             QStringLiteral("unchanged"));
    QCOMPARE(applyTriggerWords(QStringLiteral("unchanged"),
                               {{QStringLiteral("   "), QStringLiteral("x")},
                                {QString(), QStringLiteral("y")}}),
             QStringLiteral("unchanged"));
}

void TriggerWordsTest::applyPreservesUnmatchedSurrogatePairs()
{
    QCOMPARE(applyTriggerWords(QStringLiteral("say hi"), {{QStringLiteral("hi"), waveEmoji()}}),
             QStringLiteral("say ") + waveEmoji());
    QCOMPARE(applyTriggerWords(QStringLiteral("a") + waveEmoji(),
                               {{QStringLiteral("a"), QStringLiteral("b")}}),
             QStringLiteral("b") + waveEmoji());
}

QTEST_GUILESS_MAIN(TriggerWordsTest)
#include "tst_trigger_words.moc"
