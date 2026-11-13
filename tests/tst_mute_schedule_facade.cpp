#include "ui/MuteScheduleFacade.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtTest>

#include <limits>

using namespace beygram::ui;

namespace {

constexpr qint64 kHour = 60 * 60;
constexpr qint64 kDay = 24 * kHour;
constexpr qint64 kWeek = 7 * kDay;

QStringList optionTokens()
{
    return {
        QStringLiteral("1h"),
        QStringLiteral("8h"),
        QStringLiteral("1d"),
        QStringLiteral("1w"),
        QStringLiteral("forever"),
        QStringLiteral("off"),
    };
}

QString requestedChatId(const QSignalSpy& spy, int row = 0)
{
    return spy.at(row).at(0).toString();
}

qint64 requestedUntil(const QSignalSpy& spy, int row = 0)
{
    return spy.at(row).at(1).toLongLong();
}

} // namespace

class MuteScheduleFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithoutAChat();
    void setChatLoadsForever();
    void setChatLoadsUnmuted();
    void setChatLoadsATimedMute();
    void setChatIsSilentWhenNothingMoves();
    void setChatUpdatesTheSameChat();
    void setChatReplacesTheLoadedChat();
    void setChatKeepsTheIdText();
    void setChatRejectsMalformedChatIds();
    void setChatRejectsMalformedExpiries();
    void setChatAcceptsAnExpiredMute();
    void setChatAcceptsAnExtremeExpiry();

    void chooseWithoutAChatIsSilent();
    void chooseForeverRequestsForever();
    void chooseOffRequestsUnmute();
    void chooseDurationsAreRelativeToNow();
    void chooseUpdatesTheLabel();
    void chooseNoOpsAreSilent();
    void chooseIgnoresUnknownTokens();
    void chooseAfterAnExpiredMuteWorks();

    void clearUnloadsTheChat();
    void clearWithoutAChatIsSilent();
};

void MuteScheduleFacadeTest::startsWithoutAChat()
{
    const MuteScheduleFacade facade;

    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
    QVERIFY(!facade.muted());
    QCOMPARE(facade.options(), optionTokens());
}

void MuteScheduleFacadeTest::setChatLoadsForever()
{
    MuteScheduleFacade facade;
    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);

    facade.setChat(QStringLiteral("42"), -1);

    QCOMPARE(changed.count(), 1);
    QVERIFY(facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Muted forever"));
}

void MuteScheduleFacadeTest::setChatLoadsUnmuted()
{
    MuteScheduleFacade facade;
    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);

    // Loading a chat is itself a change even when the chat is not muted: the
    // sheet moved from "no chat" to this chat.
    facade.setChat(QStringLiteral("42"), 0);

    QCOMPARE(changed.count(), 1);
    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
}

void MuteScheduleFacadeTest::setChatLoadsATimedMute()
{
    MuteScheduleFacade facade;

    // The extra five minutes keeps the label at "2h" even if the clock ticks
    // between the load and the query.
    const qint64 until = QDateTime::currentSecsSinceEpoch() + 2 * kHour + 5 * 60;
    facade.setChat(QStringLiteral("42"), until);

    QVERIFY(facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Muted for 2h"));
}

void MuteScheduleFacadeTest::setChatIsSilentWhenNothingMoves()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    facade.setChat(QStringLiteral("42"), -1);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.label(), QStringLiteral("Muted forever"));
}

void MuteScheduleFacadeTest::setChatUpdatesTheSameChat()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    facade.setChat(QStringLiteral("42"), 0);

    QCOMPARE(changed.count(), 1);
    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
}

void MuteScheduleFacadeTest::setChatReplacesTheLoadedChat()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("1"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    facade.setChat(QStringLiteral("2"), 0);

    QCOMPARE(changed.count(), 1);
    QVERIFY(!facade.muted());

    // A later choice must carry the newly loaded chat, never the previous one.
    facade.choose(QStringLiteral("forever"));

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requestedChatId(requests), QStringLiteral("2"));
    QCOMPARE(requestedUntil(requests), qint64(-1));
    QCOMPARE(facade.label(), QStringLiteral("Muted forever"));
}

void MuteScheduleFacadeTest::setChatKeepsTheIdText()
{
    MuteScheduleFacade facade;

    // 2^53 + 1 does not survive a trip through a JavaScript number, so the
    // facade keeps the id as text and hands the same text back.
    facade.setChat(QStringLiteral(" 9007199254740993 "), 0);

    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);
    facade.choose(QStringLiteral("forever"));

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requestedChatId(requests), QStringLiteral("9007199254740993"));
}

void MuteScheduleFacadeTest::setChatRejectsMalformedChatIds()
{
    MuteScheduleFacade facade;
    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    facade.setChat(QString(), -1);
    facade.setChat(QStringLiteral("not-a-chat"), -1);
    facade.setChat(QStringLiteral("0"), -1);
    facade.setChat(QStringLiteral("-7"), -1);

    QCOMPARE(changed.count(), 0);
    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));

    facade.setChat(QStringLiteral("42"), -1);
    QCOMPARE(changed.count(), 1);

    // An unusable id unloads the previous chat instead of leaving it silently
    // selected for a later choose().
    facade.setChat(QStringLiteral("12x"), -1);

    QCOMPARE(changed.count(), 2);
    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));

    facade.choose(QStringLiteral("1h"));
    QCOMPARE(requests.count(), 0);
}

void MuteScheduleFacadeTest::setChatRejectsMalformedExpiries()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    facade.setChat(QStringLiteral("42"), -2);
    facade.setChat(QStringLiteral("42"), std::numeric_limits<qint64>::min());

    QCOMPARE(changed.count(), 0);
    QVERIFY(facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Muted forever"));

    // A malformed expiry on a fresh facade loads no chat either.
    MuteScheduleFacade empty;
    QSignalSpy emptyChanged(&empty, &MuteScheduleFacade::changed);

    empty.setChat(QStringLiteral("42"), -2);

    QCOMPARE(emptyChanged.count(), 0);
    QCOMPARE(empty.label(), QStringLiteral("Unmuted"));
    QVERIFY(!empty.muted());
}

void MuteScheduleFacadeTest::setChatAcceptsAnExpiredMute()
{
    MuteScheduleFacade facade;
    const qint64 past = QDateTime::currentSecsSinceEpoch() - 60;

    facade.setChat(QStringLiteral("42"), past);

    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
}

void MuteScheduleFacadeTest::setChatAcceptsAnExtremeExpiry()
{
    MuteScheduleFacade facade;

    facade.setChat(QStringLiteral("42"), std::numeric_limits<qint64>::max());

    const QString label = facade.label();
    QVERIFY(facade.muted());
    QVERIFY(label.startsWith(QStringLiteral("Muted for ")));
    QVERIFY(label != QStringLiteral("Unmuted"));
}

void MuteScheduleFacadeTest::chooseWithoutAChatIsSilent()
{
    MuteScheduleFacade facade;
    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    facade.choose(QStringLiteral("1h"));
    facade.choose(QStringLiteral("forever"));
    facade.choose(QStringLiteral("off"));

    QCOMPARE(changed.count(), 0);
    QCOMPARE(requests.count(), 0);
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
}

void MuteScheduleFacadeTest::chooseForeverRequestsForever()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), 0);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    facade.choose(QStringLiteral("forever"));

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requestedChatId(requests), QStringLiteral("42"));
    QCOMPARE(requestedUntil(requests), qint64(-1));
    QCOMPARE(changed.count(), 1);
    QVERIFY(facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Muted forever"));
}

void MuteScheduleFacadeTest::chooseOffRequestsUnmute()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    facade.choose(QStringLiteral("off"));

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requestedChatId(requests), QStringLiteral("42"));
    QCOMPARE(requestedUntil(requests), qint64(0));
    QCOMPARE(changed.count(), 1);
    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
}

void MuteScheduleFacadeTest::chooseDurationsAreRelativeToNow()
{
    struct Case {
        const char* token;
        qint64 seconds;
    };

    const Case cases[] = {
        {"1h", kHour},
        {"8h", 8 * kHour},
        {"1d", kDay},
        {"1w", kWeek},
    };

    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), 0);

    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    for (const Case& current : cases) {
        requests.clear();

        const qint64 before = QDateTime::currentSecsSinceEpoch();
        facade.choose(QString::fromLatin1(current.token));
        const qint64 after = QDateTime::currentSecsSinceEpoch();

        QCOMPARE(requests.count(), 1);
        QCOMPARE(requestedChatId(requests), QStringLiteral("42"));

        const qint64 until = requestedUntil(requests);
        QVERIFY2(until >= before + current.seconds, current.token);
        QVERIFY2(until <= after + current.seconds, current.token);
    }
}

void MuteScheduleFacadeTest::chooseUpdatesTheLabel()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), 0);

    facade.choose(QStringLiteral("8h"));

    QVERIFY(facade.muted());
    QVERIFY(facade.label().startsWith(QStringLiteral("Muted for ")));

    facade.choose(QStringLiteral("off"));

    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
}

void MuteScheduleFacadeTest::chooseNoOpsAreSilent()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    // Re-choosing forever asks for the state already stored, so there is
    // nothing to persist and nothing to repaint.
    facade.choose(QStringLiteral("forever"));
    QCOMPARE(requests.count(), 0);
    QCOMPARE(changed.count(), 0);

    facade.choose(QStringLiteral("off"));
    QCOMPARE(requests.count(), 1);
    QCOMPARE(changed.count(), 1);

    // Already unmuted: the same choice is a no-op again.
    facade.choose(QStringLiteral("off"));
    QCOMPARE(requests.count(), 1);
    QCOMPARE(changed.count(), 1);
}

void MuteScheduleFacadeTest::chooseIgnoresUnknownTokens()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    const QStringList unknown = {
        QStringLiteral("2h"),
        QStringLiteral("1H"),
        QStringLiteral("forever "),
        QString(),
        QStringLiteral("1"),
    };

    for (const QString& token : unknown)
        facade.choose(token);

    QCOMPARE(requests.count(), 0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.label(), QStringLiteral("Muted forever"));
}

void MuteScheduleFacadeTest::chooseAfterAnExpiredMuteWorks()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"),
                   QDateTime::currentSecsSinceEpoch() - 60);

    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);
    facade.choose(QStringLiteral("forever"));

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requestedUntil(requests), qint64(-1));
    QVERIFY(facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Muted forever"));
}

void MuteScheduleFacadeTest::clearUnloadsTheChat()
{
    MuteScheduleFacade facade;
    facade.setChat(QStringLiteral("42"), -1);

    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);
    QSignalSpy requests(&facade, &MuteScheduleFacade::muteRequested);

    facade.clear();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(requests.count(), 0);
    QVERIFY(!facade.muted());
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));

    facade.choose(QStringLiteral("1h"));
    QCOMPARE(requests.count(), 0);
    QCOMPARE(changed.count(), 1);

    facade.clear();
    QCOMPARE(changed.count(), 1);
}

void MuteScheduleFacadeTest::clearWithoutAChatIsSilent()
{
    MuteScheduleFacade facade;
    QSignalSpy changed(&facade, &MuteScheduleFacade::changed);

    facade.clear();

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.label(), QStringLiteral("Unmuted"));
}

QTEST_GUILESS_MAIN(MuteScheduleFacadeTest)
#include "tst_mute_schedule_facade.moc"
