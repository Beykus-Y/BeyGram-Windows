#include "ui/CallHistoryFacade.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QList>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <initializer_list>

using beygram::ui::CallHistoryFacade;

namespace {

/// Builds a parallel input list without repeating QStringLiteral for every
/// field; the facade only ever receives strings from QML, so the tests do too.
QStringList strings(std::initializer_list<const char*> values)
{
    QStringList list;
    list.reserve(static_cast<int>(values.size()));
    for (const char* value : values)
        list.append(QString::fromLatin1(value));
    return list;
}

/// The JSON object behind one prepared entry, or an empty object when the
/// index is out of range.
QJsonObject entryAt(const CallHistoryFacade& facade, int index)
{
    const QStringList lines = facade.entries();
    if (index < 0 || index >= lines.size())
        return QJsonObject();

    return QJsonDocument::fromJson(lines.at(index).toUtf8()).object();
}

QString field(const QJsonObject& entry, const char* key)
{
    return entry.value(QString::fromLatin1(key)).toString();
}

int number(const QJsonObject& entry, const char* key)
{
    return entry.value(QString::fromLatin1(key)).toInt();
}

} // namespace

class CallHistoryFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void preparesOneEntry();
    void shortListsUseDefaults();
    void dropsMalformedIds();
    void keepsFirstRecordOfAnId();
    void clampsDurations();
    void sortsNewestFirstThenByIdDescending();
    void unknownDatesSortLast();
    void readsOutcomeTokens();
    void blankNamesBecomeUnknown();
    void trimsTheIdForCallback();
    void countsMissedCalls();
    void replacesInsteadOfAppending();
    void emptyIdsClearTheList();
    void repeatedInputIsSilent();
    void clearEmptiesTheList();
    void clearIsNoOpWhenEmpty();
    void keepsAwkwardNamesIntact();
    void entriesAreCompactJsonObjects();
};

void CallHistoryFacadeTest::startsEmpty()
{
    const CallHistoryFacade facade;

    QVERIFY(facade.entries().isEmpty());
    QCOMPARE(facade.missedCount(), 0);
    QVERIFY(facade.empty());
}

void CallHistoryFacadeTest::preparesOneEntry()
{
    CallHistoryFacade facade;
    QSignalSpy changed(&facade, &CallHistoryFacade::changed);

    facade.setRecords(strings({"42"}), strings({"Ada"}), strings({"1763030400"}),
                      strings({"75"}), strings({"0"}));

    QCOMPARE(changed.count(), 1);
    QVERIFY(!facade.empty());
    QCOMPARE(facade.entries().size(), 1);

    const QJsonObject entry = entryAt(facade, 0);
    QVERIFY(!entry.isEmpty());
    QCOMPARE(field(entry, "id"), QStringLiteral("42"));
    QCOMPARE(field(entry, "name"), QStringLiteral("Ada"));
    QCOMPARE(entry.value(QStringLiteral("outgoing")).toBool(), false);
    QCOMPARE(entry.value(QStringLiteral("missed")).toBool(), false);
    QCOMPARE(static_cast<qint64>(entry.value(QStringLiteral("date")).toDouble()),
             static_cast<qint64>(1763030400));
    QCOMPARE(number(entry, "duration"), 75);
    QCOMPARE(field(entry, "durationLabel"), QStringLiteral("01:15"));
}

void CallHistoryFacadeTest::shortListsUseDefaults()
{
    CallHistoryFacade facade;

    // Only the first call has a name, a date, a duration and an outcome.
    facade.setRecords(strings({"7", "8"}), strings({"Ada"}), strings({"1763030400"}),
                      strings({"30"}), strings({"1"}));

    QCOMPARE(facade.entries().size(), 2);

    // The dated call sorts first; the other one keeps every default.
    const QJsonObject dated = entryAt(facade, 0);
    QCOMPARE(field(dated, "id"), QStringLiteral("7"));
    QCOMPARE(field(dated, "name"), QStringLiteral("Ada"));
    QCOMPARE(number(dated, "duration"), 30);
    QCOMPARE(dated.value(QStringLiteral("missed")).toBool(), true);

    const QJsonObject bare = entryAt(facade, 1);
    QCOMPARE(field(bare, "id"), QStringLiteral("8"));
    QCOMPARE(field(bare, "name"), QStringLiteral("Unknown"));
    QCOMPARE(number(bare, "duration"), 0);
    QCOMPARE(field(bare, "durationLabel"), QStringLiteral("00:00"));
    QCOMPARE(static_cast<qint64>(bare.value(QStringLiteral("date")).toDouble()),
             static_cast<qint64>(0));
    QCOMPARE(bare.value(QStringLiteral("outgoing")).toBool(), false);
    QCOMPARE(bare.value(QStringLiteral("missed")).toBool(), false);
    QCOMPARE(facade.missedCount(), 1);
}

void CallHistoryFacadeTest::dropsMalformedIds()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"", "0", "-2", "abc", " 9 "}),
                      strings({"Blank", "Zero", "Negative", "Text", "Nine"}),
                      strings({"500", "400", "300", "200", "100"}),
                      strings({"10", "20", "30", "40", "50"}),
                      strings({"0", "0", "0", "0", "0"}));

    // A call without a positive id could never call back, so it is dropped
    // before it can occupy a row.
    QCOMPARE(facade.entries().size(), 1);
    QCOMPARE(field(entryAt(facade, 0), "id"), QStringLiteral("9"));
    QCOMPARE(field(entryAt(facade, 0), "name"), QStringLiteral("Nine"));
}

void CallHistoryFacadeTest::keepsFirstRecordOfAnId()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"7", "7", "8"}),
                      strings({"First", "Second", "Other"}),
                      strings({"300", "200", "100"}),
                      strings({"10", "20", "30"}),
                      strings({"0", "0", "0"}));

    QCOMPARE(facade.entries().size(), 2);
    QCOMPARE(field(entryAt(facade, 0), "id"), QStringLiteral("7"));
    QCOMPARE(field(entryAt(facade, 0), "name"), QStringLiteral("First"));
    QCOMPARE(number(entryAt(facade, 0), "duration"), 10);
    QCOMPARE(field(entryAt(facade, 1), "id"), QStringLiteral("8"));
}

void CallHistoryFacadeTest::clampsDurations()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"1", "2", "3", "4"}),
                      strings({"A", "B", "C", "D"}),
                      strings({"400", "300", "200", "100"}),
                      strings({"-5", "0", "3725", "abc"}),
                      strings({"0", "0", "0", "0"}));

    QCOMPARE(facade.entries().size(), 4);

    // Newest first: 1, 2, 3, 4.
    QCOMPARE(number(entryAt(facade, 0), "duration"), 0);
    QCOMPARE(field(entryAt(facade, 0), "durationLabel"), QStringLiteral("00:00"));

    QCOMPARE(number(entryAt(facade, 1), "duration"), 0);

    // A call longer than 99 minutes keeps counting instead of wrapping.
    QCOMPARE(number(entryAt(facade, 2), "duration"), 3725);
    QCOMPARE(field(entryAt(facade, 2), "durationLabel"), QStringLiteral("62:05"));

    // An unparsable duration reads as 0, never as a rejected call.
    QCOMPARE(number(entryAt(facade, 3), "duration"), 0);
}

void CallHistoryFacadeTest::sortsNewestFirstThenByIdDescending()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"10", "11", "12"}),
                      strings({"Ten", "Eleven", "Twelve"}),
                      strings({"100", "300", "300"}),
                      strings({"0", "0", "0"}),
                      strings({"0", "0", "0"}));

    // 11 and 12 share a date, so the larger id comes first.
    QCOMPARE(field(entryAt(facade, 0), "id"), QStringLiteral("12"));
    QCOMPARE(field(entryAt(facade, 1), "id"), QStringLiteral("11"));
    QCOMPARE(field(entryAt(facade, 2), "id"), QStringLiteral("10"));
}

void CallHistoryFacadeTest::unknownDatesSortLast()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"5", "6", "7"}),
                      strings({"A", "B", "C"}),
                      strings({"", "abc", "50"}),
                      strings({"0", "0", "0"}),
                      strings({"0", "0", "0"}));

    // The only dated call leads; the two unknown dates fall to the tail and
    // keep the id-descending tie-break.
    QCOMPARE(field(entryAt(facade, 0), "id"), QStringLiteral("7"));
    QCOMPARE(field(entryAt(facade, 1), "id"), QStringLiteral("6"));
    QCOMPARE(field(entryAt(facade, 2), "id"), QStringLiteral("5"));
}

void CallHistoryFacadeTest::readsOutcomeTokens()
{
    struct Token
    {
        QString value;
        bool missed;
        bool outgoing;
    };

    const QList<Token> tokens = {
        { QStringLiteral("missed"), true, false },
        { QStringLiteral("true"), true, false },
        { QStringLiteral("1"), true, false },
        { QStringLiteral("yes"), true, false },
        { QStringLiteral("MISSED"), true, false },
        { QStringLiteral("cancelled"), false, true },
        { QStringLiteral("canceled"), false, true },
        { QStringLiteral("outgoing"), false, true },
        { QStringLiteral("noanswer"), false, true },
        { QStringLiteral("no-answer"), false, true },
        { QStringLiteral("incoming"), false, false },
        { QStringLiteral("declined"), false, false },
        { QStringLiteral("0"), false, false },
        { QStringLiteral("false"), false, false },
        { QStringLiteral("ended"), false, false },
        { QString(), false, false },
    };

    CallHistoryFacade facade;

    for (const Token& token : tokens) {
        facade.setRecords(strings({"9"}), strings({"Ada"}), strings({"100"}),
                          strings({"30"}), QStringList{ token.value });

        const QJsonObject entry = entryAt(facade, 0);
        QVERIFY2(!entry.isEmpty(), qPrintable(token.value));
        QCOMPARE(entry.value(QStringLiteral("missed")).toBool(), token.missed);
        QCOMPARE(entry.value(QStringLiteral("outgoing")).toBool(), token.outgoing);
    }
}

void CallHistoryFacadeTest::blankNamesBecomeUnknown()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"1", "2"}), strings({"", "   "}),
                      strings({"200", "100"}), strings({"0", "0"}),
                      strings({"0", "0"}));

    QCOMPARE(field(entryAt(facade, 0), "name"), QStringLiteral("Unknown"));
    QCOMPARE(field(entryAt(facade, 1), "name"), QStringLiteral("Unknown"));
}

void CallHistoryFacadeTest::trimsTheIdForCallback()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({" 42 "}), strings({"Ada"}), strings({"100"}),
                      strings({"0"}), strings({"0"}));

    // The row hands this string straight back through callBackRequested(), so
    // the id must not keep the padding the owner happened to send.
    QCOMPARE(field(entryAt(facade, 0), "id"), QStringLiteral("42"));
}

void CallHistoryFacadeTest::countsMissedCalls()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"1", "2", "3"}),
                      strings({"A", "B", "C"}),
                      strings({"300", "200", "100"}),
                      strings({"0", "0", "0"}),
                      strings({"missed", "cancelled", "true"}));

    // A cancelled outgoing call is not a missed one.
    QCOMPARE(facade.missedCount(), 2);
    QVERIFY(!facade.empty());
}

void CallHistoryFacadeTest::replacesInsteadOfAppending()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"1", "2"}), strings({"A", "B"}),
                      strings({"200", "100"}), strings({"0", "0"}),
                      strings({"0", "0"}));
    QCOMPARE(facade.entries().size(), 2);

    facade.setRecords(strings({"3"}), strings({"C"}), strings({"300"}),
                      strings({"0"}), strings({"0"}));

    QCOMPARE(facade.entries().size(), 1);
    QCOMPARE(field(entryAt(facade, 0), "id"), QStringLiteral("3"));
    QCOMPARE(facade.missedCount(), 0);
}

void CallHistoryFacadeTest::emptyIdsClearTheList()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"1"}), strings({"A"}), strings({"100"}),
                      strings({"0"}), strings({"0"}));

    QSignalSpy changed(&facade, &CallHistoryFacade::changed);
    facade.setRecords(QStringList(), QStringList(), QStringList(), QStringList(),
                      QStringList());

    QVERIFY(facade.entries().isEmpty());
    QVERIFY(facade.empty());
    QCOMPARE(changed.count(), 1);
}

void CallHistoryFacadeTest::repeatedInputIsSilent()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"7", "8"}), strings({"Ada", "Bob"}),
                      strings({"200", "100"}), strings({"30", "40"}),
                      strings({"0", "1"}));

    QSignalSpy changed(&facade, &CallHistoryFacade::changed);

    // The same page again: nothing on screen changed.
    facade.setRecords(strings({"7", "8"}), strings({"Ada", "Bob"}),
                      strings({"200", "100"}), strings({"30", "40"}),
                      strings({"0", "1"}));
    QCOMPARE(changed.count(), 0);

    // The same calls in another input order prepare to the same list, so a
    // refresh that reorders its page does not repaint either.
    facade.setRecords(strings({"8", "7"}), strings({"Bob", "Ada"}),
                      strings({"100", "200"}), strings({"40", "30"}),
                      strings({"1", "0"}));
    QCOMPARE(changed.count(), 0);

    // A real change still reports once.
    facade.setRecords(strings({"7", "8"}), strings({"Ada", "Bob"}),
                      strings({"200", "100"}), strings({"30", "41"}),
                      strings({"0", "1"}));
    QCOMPARE(changed.count(), 1);
}

void CallHistoryFacadeTest::clearEmptiesTheList()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"1", "2"}), strings({"A", "B"}),
                      strings({"200", "100"}), strings({"0", "0"}),
                      strings({"missed", "0"}));
    QCOMPARE(facade.missedCount(), 1);

    QSignalSpy changed(&facade, &CallHistoryFacade::changed);
    facade.clear();

    QVERIFY(facade.entries().isEmpty());
    QVERIFY(facade.empty());
    QCOMPARE(facade.missedCount(), 0);
    QCOMPARE(changed.count(), 1);
}

void CallHistoryFacadeTest::clearIsNoOpWhenEmpty()
{
    CallHistoryFacade facade;
    QSignalSpy changed(&facade, &CallHistoryFacade::changed);

    facade.clear();
    facade.clear();

    QVERIFY(facade.empty());
    QCOMPARE(changed.count(), 0);
}

void CallHistoryFacadeTest::keepsAwkwardNamesIntact()
{
    CallHistoryFacade facade;

    // A quote or a comma in a name must survive the JSON encoding; a display
    // line joined by a delimiter would not.
    const QString name = QStringLiteral("Ada \"Lovelace\", Jr.");
    facade.setRecords(strings({"1"}), QStringList{ name }, strings({"100"}),
                      strings({"0"}), strings({"0"}));

    QCOMPARE(field(entryAt(facade, 0), "name"), name);
}

void CallHistoryFacadeTest::entriesAreCompactJsonObjects()
{
    CallHistoryFacade facade;

    facade.setRecords(strings({"1", "2"}), strings({"A", "B"}),
                      strings({"200", "100"}), strings({"0", "0"}),
                      strings({"0", "0"}));

    const QStringList lines = facade.entries();
    QCOMPARE(lines.size(), 2);

    for (const QString& line : lines) {
        // One line per call: a delegate can hand the string to JSON.parse()
        // without stripping newlines first.
        QVERIFY(!line.contains(QLatin1Char('\n')));
        QVERIFY(line.startsWith(QLatin1Char('{')));
        QVERIFY(line.endsWith(QLatin1Char('}')));

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(line.toUtf8(), &error);
        QVERIFY(error.error == QJsonParseError::NoError);
        QVERIFY(document.isObject());
        QVERIFY(!document.object().isEmpty());
    }
}

QTEST_GUILESS_MAIN(CallHistoryFacadeTest)
#include "tst_call_history_facade.moc"
