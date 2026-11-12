#include "telegram/ForwardTargetModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

ChatData chat(qint64 id, const QString& title)
{
    ChatData data;
    data.id = id;
    data.title = title;
    return data;
}

QVector<qint64> idsOf(const QVector<ChatData>& chats)
{
    QVector<qint64> ids;
    ids.reserve(chats.size());
    for (const ChatData& data : chats)
        ids.append(data.id);
    return ids;
}

QVector<QString> titlesOf(const QVector<ChatData>& chats)
{
    QVector<QString> titles;
    titles.reserve(chats.size());
    for (const ChatData& data : chats)
        titles.append(data.title);
    return titles;
}

} // namespace

class ForwardTargetModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void openRejectsInputWithoutPositiveIds();
    void openDeduplicatesAndPreservesOrder();
    void openForgetsPreviousTarget();
    void closeOnlyAffectsActiveSession();
    void setChatsDropsNonPositiveAndDuplicates();
    void setChatsClearsTargetThatDisappeared();
    void filterMatchesTitleCaseInsensitively();
    void filterRejectsTitlesWithoutMatch();
    void emptyFilterReturnsEveryChat();
    void setSelectedChatRejectsUnknownIds();
    void setSelectedChatAcceptsCandidate();
    void canSendRequiresSessionAndTarget();
    void resetClearsEverything();
    void changedOnlyOnRealChanges();
};

void ForwardTargetModelTest::startsInactive()
{
    ForwardTargetModel model;
    QVERIFY(!model.active());
    QVERIFY(model.messageIds().isEmpty());
    QVERIFY(model.chats().isEmpty());
    QVERIFY(model.filter().isEmpty());
    QCOMPARE(model.selectedChatId(), qint64(0));
    QVERIFY(!model.canSend());
}

void ForwardTargetModelTest::openRejectsInputWithoutPositiveIds()
{
    ForwardTargetModel model;
    QSignalSpy spy(&model, &ForwardTargetModel::changed);

    model.open({});
    QVERIFY(!model.active());
    QCOMPARE(spy.count(), 0);

    model.open({0, -1, -42});
    QVERIFY(!model.active());
    QVERIFY(model.messageIds().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void ForwardTargetModelTest::openDeduplicatesAndPreservesOrder()
{
    ForwardTargetModel model;
    QSignalSpy spy(&model, &ForwardTargetModel::changed);

    model.open({30, 0, 10, 30, -2, 20, 10});
    QVERIFY(model.active());
    QCOMPARE(model.messageIds(), (QVector<qint64>{30, 10, 20}));
    QCOMPARE(spy.count(), 1);
}

void ForwardTargetModelTest::openForgetsPreviousTarget()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("One")), chat(2, QStringLiteral("Two"))});
    model.open({5});
    model.setSelectedChat(2);
    QCOMPARE(model.selectedChatId(), qint64(2));
    QVERIFY(model.canSend());

    model.open({6, 7});
    QCOMPARE(model.messageIds(), (QVector<qint64>{6, 7}));
    QCOMPARE(model.selectedChatId(), qint64(0));
    QVERIFY(!model.canSend());
}

void ForwardTargetModelTest::closeOnlyAffectsActiveSession()
{
    ForwardTargetModel model;
    QSignalSpy spy(&model, &ForwardTargetModel::changed);

    model.close();
    QCOMPARE(spy.count(), 0);

    model.setChats({chat(1, QStringLiteral("One"))});
    model.open({11, 12});
    model.setSelectedChat(1);
    spy.clear();

    model.close();
    QVERIFY(!model.active());
    QVERIFY(model.messageIds().isEmpty());
    QCOMPARE(model.selectedChatId(), qint64(0));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.canSend());

    model.close();
    QCOMPARE(spy.count(), 1);
}

void ForwardTargetModelTest::setChatsDropsNonPositiveAndDuplicates()
{
    ForwardTargetModel model;
    QSignalSpy spy(&model, &ForwardTargetModel::changed);

    model.setChats({chat(3, QStringLiteral("Three")), chat(0, QStringLiteral("Zero")),
                    chat(1, QStringLiteral("One")), chat(3, QStringLiteral("Three again")),
                    chat(-5, QStringLiteral("Negative")), chat(2, QStringLiteral("Two"))});

    QCOMPARE(idsOf(model.chats()), (QVector<qint64>{3, 1, 2}));
    QCOMPARE(titlesOf(model.chats()), (QVector<QString>{QStringLiteral("Three"),
                                                        QStringLiteral("One"),
                                                        QStringLiteral("Two")}));
    QCOMPARE(spy.count(), 1);
}

void ForwardTargetModelTest::setChatsClearsTargetThatDisappeared()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("One")), chat(2, QStringLiteral("Two"))});
    model.open({9});
    model.setSelectedChat(2);
    QVERIFY(model.canSend());

    model.setChats({chat(1, QStringLiteral("One"))});
    QCOMPARE(model.selectedChatId(), qint64(0));
    QVERIFY(!model.canSend());
    QCOMPARE(idsOf(model.chats()), (QVector<qint64>{1}));
}

void ForwardTargetModelTest::filterMatchesTitleCaseInsensitively()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("Alice Adams")),
                    chat(2, QStringLiteral("Bob Brown")),
                    chat(3, QStringLiteral("Team ALPHA"))});

    model.setFilter(QStringLiteral("al"));
    QCOMPARE(idsOf(model.filteredChats()), (QVector<qint64>{1, 3}));

    model.setFilter(QStringLiteral("ALPHA"));
    QCOMPARE(idsOf(model.filteredChats()), (QVector<qint64>{3}));

    model.setFilter(QStringLiteral("brown"));
    QCOMPARE(idsOf(model.filteredChats()), (QVector<qint64>{2}));

    QCOMPARE(model.filter(), QStringLiteral("brown"));
}

void ForwardTargetModelTest::filterRejectsTitlesWithoutMatch()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("Alice")), chat(2, QStringLiteral("Bob"))});

    model.setFilter(QStringLiteral("carol"));
    QVERIFY(model.filteredChats().isEmpty());
    QCOMPARE(model.chats().size(), 2);
}

void ForwardTargetModelTest::emptyFilterReturnsEveryChat()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("Alice")), chat(2, QStringLiteral("Bob"))});
    model.setFilter(QStringLiteral("bob"));
    QCOMPARE(model.filteredChats().size(), 1);

    model.setFilter(QString());
    QCOMPARE(idsOf(model.filteredChats()), (QVector<qint64>{1, 2}));
    QVERIFY(model.filter().isEmpty());
}

void ForwardTargetModelTest::setSelectedChatRejectsUnknownIds()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("One"))});
    QSignalSpy spy(&model, &ForwardTargetModel::changed);

    model.setSelectedChat(99);
    QCOMPARE(model.selectedChatId(), qint64(0));
    QCOMPARE(spy.count(), 0);

    model.setSelectedChat(-4);
    QCOMPARE(model.selectedChatId(), qint64(0));
    QCOMPARE(spy.count(), 0);
}

void ForwardTargetModelTest::setSelectedChatAcceptsCandidate()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("One")), chat(2, QStringLiteral("Two"))});
    QSignalSpy spy(&model, &ForwardTargetModel::changed);

    model.setSelectedChat(2);
    QCOMPARE(model.selectedChatId(), qint64(2));
    QCOMPARE(spy.count(), 1);

    model.setSelectedChat(2);
    QCOMPARE(spy.count(), 1);

    model.setSelectedChat(0);
    QCOMPARE(model.selectedChatId(), qint64(0));
    QCOMPARE(spy.count(), 2);
}

void ForwardTargetModelTest::canSendRequiresSessionAndTarget()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("One")), chat(2, QStringLiteral("Two"))});
    QVERIFY(!model.canSend());

    model.setSelectedChat(1);
    QVERIFY(!model.canSend());

    model.open({42});
    QVERIFY(!model.canSend());

    model.setSelectedChat(1);
    QVERIFY(model.canSend());

    model.close();
    QVERIFY(!model.canSend());
}

void ForwardTargetModelTest::resetClearsEverything()
{
    ForwardTargetModel model;
    model.setChats({chat(1, QStringLiteral("One"))});
    model.open({7, 8});
    model.setFilter(QStringLiteral("on"));
    model.setSelectedChat(1);
    QVERIFY(model.canSend());

    QSignalSpy spy(&model, &ForwardTargetModel::changed);
    model.reset();
    QCOMPARE(spy.count(), 1);

    QVERIFY(!model.active());
    QVERIFY(model.messageIds().isEmpty());
    QVERIFY(model.chats().isEmpty());
    QVERIFY(model.filter().isEmpty());
    QCOMPARE(model.selectedChatId(), qint64(0));
    QVERIFY(!model.canSend());

    model.reset();
    QCOMPARE(spy.count(), 1);
}

void ForwardTargetModelTest::changedOnlyOnRealChanges()
{
    ForwardTargetModel model;
    QSignalSpy spy(&model, &ForwardTargetModel::changed);

    model.open({1, 2});
    QCOMPARE(spy.count(), 1);

    model.open({2, 1});
    QCOMPARE(model.messageIds(), (QVector<qint64>{2, 1}));
    QCOMPARE(spy.count(), 2);

    model.open({2, 1});
    QCOMPARE(spy.count(), 2);

    const QVector<ChatData> chats{chat(10, QStringLiteral("Ten")),
                                  chat(11, QStringLiteral("Eleven"))};
    model.setChats(chats);
    QCOMPARE(spy.count(), 3);

    model.setChats(chats);
    QCOMPARE(spy.count(), 3);

    model.setFilter(QStringLiteral("el"));
    QCOMPARE(spy.count(), 4);

    model.setFilter(QStringLiteral("el"));
    QCOMPARE(spy.count(), 4);

    model.setSelectedChat(11);
    QCOMPARE(spy.count(), 5);

    model.setSelectedChat(11);
    QCOMPARE(spy.count(), 5);

    model.setChats(chats);
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(ForwardTargetModelTest)
#include "tst_forward_target_model.moc"
