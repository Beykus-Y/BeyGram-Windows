#include "telegram/TextReplacementModel.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

class TextReplacementModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void appliesSingleRule();
    void appliesEveryOccurrence();
    void dropsEmptyNeedles();
    void keepsFirstRuleForRepeatedNeedle();
    void longestNeedleWinsAtSameOffset();
    void longestNeedleWinsRegardlessOfSuppliedOrder();
    void doesNotRescanSubstitutedText();
    void doesNotCascadeIntoItsOwnOutput();
    void isCaseSensitive();
    void leavesTextWithoutNeedleUnchanged();
    void returnsEmptyForEmptyText();
    void allowsRulesThatDeleteText();
    void handlesUnicodeNeedles();
    void reportsEffectiveRuleCount();
    void reportsRulesInSuppliedOrder();
    void emitsChangedForNewRules();
    void doesNotEmitWhenRulesAreUnchanged();
    void doesNotEmitWhenNewRulesNormaliseAway();
    void replacesPreviousRules();
    void clearEmptiesRulesAndEmits();
    void clearIsNoOpWhenEmpty();
    void applyDoesNotModifyStoredRules();
};

void TextReplacementModelTest::startsEmpty()
{
    TextReplacementModel model;

    QVERIFY(model.rules().isEmpty());
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.apply(QStringLiteral("hello")), QStringLiteral("hello"));
}

void TextReplacementModelTest::appliesSingleRule()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("brb"), QStringLiteral("be right back")}});

    QCOMPARE(model.apply(QStringLiteral("ok brb")), QStringLiteral("ok be right back"));
}

void TextReplacementModelTest::appliesEveryOccurrence()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("aa"), QStringLiteral("b")}});

    // Adjacent needles are separate matches; consuming "aa" leaves the next
    // "aa" intact for the same pass.
    QCOMPARE(model.apply(QStringLiteral("aaaa")), QStringLiteral("bb"));
    QCOMPARE(model.apply(QStringLiteral("aaxaa")), QStringLiteral("bxb"));
}

void TextReplacementModelTest::dropsEmptyNeedles()
{
    TextReplacementModel model;
    model.setRules({
        {QString(), QStringLiteral("ignored")},
        {QStringLiteral("x"), QStringLiteral("y")},
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rules().size(), 1);
    QCOMPARE(model.rules().at(0).from, QStringLiteral("x"));
    QCOMPARE(model.apply(QStringLiteral("x")), QStringLiteral("y"));
}

void TextReplacementModelTest::keepsFirstRuleForRepeatedNeedle()
{
    TextReplacementModel model;
    model.setRules({
        {QStringLiteral("a"), QStringLiteral("first")},
        {QStringLiteral("a"), QStringLiteral("second")},
        {QStringLiteral("a"), QStringLiteral("third")},
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.apply(QStringLiteral("a")), QStringLiteral("first"));
}

void TextReplacementModelTest::longestNeedleWinsAtSameOffset()
{
    TextReplacementModel model;
    model.setRules({
        {QStringLiteral("ab"), QStringLiteral("short")},
        {QStringLiteral("abc"), QStringLiteral("long")},
    });

    QCOMPARE(model.apply(QStringLiteral("abc")), QStringLiteral("long"));
    QCOMPARE(model.apply(QStringLiteral("abd")), QStringLiteral("shortd"));
}

void TextReplacementModelTest::longestNeedleWinsRegardlessOfSuppliedOrder()
{
    TextReplacementModel model;
    model.setRules({
        {QStringLiteral("abc"), QStringLiteral("long")},
        {QStringLiteral("a"), QStringLiteral("short")},
        {QStringLiteral("ab"), QStringLiteral("medium")},
    });

    QCOMPARE(model.apply(QStringLiteral("abc")), QStringLiteral("long"));
    QCOMPARE(model.apply(QStringLiteral("ab")), QStringLiteral("medium"));
    QCOMPARE(model.apply(QStringLiteral("a")), QStringLiteral("short"));
}

void TextReplacementModelTest::doesNotRescanSubstitutedText()
{
    TextReplacementModel model;
    model.setRules({
        {QStringLiteral("a"), QStringLiteral("b")},
        {QStringLiteral("b"), QStringLiteral("c")},
    });

    // "a" becomes "b" and stops there: the freshly written "b" is never looked
    // at again, even though another rule would match it.
    QCOMPARE(model.apply(QStringLiteral("a")), QStringLiteral("b"));
    QCOMPARE(model.apply(QStringLiteral("ab")), QStringLiteral("bc"));
}

void TextReplacementModelTest::doesNotCascadeIntoItsOwnOutput()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("ab"), QStringLiteral("abab")}});

    QCOMPARE(model.apply(QStringLiteral("ab")), QStringLiteral("abab"));
    QCOMPARE(model.apply(QStringLiteral("abab")), QStringLiteral("abababab"));
}

void TextReplacementModelTest::isCaseSensitive()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("Brb"), QStringLiteral("be right back")}});

    QCOMPARE(model.apply(QStringLiteral("Brb")), QStringLiteral("be right back"));
    QCOMPARE(model.apply(QStringLiteral("brb")), QStringLiteral("brb"));
    QCOMPARE(model.apply(QStringLiteral("BRB")), QStringLiteral("BRB"));
}

void TextReplacementModelTest::leavesTextWithoutNeedleUnchanged()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("zzz"), QStringLiteral("y")}});

    const QString text = QStringLiteral("no needle here, line two\nand a third");
    QCOMPARE(model.apply(text), text);
}

void TextReplacementModelTest::returnsEmptyForEmptyText()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("a"), QStringLiteral("b")}});

    QVERIFY(model.apply(QString()).isEmpty());
}

void TextReplacementModelTest::allowsRulesThatDeleteText()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral(" "), QString()}});

    QCOMPARE(model.apply(QStringLiteral("a b c")), QStringLiteral("abc"));
}

void TextReplacementModelTest::handlesUnicodeNeedles()
{
    // U+1F642 is a surrogate pair in QString, U+00E9 a single unit; both are
    // matched as whole needles.
    const QString smiley = QString::fromUtf8("\xF0\x9F\x99\x82");
    const QString eAcute = QString::fromUtf8("\xC3\xA9");

    TextReplacementModel model;
    model.setRules({
        {smiley, QStringLiteral("smile")},
        {eAcute, QStringLiteral("e")},
    });

    QCOMPARE(model.apply(QStringLiteral("hi ") + smiley), QStringLiteral("hi smile"));
    QCOMPARE(model.apply(QStringLiteral("caf") + eAcute), QStringLiteral("cafe"));
}

void TextReplacementModelTest::reportsEffectiveRuleCount()
{
    TextReplacementModel model;
    model.setRules({
        {QStringLiteral("a"), QStringLiteral("1")},
        {QStringLiteral("b"), QStringLiteral("2")},
        {QStringLiteral("a"), QStringLiteral("3")},
        {QString(), QStringLiteral("4")},
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.rules().size(), 2);
}

void TextReplacementModelTest::reportsRulesInSuppliedOrder()
{
    TextReplacementModel model;
    model.setRules({
        {QStringLiteral("ccc"), QStringLiteral("1")},
        {QStringLiteral("a"), QStringLiteral("2")},
        {QStringLiteral("bb"), QStringLiteral("3")},
    });

    QStringList needles;
    for (const ReplacementRule& rule : model.rules())
        needles.append(rule.from);

    QCOMPARE(needles, QStringList({QStringLiteral("ccc"), QStringLiteral("a"),
                                   QStringLiteral("bb")}));
}

void TextReplacementModelTest::emitsChangedForNewRules()
{
    TextReplacementModel model;
    QSignalSpy spy(&model, &TextReplacementModel::changed);

    model.setRules({{QStringLiteral("a"), QStringLiteral("b")}});

    QCOMPARE(spy.count(), 1);
}

void TextReplacementModelTest::doesNotEmitWhenRulesAreUnchanged()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("a"), QStringLiteral("b")}});

    QSignalSpy spy(&model, &TextReplacementModel::changed);
    model.setRules({{QStringLiteral("a"), QStringLiteral("b")}});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void TextReplacementModelTest::doesNotEmitWhenNewRulesNormaliseAway()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("a"), QStringLiteral("b")}});

    QSignalSpy spy(&model, &TextReplacementModel::changed);

    // A repeated needle keeps the first rule, so nothing changes.
    model.setRules({
        {QStringLiteral("a"), QStringLiteral("b")},
        {QStringLiteral("a"), QStringLiteral("other")},
    });
    QCOMPARE(spy.count(), 0);

    // An empty needle is not a rule either.
    model.setRules({
        {QStringLiteral("a"), QStringLiteral("b")},
        {QString(), QStringLiteral("ignored")},
    });
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void TextReplacementModelTest::replacesPreviousRules()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("a"), QStringLiteral("b")}});
    model.setRules({{QStringLiteral("x"), QStringLiteral("y")}});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.apply(QStringLiteral("a")), QStringLiteral("a"));
    QCOMPARE(model.apply(QStringLiteral("x")), QStringLiteral("y"));
}

void TextReplacementModelTest::clearEmptiesRulesAndEmits()
{
    TextReplacementModel model;
    model.setRules({{QStringLiteral("a"), QStringLiteral("b")}});
    QSignalSpy spy(&model, &TextReplacementModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.rules().isEmpty());
    QCOMPARE(model.apply(QStringLiteral("a")), QStringLiteral("a"));
}

void TextReplacementModelTest::clearIsNoOpWhenEmpty()
{
    TextReplacementModel model;
    QSignalSpy spy(&model, &TextReplacementModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 0);
}

void TextReplacementModelTest::applyDoesNotModifyStoredRules()
{
    TextReplacementModel model;
    const QVector<ReplacementRule> rules = {
        {QStringLiteral("ab"), QStringLiteral("short")},
        {QStringLiteral("abc"), QStringLiteral("long")},
    };
    model.setRules(rules);

    QSignalSpy spy(&model, &TextReplacementModel::changed);
    QCOMPARE(model.apply(QStringLiteral("abc")), QStringLiteral("long"));

    // Applying sorts a private copy, so the stored order and the signal stay put.
    QCOMPARE(model.rules().size(), rules.size());
    QCOMPARE(model.rules().at(0).from, QStringLiteral("ab"));
    QCOMPARE(model.rules().at(1).from, QStringLiteral("abc"));
    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(TextReplacementModelTest)
#include "tst_text_replacement_model.moc"
