#include "ui/MultiForwardFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <initializer_list>

using beygram::ui::MultiForwardFacade;

namespace {

QStringList strings(std::initializer_list<const char*> values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const char* value : values)
        result.append(QString::fromLatin1(value));
    return result;
}

QStringList numericIds(int first, int count)
{
    QStringList ids;
    ids.reserve(count);
    for (int index = 0; index < count; ++index)
        ids.append(QString::number(first + index));
    return ids;
}

/// Opens a two-message session and offers the three standard chats.
void seedSession(MultiForwardFacade& facade)
{
    facade.begin(QStringLiteral("7"), strings({"101", "102"}));
    facade.setChats(strings({"1", "2", "3"}), strings({"Alice", "Bob", "Carol"}));
}

} // namespace

class MultiForwardFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void beginStoresSessionAndClearsPreviousSelection();
    void beginRejectsMissingSourceAndClearsSession();
    void beginRejectsMissingMessages();
    void beginNormalisesMessageIds();
    void beginRejectsMoreThan100Messages();
    void beginAcceptsDuplicateIdsWithinTheLimit();

    void setChatsPairsTitlesAndFallsBackToTheId();
    void setChatsDropsInvalidAndDuplicateIds();
    void setChatsDropsVanishedTargets();
    void setChatsIsSilentWhenNothingChanges();

    void filterMatchesTitlesCaseInsensitively();
    void filterKeepsTheSelection();
    void filterWithNoMatchesShowsNoRows();
    void chatIdAtFollowsTheFilteredOrder();

    void toggleTargetAddsAndRemoves();
    void toggleTargetIgnoresUnknownIds();
    void selectedIdsFollowCandidateOrder();
    void targetLimitIsTwenty();

    void canForwardRequiresSourceMessagesAndTarget();
    void confirmWithoutTargetFails();
    void confirmWithoutSessionFails();
    void confirmEmitsASnapshot();
    void confirmAcceptsNegativeSourceChat();
    void failedBeginDoesNotLeaveAnArmedSession();

    void changedIsEmittedOnlyForRealChanges();
};

void MultiForwardFacadeTest::startsEmpty()
{
    const MultiForwardFacade facade;

    QVERIFY2(facade.chatTitles().isEmpty(), "no chats are offered yet");
    QVERIFY2(facade.selectedIds().isEmpty(), "nothing is selected yet");
    QVERIFY2(facade.filter().isEmpty(), "the search starts empty");
    QVERIFY2(!facade.canForward(), "nothing can be forwarded yet");
    QVERIFY2(facade.error().isEmpty(), "a fresh facade has no error");
    QCOMPARE(facade.chatIdAt(0), QString());
}

void MultiForwardFacadeTest::beginStoresSessionAndClearsPreviousSelection()
{
    MultiForwardFacade facade;
    seedSession(facade);
    facade.toggleTarget(QStringLiteral("2"));
    facade.setFilter(QStringLiteral("bo"));

    QVERIFY(facade.canForward());

    facade.begin(QStringLiteral("8"), strings({"201"}));

    QVERIFY(facade.selectedIds().isEmpty());
    QVERIFY(facade.filter().isEmpty());
    QVERIFY(facade.error().isEmpty());
    QVERIFY2(!facade.canForward(), "the new session has no target yet");
    QCOMPARE(facade.chatTitles(), strings({"Alice", "Bob", "Carol"}));
}

void MultiForwardFacadeTest::beginRejectsMissingSourceAndClearsSession()
{
    MultiForwardFacade facade;
    seedSession(facade);
    facade.toggleTarget(QStringLiteral("1"));
    QVERIFY(facade.canForward());

    QSignalSpy spy(&facade, &MultiForwardFacade::changed);
    facade.begin(QStringLiteral("0"), strings({"101"}));

    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.error().isEmpty());
    QVERIFY(!facade.canForward());
    QVERIFY(facade.selectedIds().isEmpty());

    // Selecting a target again cannot revive a session without a source.
    facade.toggleTarget(QStringLiteral("1"));
    QVERIFY(!facade.canForward());
}

void MultiForwardFacadeTest::beginRejectsMissingMessages()
{
    MultiForwardFacade facade;

    facade.begin(QStringLiteral("7"), {});
    QVERIFY(!facade.error().isEmpty());
    QVERIFY(!facade.canForward());

    facade.begin(QStringLiteral("7"), strings({"0", "-1", "abc"}));
    QVERIFY(!facade.error().isEmpty());
    QVERIFY(!facade.canForward());
}

void MultiForwardFacadeTest::beginNormalisesMessageIds()
{
    MultiForwardFacade facade;
    facade.begin(QStringLiteral("7"),
                 strings({"101", "101", "0", "-3", "abc", " 102 "}));
    facade.setChats(strings({"1"}), strings({"Alice"}));
    facade.toggleTarget(QStringLiteral("1"));

    QVERIFY(facade.error().isEmpty());

    QSignalSpy spy(&facade, &MultiForwardFacade::forwardRequested);
    QVERIFY(facade.confirm());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("7"));
    QCOMPARE(spy.at(0).at(1).toStringList(), strings({"101", "102"}));
}

void MultiForwardFacadeTest::beginRejectsMoreThan100Messages()
{
    MultiForwardFacade facade;

    facade.begin(QStringLiteral("7"), numericIds(1, 101));
    QVERIFY(!facade.error().isEmpty());
    QVERIFY2(facade.error().contains(QStringLiteral("100")),
             "the error names the message limit");
    QVERIFY(!facade.canForward());

    facade.begin(QStringLiteral("7"), numericIds(1, 100));
    QVERIFY(facade.error().isEmpty());
    QVERIFY(facade.chatTitles().isEmpty());
}

void MultiForwardFacadeTest::beginAcceptsDuplicateIdsWithinTheLimit()
{
    MultiForwardFacade facade;
    QStringList messages = numericIds(1, 100);
    messages << messages.first() << QStringLiteral("0") << QStringLiteral("-4");

    facade.begin(QStringLiteral("7"), messages);
    QVERIFY(facade.error().isEmpty());
}

void MultiForwardFacadeTest::setChatsPairsTitlesAndFallsBackToTheId()
{
    MultiForwardFacade facade;

    facade.setChats(strings({"1", "2", "3"}), strings({"Alice", "  Bob  "}));

    QCOMPARE(facade.chatTitles(), strings({"Alice", "Bob", "3"}));
    QCOMPARE(facade.chatIdAt(0), QStringLiteral("1"));
    QCOMPARE(facade.chatIdAt(2), QStringLiteral("3"));
}

void MultiForwardFacadeTest::setChatsDropsInvalidAndDuplicateIds()
{
    MultiForwardFacade facade;

    facade.setChats(strings({"1", "1", "0", "-2", "3"}),
                    strings({"Alice", "Duplicate", "Zero", "Negative", "Carol"}));

    QCOMPARE(facade.chatTitles(), strings({"Alice", "Carol"}));
    QCOMPARE(facade.chatIdAt(0), QStringLiteral("1"));
    QCOMPARE(facade.chatIdAt(1), QStringLiteral("3"));
}

void MultiForwardFacadeTest::setChatsDropsVanishedTargets()
{
    MultiForwardFacade facade;
    facade.setChats(strings({"1", "2", "3"}), strings({"A", "B", "C"}));
    facade.toggleTarget(QStringLiteral("1"));
    facade.toggleTarget(QStringLiteral("3"));

    QSignalSpy spy(&facade, &MultiForwardFacade::changed);
    facade.setChats(strings({"1", "2"}), strings({"A", "B"}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.selectedIds(), strings({"1"}));
}

void MultiForwardFacadeTest::setChatsIsSilentWhenNothingChanges()
{
    MultiForwardFacade facade;
    facade.setChats(strings({"1"}), strings({"A"}));

    QSignalSpy spy(&facade, &MultiForwardFacade::changed);
    facade.setChats(strings({"1"}), strings({"A"}));
    QCOMPARE(spy.count(), 0);

    facade.setChats(strings({"1"}), strings({"B"}));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.chatTitles(), strings({"B"}));
}

void MultiForwardFacadeTest::filterMatchesTitlesCaseInsensitively()
{
    MultiForwardFacade facade;
    facade.setChats(strings({"1", "2", "3"}),
                    strings({"Alice", "Bob", "Alice Cooper"}));

    facade.setFilter(QStringLiteral("ALI"));
    QCOMPARE(facade.chatTitles(), strings({"Alice", "Alice Cooper"}));
    QCOMPARE(facade.filter(), QStringLiteral("ALI"));

    facade.setFilter(QString());
    QCOMPARE(facade.chatTitles(), strings({"Alice", "Bob", "Alice Cooper"}));
}

void MultiForwardFacadeTest::filterKeepsTheSelection()
{
    MultiForwardFacade facade;
    facade.begin(QStringLiteral("7"), strings({"101"}));
    facade.setChats(strings({"1", "2"}), strings({"Alice", "Bob"}));
    facade.toggleTarget(QStringLiteral("1"));

    facade.setFilter(QStringLiteral("bo"));

    QCOMPARE(facade.chatTitles(), strings({"Bob"}));
    QCOMPARE(facade.selectedIds(), strings({"1"}));
    QVERIFY2(facade.canForward(), "a hidden target stays selected");
}

void MultiForwardFacadeTest::filterWithNoMatchesShowsNoRows()
{
    MultiForwardFacade facade;
    facade.setChats(strings({"1"}), strings({"Alice"}));

    facade.setFilter(QStringLiteral("zzz"));

    QVERIFY(facade.chatTitles().isEmpty());
    QCOMPARE(facade.chatIdAt(0), QString());
}

void MultiForwardFacadeTest::chatIdAtFollowsTheFilteredOrder()
{
    MultiForwardFacade facade;
    facade.setChats(strings({"10", "20", "30"}),
                    strings({"Alpha", "Beta", "Alphabet"}));

    QCOMPARE(facade.chatIdAt(0), QStringLiteral("10"));
    QCOMPARE(facade.chatIdAt(1), QStringLiteral("20"));

    facade.setFilter(QStringLiteral("alpha"));

    QCOMPARE(facade.chatTitles(), strings({"Alpha", "Alphabet"}));
    QCOMPARE(facade.chatIdAt(0), QStringLiteral("10"));
    QCOMPARE(facade.chatIdAt(1), QStringLiteral("30"));
    QCOMPARE(facade.chatIdAt(2), QString());
    QCOMPARE(facade.chatIdAt(-1), QString());
}

void MultiForwardFacadeTest::toggleTargetAddsAndRemoves()
{
    MultiForwardFacade facade;
    facade.begin(QStringLiteral("7"), strings({"101"}));
    facade.setChats(strings({"1", "2"}), strings({"Alice", "Bob"}));

    QSignalSpy spy(&facade, &MultiForwardFacade::changed);

    facade.toggleTarget(QStringLiteral("2"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.selectedIds(), strings({"2"}));
    QVERIFY(facade.canForward());

    facade.toggleTarget(QStringLiteral("2"));
    QCOMPARE(spy.count(), 2);
    QVERIFY(facade.selectedIds().isEmpty());
    QVERIFY(!facade.canForward());
}

void MultiForwardFacadeTest::toggleTargetIgnoresUnknownIds()
{
    MultiForwardFacade facade;
    facade.setChats(strings({"1"}), strings({"Alice"}));

    QSignalSpy spy(&facade, &MultiForwardFacade::changed);
    facade.toggleTarget(QStringLiteral("2"));
    facade.toggleTarget(QStringLiteral("0"));
    facade.toggleTarget(QStringLiteral("abc"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(facade.selectedIds().isEmpty());
}

void MultiForwardFacadeTest::selectedIdsFollowCandidateOrder()
{
    MultiForwardFacade facade;
    facade.setChats(strings({"10", "20", "30"}), strings({"A", "B", "C"}));

    facade.toggleTarget(QStringLiteral("30"));
    facade.toggleTarget(QStringLiteral("10"));

    QCOMPARE(facade.selectedIds(), strings({"10", "30"}));
}

void MultiForwardFacadeTest::targetLimitIsTwenty()
{
    MultiForwardFacade facade;
    facade.begin(QStringLiteral("7"), strings({"101"}));
    facade.setChats(numericIds(1, 21), QStringList());

    for (int id = 1; id <= 20; ++id)
        facade.toggleTarget(QString::number(id));

    QCOMPARE(facade.selectedIds().size(), 20);
    QVERIFY(facade.canForward());
    QVERIFY(facade.error().isEmpty());

    facade.toggleTarget(QStringLiteral("21"));
    QCOMPARE(facade.selectedIds().size(), 20);
    QVERIFY2(facade.error().contains(QStringLiteral("20")),
             "the error names the target limit");
    QVERIFY2(facade.canForward(), "the existing twenty targets stay valid");

    // Dropping one target clears the limit error and frees the slot.
    facade.toggleTarget(QStringLiteral("20"));
    QCOMPARE(facade.selectedIds().size(), 19);
    QVERIFY(facade.error().isEmpty());

    facade.toggleTarget(QStringLiteral("21"));
    QCOMPARE(facade.selectedIds().size(), 20);
    QVERIFY(facade.error().isEmpty());
}

void MultiForwardFacadeTest::canForwardRequiresSourceMessagesAndTarget()
{
    MultiForwardFacade facade;
    QVERIFY(!facade.canForward());

    facade.setChats(strings({"1"}), strings({"Alice"}));
    facade.toggleTarget(QStringLiteral("1"));
    QVERIFY2(!facade.canForward(), "a target without a session is not enough");

    facade.begin(QStringLiteral("7"), strings({"101"}));
    QVERIFY2(!facade.canForward(), "begin starts a fresh selection");

    facade.toggleTarget(QStringLiteral("1"));
    QVERIFY(facade.canForward());

    facade.toggleTarget(QStringLiteral("1"));
    QVERIFY(!facade.canForward());
}

void MultiForwardFacadeTest::confirmWithoutTargetFails()
{
    MultiForwardFacade facade;
    facade.begin(QStringLiteral("7"), strings({"101"}));
    facade.setChats(strings({"1"}), strings({"Alice"}));

    QSignalSpy spy(&facade, &MultiForwardFacade::forwardRequested);
    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.error().isEmpty());

    facade.toggleTarget(QStringLiteral("1"));
    QVERIFY2(facade.error().isEmpty(), "selecting a target fixes the problem");

    QVERIFY(facade.confirm());
    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.error().isEmpty());
}

void MultiForwardFacadeTest::confirmWithoutSessionFails()
{
    MultiForwardFacade facade;

    QSignalSpy spy(&facade, &MultiForwardFacade::forwardRequested);
    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.error().isEmpty());
}

void MultiForwardFacadeTest::confirmEmitsASnapshot()
{
    MultiForwardFacade facade;
    seedSession(facade);
    facade.toggleTarget(QStringLiteral("3"));
    facade.toggleTarget(QStringLiteral("1"));

    QSignalSpy spy(&facade, &MultiForwardFacade::forwardRequested);
    QVERIFY(facade.confirm());

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> arguments = spy.at(0);
    QCOMPARE(arguments.at(0).toString(), QStringLiteral("7"));
    QCOMPARE(arguments.at(1).toStringList(), strings({"101", "102"}));
    QCOMPARE(arguments.at(2).toStringList(), strings({"1", "3"}));

    QVERIFY2(facade.canForward(), "the session is kept for a retry");
    QCOMPARE(facade.selectedIds(), strings({"1", "3"}));
}

void MultiForwardFacadeTest::confirmAcceptsNegativeSourceChat()
{
    MultiForwardFacade facade;
    facade.begin(QStringLiteral("-1001234567890"), strings({"5"}));
    facade.setChats(strings({"42"}), strings({"Saved messages"}));
    facade.toggleTarget(QStringLiteral("42"));

    QSignalSpy spy(&facade, &MultiForwardFacade::forwardRequested);
    QVERIFY(facade.confirm());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("-1001234567890"));
}

void MultiForwardFacadeTest::failedBeginDoesNotLeaveAnArmedSession()
{
    MultiForwardFacade facade;
    seedSession(facade);
    facade.toggleTarget(QStringLiteral("1"));
    QVERIFY(facade.canForward());

    facade.begin(QString(), strings({"101"}));

    QVERIFY(!facade.canForward());
    QVERIFY(!facade.error().isEmpty());
    QCOMPARE(facade.chatTitles().size(), 3);

    QSignalSpy spy(&facade, &MultiForwardFacade::forwardRequested);
    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 0);
}

void MultiForwardFacadeTest::changedIsEmittedOnlyForRealChanges()
{
    MultiForwardFacade facade;
    QSignalSpy spy(&facade, &MultiForwardFacade::changed);

    facade.begin(QStringLiteral("7"), strings({"101"}));
    QCOMPARE(spy.count(), 1);

    facade.begin(QStringLiteral("7"), strings({"101"}));
    QCOMPARE(spy.count(), 1);

    facade.setChats(strings({"1", "2"}), strings({"A", "B"}));
    QCOMPARE(spy.count(), 2);

    facade.setChats(strings({"1", "2"}), strings({"A", "B"}));
    QCOMPARE(spy.count(), 2);

    facade.setFilter(QStringLiteral("a"));
    QCOMPARE(spy.count(), 3);

    facade.setFilter(QStringLiteral("a"));
    QCOMPARE(spy.count(), 3);

    facade.toggleTarget(QStringLiteral("1"));
    QCOMPARE(spy.count(), 4);

    facade.toggleTarget(QStringLiteral("999"));
    QCOMPARE(spy.count(), 4);

    facade.toggleTarget(QStringLiteral("1"));
    QCOMPARE(spy.count(), 5);

    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 6);

    QVERIFY(!facade.confirm());
    QCOMPARE(spy.count(), 6);

    facade.toggleTarget(QStringLiteral("1"));
    QCOMPARE(spy.count(), 7);

    QVERIFY(facade.confirm());
    QCOMPARE(spy.count(), 7);
}

QTEST_GUILESS_MAIN(MultiForwardFacadeTest)
#include "tst_multi_forward_facade.moc"
