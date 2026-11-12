#include "telegram/PollComposerModel.h"

#include <QChar>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QString repeated(QChar character, int count)
{
    return QString(count, character);
}

/// U+1F642, a surrogate pair in QString.
QString smiley()
{
    return QString::fromUtf8("\xF0\x9F\x99\x82");
}

/// A question plus two options: the smallest composition that can be sent.
void fillMinimalPoll(PollComposerModel& model)
{
    model.setQuestion(QStringLiteral("Where do we eat?"));
    model.addOption(QStringLiteral("Pizza"));
    model.addOption(QStringLiteral("Sushi"));
}

} // namespace

class PollComposerModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmptyWithDefaults();
    void trimsQuestion();
    void capsQuestionAt255();
    void doesNotSplitSurrogatePairAtQuestionLimit();
    void questionIsUnchangedForEquivalentInput();
    void addOptionAppendsTrimmedText();
    void rejectsEmptyOption();
    void rejectsDuplicateOption();
    void comparesDuplicatesCaseSensitively();
    void capsOptionAt100();
    void acceptsTenOptionsAndRejectsTheEleventh();
    void removeOptionDropsTheIndexedOption();
    void removeOptionRejectsInvalidIndex();
    void removeOptionKeepsCorrectOptionInRange();
    void removeOptionResetsCorrectOptionThatFellOffTheEnd();
    void setOptionReplacesText();
    void setOptionRejectsInvalidInput();
    void setOptionIsQuietWhenTextIsUnchanged();
    void defaultIsAnonymousSingleAnswerRegular();
    void setAnonymousEmitsOnlyOnChange();
    void setMultipleEmitsOnlyOnChange();
    void turningQuizOffKeepsCorrectOptionAndExplanation();
    void setCorrectOptionIgnoresOutOfRangeIndex();
    void explanationIsTrimmedAndCapped();
    void validRequiresQuestion();
    void validRequiresTwoOptions();
    void validAcceptsRegularPoll();
    void validAcceptsQuizWithAnswerInRange();
    void buildCopiesComposition();
    void buildIsEmptyWhenInvalid();
    void buildLeavesCorrectOptionUnsetForRegularPoll();
    void buildDoesNotEmitChanged();
    void resetRestoresDefaults();
    void resetIsQuietWhenAlreadyDefault();
    void changedFiresOncePerRealChange();
};

void PollComposerModelTest::startsEmptyWithDefaults()
{
    PollComposerModel model;

    QVERIFY(model.question().isEmpty());
    QVERIFY(model.options().isEmpty());
    QVERIFY(model.anonymous());
    QVERIFY(!model.multiple());
    QVERIFY(!model.quiz());
    QCOMPARE(model.correctOption(), 0);
    QVERIFY(model.explanation().isEmpty());
    QVERIFY(!model.valid());
}

void PollComposerModelTest::trimsQuestion()
{
    PollComposerModel model;
    model.setQuestion(QStringLiteral("  Where do we eat?  "));

    QCOMPARE(model.question(), QStringLiteral("Where do we eat?"));
}

void PollComposerModelTest::capsQuestionAt255()
{
    PollComposerModel model;
    model.setQuestion(repeated(QChar(u'a'), 300));

    QCOMPARE(model.question().size(), 255);
}

void PollComposerModelTest::doesNotSplitSurrogatePairAtQuestionLimit()
{
    PollComposerModel model;
    // 254 units plus the pair is one unit over the cap; the pair is dropped
    // whole rather than being cut in half.
    model.setQuestion(repeated(QChar(u'a'), 254) + smiley());

    QCOMPARE(model.question().size(), 254);
    QCOMPARE(model.question(), repeated(QChar(u'a'), 254));

    // A string that fits exactly keeps the pair.
    model.setQuestion(repeated(QChar(u'a'), 253) + smiley());
    QCOMPARE(model.question().size(), 255);
    QVERIFY(model.question().endsWith(smiley()));
}

void PollComposerModelTest::questionIsUnchangedForEquivalentInput()
{
    PollComposerModel model;
    model.setQuestion(QStringLiteral("Q"));

    QSignalSpy spy(&model, &PollComposerModel::changed);
    model.setQuestion(QStringLiteral("  Q  "));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.question(), QStringLiteral("Q"));
}

void PollComposerModelTest::addOptionAppendsTrimmedText()
{
    PollComposerModel model;

    QVERIFY(model.addOption(QStringLiteral("  Pizza ")));
    QVERIFY(model.addOption(QStringLiteral("Sushi")));

    QCOMPARE(model.options(), QStringList({QStringLiteral("Pizza"), QStringLiteral("Sushi")}));
}

void PollComposerModelTest::rejectsEmptyOption()
{
    PollComposerModel model;
    QSignalSpy spy(&model, &PollComposerModel::changed);

    QVERIFY(!model.addOption(QString()));
    QVERIFY(!model.addOption(QStringLiteral("   ")));

    QVERIFY(model.options().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void PollComposerModelTest::rejectsDuplicateOption()
{
    PollComposerModel model;
    QVERIFY(model.addOption(QStringLiteral("Pizza")));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    QVERIFY(!model.addOption(QStringLiteral("Pizza")));
    QVERIFY(!model.addOption(QStringLiteral("  Pizza  ")));

    QCOMPARE(model.options().size(), 1);
    QCOMPARE(spy.count(), 0);
}

void PollComposerModelTest::comparesDuplicatesCaseSensitively()
{
    PollComposerModel model;
    QVERIFY(model.addOption(QStringLiteral("Pizza")));

    QVERIFY(model.addOption(QStringLiteral("pizza")));
    QVERIFY(model.addOption(QStringLiteral("PIZZA")));
    QCOMPARE(model.options().size(), 3);
}

void PollComposerModelTest::capsOptionAt100()
{
    PollComposerModel model;
    QVERIFY(model.addOption(repeated(QChar(u'b'), 120)));

    QCOMPARE(model.options().size(), 1);
    QCOMPARE(model.options().at(0).size(), 100);
}

void PollComposerModelTest::acceptsTenOptionsAndRejectsTheEleventh()
{
    PollComposerModel model;
    for (int i = 0; i < 10; ++i)
        QVERIFY(model.addOption(QStringLiteral("option %1").arg(i)));

    QSignalSpy spy(&model, &PollComposerModel::changed);
    QVERIFY(!model.addOption(QStringLiteral("option 10")));

    QCOMPARE(model.options().size(), 10);
    QCOMPARE(spy.count(), 0);
}

void PollComposerModelTest::removeOptionDropsTheIndexedOption()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    model.addOption(QStringLiteral("C"));

    QVERIFY(model.removeOption(1));

    QCOMPARE(model.options(), QStringList({QStringLiteral("A"), QStringLiteral("C")}));
}

void PollComposerModelTest::removeOptionRejectsInvalidIndex()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    QVERIFY(!model.removeOption(-1));
    QVERIFY(!model.removeOption(1));

    QCOMPARE(model.options(), QStringList({QStringLiteral("A")}));
    QCOMPARE(spy.count(), 0);
}

void PollComposerModelTest::removeOptionKeepsCorrectOptionInRange()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    model.addOption(QStringLiteral("C"));
    model.addOption(QStringLiteral("D"));
    model.setCorrectOption(2);
    QSignalSpy spy(&model, &PollComposerModel::changed);

    // An answer that still fits keeps its index after an earlier removal.
    QVERIFY(model.removeOption(0));

    QCOMPARE(model.options().size(), 3);
    QCOMPARE(model.correctOption(), 2);
    QCOMPARE(spy.count(), 1);
}

void PollComposerModelTest::removeOptionResetsCorrectOptionThatFellOffTheEnd()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    model.addOption(QStringLiteral("C"));
    model.setCorrectOption(2);
    QSignalSpy spy(&model, &PollComposerModel::changed);

    QVERIFY(model.removeOption(2));

    QCOMPARE(model.options().size(), 2);
    QCOMPARE(model.correctOption(), 0);
    QCOMPARE(spy.count(), 1);

    // Removing everything still leaves a sane index.
    QVERIFY(model.removeOption(1));
    QVERIFY(model.removeOption(0));
    QCOMPARE(model.correctOption(), 0);
    QVERIFY(model.options().isEmpty());
}

void PollComposerModelTest::setOptionReplacesText()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    QVERIFY(model.setOption(1, QStringLiteral("  Ramen ")));

    QCOMPARE(model.options(), QStringList({QStringLiteral("A"), QStringLiteral("Ramen")}));
    QCOMPARE(spy.count(), 1);
}

void PollComposerModelTest::setOptionRejectsInvalidInput()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    QVERIFY(!model.setOption(-1, QStringLiteral("X")));
    QVERIFY(!model.setOption(2, QStringLiteral("X")));
    QVERIFY(!model.setOption(0, QString()));
    QVERIFY(!model.setOption(0, QStringLiteral("   ")));
    // "B" is already used at index 1.
    QVERIFY(!model.setOption(0, QStringLiteral(" B ")));

    QCOMPARE(model.options(), QStringList({QStringLiteral("A"), QStringLiteral("B")}));
    QCOMPARE(spy.count(), 0);
}

void PollComposerModelTest::setOptionIsQuietWhenTextIsUnchanged()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    QVERIFY(model.setOption(0, QStringLiteral("  A ")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.options(), QStringList({QStringLiteral("A")}));
}

void PollComposerModelTest::defaultIsAnonymousSingleAnswerRegular()
{
    PollComposerModel model;

    QVERIFY(model.anonymous());
    QVERIFY(!model.multiple());
    QVERIFY(!model.quiz());
}

void PollComposerModelTest::setAnonymousEmitsOnlyOnChange()
{
    PollComposerModel model;
    QSignalSpy spy(&model, &PollComposerModel::changed);

    model.setAnonymous(true);
    QCOMPARE(spy.count(), 0);

    model.setAnonymous(false);
    QVERIFY(!model.anonymous());
    QCOMPARE(spy.count(), 1);

    model.setAnonymous(false);
    QCOMPARE(spy.count(), 1);
}

void PollComposerModelTest::setMultipleEmitsOnlyOnChange()
{
    PollComposerModel model;
    QSignalSpy spy(&model, &PollComposerModel::changed);

    model.setMultiple(false);
    QCOMPARE(spy.count(), 0);

    model.setMultiple(true);
    QVERIFY(model.multiple());
    QCOMPARE(spy.count(), 1);

    model.setMultiple(true);
    QCOMPARE(spy.count(), 1);
}

void PollComposerModelTest::turningQuizOffKeepsCorrectOptionAndExplanation()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    model.setQuiz(true);
    model.setCorrectOption(1);
    model.setExplanation(QStringLiteral("because"));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    model.setQuiz(false);

    QVERIFY(!model.quiz());
    QCOMPARE(model.correctOption(), 1);
    QCOMPARE(model.explanation(), QStringLiteral("because"));
    QCOMPARE(spy.count(), 1);

    model.setQuiz(true);
    QVERIFY(model.quiz());
    QCOMPARE(model.correctOption(), 1);
    QCOMPARE(model.explanation(), QStringLiteral("because"));
}

void PollComposerModelTest::setCorrectOptionIgnoresOutOfRangeIndex()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    model.setCorrectOption(5);
    QCOMPARE(model.correctOption(), 0);
    model.setCorrectOption(-1);
    QCOMPARE(model.correctOption(), 0);
    QCOMPARE(spy.count(), 0);

    model.setCorrectOption(1);
    QCOMPARE(model.correctOption(), 1);
    QCOMPARE(spy.count(), 1);

    model.setCorrectOption(1);
    QCOMPARE(spy.count(), 1);
}

void PollComposerModelTest::explanationIsTrimmedAndCapped()
{
    PollComposerModel model;
    model.setExplanation(QStringLiteral("  because  "));
    QCOMPARE(model.explanation(), QStringLiteral("because"));

    model.setExplanation(repeated(QChar(u'x'), 250));
    QCOMPARE(model.explanation().size(), 200);
}

void PollComposerModelTest::validRequiresQuestion()
{
    PollComposerModel model;
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));

    QVERIFY(!model.valid());

    model.setQuestion(QStringLiteral("   "));
    QVERIFY(!model.valid());

    model.setQuestion(QStringLiteral("Pick one"));
    QVERIFY(model.valid());
}

void PollComposerModelTest::validRequiresTwoOptions()
{
    PollComposerModel model;
    model.setQuestion(QStringLiteral("Pick one"));

    QVERIFY(!model.valid());

    model.addOption(QStringLiteral("A"));
    QVERIFY(!model.valid());

    model.addOption(QStringLiteral("B"));
    QVERIFY(model.valid());
}

void PollComposerModelTest::validAcceptsRegularPoll()
{
    PollComposerModel model;
    fillMinimalPoll(model);
    model.setMultiple(true);
    model.setAnonymous(false);

    QVERIFY(model.valid());
}

void PollComposerModelTest::validAcceptsQuizWithAnswerInRange()
{
    PollComposerModel model;
    model.setQuestion(QStringLiteral("Pick one"));
    model.addOption(QStringLiteral("A"));
    model.addOption(QStringLiteral("B"));
    model.setQuiz(true);

    // The default answer is the first option, which is inside the list.
    QVERIFY(model.valid());
    QCOMPARE(model.correctOption(), 0);

    model.setCorrectOption(1);
    QVERIFY(model.valid());

    // Dropping the answered option still leaves a sendable quiz: the answer is
    // clamped back into range.
    model.addOption(QStringLiteral("C"));
    model.setCorrectOption(2);
    QVERIFY(model.removeOption(2));

    QCOMPARE(model.options().size(), 2);
    QCOMPARE(model.correctOption(), 0);
    QVERIFY(model.valid());
}

void PollComposerModelTest::buildCopiesComposition()
{
    PollComposerModel model;
    fillMinimalPoll(model);
    model.setAnonymous(false);
    model.setMultiple(true);
    model.setQuiz(true);
    model.setCorrectOption(1);
    model.setExplanation(QStringLiteral("  sushi  "));

    const OutgoingPoll poll = model.build();

    QCOMPARE(poll.question, QStringLiteral("Where do we eat?"));
    QCOMPARE(poll.options, QVector<QString>({QStringLiteral("Pizza"), QStringLiteral("Sushi")}));
    QVERIFY(!poll.anonymous);
    QVERIFY(poll.multiple);
    QVERIFY(poll.quiz);
    QCOMPARE(poll.correctOption, 1);
    QCOMPARE(poll.explanation, QStringLiteral("sushi"));
}

void PollComposerModelTest::buildIsEmptyWhenInvalid()
{
    PollComposerModel model;
    model.setQuestion(QStringLiteral("Only one answer"));
    model.addOption(QStringLiteral("A"));
    model.setQuiz(true);
    model.setExplanation(QStringLiteral("because"));

    const OutgoingPoll poll = model.build();

    QVERIFY(poll.question.isEmpty());
    QVERIFY(poll.options.isEmpty());
    // The rest of the composition is still reported, so a caller can show it.
    QVERIFY(poll.quiz);
    QCOMPARE(poll.explanation, QStringLiteral("because"));
}

void PollComposerModelTest::buildLeavesCorrectOptionUnsetForRegularPoll()
{
    PollComposerModel model;
    fillMinimalPoll(model);
    model.setCorrectOption(1);
    QVERIFY(!model.quiz());

    const OutgoingPoll poll = model.build();

    QVERIFY(!poll.quiz);
    QCOMPARE(poll.correctOption, -1);
}

void PollComposerModelTest::buildDoesNotEmitChanged()
{
    PollComposerModel model;
    fillMinimalPoll(model);
    QSignalSpy spy(&model, &PollComposerModel::changed);

    const OutgoingPoll poll = model.build();

    QVERIFY(!poll.question.isEmpty());
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.options().size(), 2);
}

void PollComposerModelTest::resetRestoresDefaults()
{
    PollComposerModel model;
    fillMinimalPoll(model);
    model.setAnonymous(false);
    model.setMultiple(true);
    model.setQuiz(true);
    model.setCorrectOption(1);
    model.setExplanation(QStringLiteral("because"));
    QSignalSpy spy(&model, &PollComposerModel::changed);

    model.reset();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.question().isEmpty());
    QVERIFY(model.options().isEmpty());
    QVERIFY(model.anonymous());
    QVERIFY(!model.multiple());
    QVERIFY(!model.quiz());
    QCOMPARE(model.correctOption(), 0);
    QVERIFY(model.explanation().isEmpty());
    QVERIFY(!model.valid());
}

void PollComposerModelTest::resetIsQuietWhenAlreadyDefault()
{
    PollComposerModel model;
    QSignalSpy spy(&model, &PollComposerModel::changed);

    model.reset();
    model.reset();

    QCOMPARE(spy.count(), 0);
}

void PollComposerModelTest::changedFiresOncePerRealChange()
{
    PollComposerModel model;
    QSignalSpy spy(&model, &PollComposerModel::changed);

    model.setQuestion(QStringLiteral("Q"));   // 1
    model.setQuestion(QStringLiteral("Q"));   // no change
    model.addOption(QStringLiteral("A"));     // 2
    model.addOption(QStringLiteral("A"));     // rejected
    model.setAnonymous(true);                 // no change
    model.setMultiple(true);                  // 3
    model.setCorrectOption(0);                // no change
    model.setOption(0, QStringLiteral("B"));  // 4
    model.removeOption(5);                    // rejected
    model.setExplanation(QStringLiteral("why")); // 5

    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(PollComposerModelTest)
#include "tst_poll_composer_model.moc"
