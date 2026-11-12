#include "telegram/StoryReactionModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

StoryReaction makeReaction(qint64 storyId, const QString& emoji, qint64 chatId)
{
    StoryReaction reaction;
    reaction.storyId = storyId;
    reaction.emoji = emoji;
    reaction.chatId = chatId;
    return reaction;
}

QVector<qint64> idsOf(const QVector<StoryReaction>& reactions)
{
    QVector<qint64> ids;
    ids.reserve(reactions.size());
    for (const StoryReaction& reaction : reactions)
        ids.append(reaction.storyId);
    return ids;
}

QStringList emojisOf(const QVector<StoryReaction>& reactions)
{
    QStringList emojis;
    emojis.reserve(reactions.size());
    for (const StoryReaction& reaction : reactions)
        emojis.append(reaction.emoji);
    return emojis;
}

} // namespace

class StoryReactionModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setReactionsStoresEntries();
    void setReactionsDropsNonPositiveIds();
    void setReactionsDedupesKeepingFirst();
    void setReactionsTrimsEmoji();
    void setReactionsDropsEmptyEmoji();
    void setReactionsPreservesOrder();
    void setReactionsEmitsChangedOnce();
    void setReactionsNoOpWhenIdentical();
    void setReactionsReplacesPreviousSet();
    void setReactionsWithEmptyListClears();
    void reactionsReturnsSnapshot();
    void countTracksStoredEntries();
    void reactionForReturnsEmoji();
    void reactionForUnknownIsEmpty();
    void setReactionAddsEntry();
    void setReactionUpdatesEntry();
    void setReactionRemovesOnEmptyEmoji();
    void setReactionRemovingMissingIsNoOp();
    void setReactionNoOpWhenUnchanged();
    void setReactionTrimsEmoji();
    void setReactionRejectsNonPositiveStoryId();
    void setReactionAppendsNewEntries();
    void forChatFiltersByChat();
    void forChatPreservesOrder();
    void forChatRejectsNonPositiveChatId();
    void forChatWithoutMatchIsEmpty();
    void clearDropsEverything();
    void clearOnEmptyIsSilent();
    void changedOnlyOnRealChanges();
};

void StoryReactionModelTest::startsEmpty()
{
    const StoryReactionModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.reactions().isEmpty());
    QVERIFY(model.reactionFor(1).isEmpty());
    QVERIFY(model.forChat(1).isEmpty());
}

void StoryReactionModelTest::setReactionsStoresEntries()
{
    StoryReactionModel model;

    model.setReactions({makeReaction(10, QStringLiteral("👍"), 500),
                        makeReaction(11, QStringLiteral("🔥"), 500)});

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.reactions()), QVector<qint64>({10, 11}));
    QCOMPARE(emojisOf(model.reactions()), QStringList({QStringLiteral("👍"), QStringLiteral("🔥")}));
    QCOMPARE(model.reactions().at(0).chatId, 500);
    QCOMPARE(model.reactions().at(1).chatId, 500);
}

void StoryReactionModelTest::setReactionsDropsNonPositiveIds()
{
    StoryReactionModel model;

    model.setReactions({makeReaction(0, QStringLiteral("👍"), 500),
                        makeReaction(-4, QStringLiteral("🔥"), 500),
                        makeReaction(7, QStringLiteral("❤"), 500)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.reactions()), QVector<qint64>({7}));
    QVERIFY(model.reactionFor(0).isEmpty());
    QVERIFY(model.reactionFor(-4).isEmpty());
    QCOMPARE(model.reactionFor(7), QStringLiteral("❤"));
}

void StoryReactionModelTest::setReactionsDedupesKeepingFirst()
{
    StoryReactionModel model;

    model.setReactions({makeReaction(3, QStringLiteral("👍"), 100),
                        makeReaction(3, QStringLiteral("🔥"), 200),
                        makeReaction(4, QStringLiteral("❤"), 100)});

    QCOMPARE(model.count(), 2);
    QCOMPARE(idsOf(model.reactions()), QVector<qint64>({3, 4}));
    QCOMPARE(model.reactionFor(3), QStringLiteral("👍"));
    QCOMPARE(model.reactions().at(0).chatId, 100);
}

void StoryReactionModelTest::setReactionsTrimsEmoji()
{
    StoryReactionModel model;

    model.setReactions({makeReaction(5, QStringLiteral("  👍  "), 100)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.reactionFor(5), QStringLiteral("👍"));
    QCOMPARE(model.reactions().at(0).emoji, QStringLiteral("👍"));
}

void StoryReactionModelTest::setReactionsDropsEmptyEmoji()
{
    StoryReactionModel model;

    model.setReactions({makeReaction(5, QString(), 100),
                        makeReaction(6, QStringLiteral("   "), 100),
                        makeReaction(7, QStringLiteral("❤"), 100)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(idsOf(model.reactions()), QVector<qint64>({7}));
    QVERIFY(model.reactionFor(5).isEmpty());
    QVERIFY(model.reactionFor(6).isEmpty());
}

void StoryReactionModelTest::setReactionsPreservesOrder()
{
    StoryReactionModel model;

    model.setReactions({makeReaction(9, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100),
                        makeReaction(6, QStringLiteral("❤"), 100)});

    QCOMPARE(idsOf(model.reactions()), QVector<qint64>({9, 2, 6}));
}

void StoryReactionModelTest::setReactionsEmitsChangedOnce()
{
    StoryReactionModel model;
    QSignalSpy spy(&model, &StoryReactionModel::changed);

    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100),
                        makeReaction(1, QStringLiteral("❤"), 100)});

    QCOMPARE(spy.count(), 1);
}

void StoryReactionModelTest::setReactionsNoOpWhenIdentical()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100)});

    QSignalSpy spy(&model, &StoryReactionModel::changed);

    // Same content, padded emoji: normalisation makes this a no-op.
    model.setReactions({makeReaction(1, QStringLiteral("  👍 "), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100)});
    QCOMPARE(spy.count(), 0);

    // Only chatId differs, which is still stored content.
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 999),
                        makeReaction(2, QStringLiteral("🔥"), 100)});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.reactions().at(0).chatId, 999);
}

void StoryReactionModelTest::setReactionsReplacesPreviousSet()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100)});

    model.setReactions({makeReaction(8, QStringLiteral("❤"), 200)});

    QCOMPARE(model.count(), 1);
    QVERIFY(model.reactionFor(1).isEmpty());
    QCOMPARE(model.reactionFor(8), QStringLiteral("❤"));
}

void StoryReactionModelTest::setReactionsWithEmptyListClears()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100)});

    QSignalSpy spy(&model, &StoryReactionModel::changed);

    model.setReactions({});

    QCOMPARE(model.count(), 0);
    QCOMPARE(spy.count(), 1);

    // Replacing an empty set with an empty set is silent.
    model.setReactions({});
    QCOMPARE(spy.count(), 1);
}

void StoryReactionModelTest::reactionsReturnsSnapshot()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100)});

    QVector<StoryReaction> copy = model.reactions();
    copy[0].emoji = QStringLiteral("🔥");
    copy.append(makeReaction(2, QStringLiteral("❤"), 100));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.reactionFor(1), QStringLiteral("👍"));
}

void StoryReactionModelTest::countTracksStoredEntries()
{
    StoryReactionModel model;
    QCOMPARE(model.count(), 0);

    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100),
                        makeReaction(3, QStringLiteral("❤"), 100)});
    QCOMPARE(model.count(), 3);

    model.setReaction(2, QString());
    QCOMPARE(model.count(), 2);

    model.setReaction(4, QStringLiteral("😀"));
    QCOMPARE(model.count(), 3);

    model.clear();
    QCOMPARE(model.count(), 0);
}

void StoryReactionModelTest::reactionForReturnsEmoji()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100)});

    QCOMPARE(model.reactionFor(1), QStringLiteral("👍"));
    QCOMPARE(model.reactionFor(2), QStringLiteral("🔥"));
}

void StoryReactionModelTest::reactionForUnknownIsEmpty()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100)});

    QVERIFY(model.reactionFor(2).isEmpty());
    QVERIFY(model.reactionFor(0).isEmpty());
    QVERIFY(model.reactionFor(-1).isEmpty());
}

void StoryReactionModelTest::setReactionAddsEntry()
{
    StoryReactionModel model;
    QSignalSpy spy(&model, &StoryReactionModel::changed);

    QVERIFY(model.setReaction(42, QStringLiteral("👍")));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.reactionFor(42), QStringLiteral("👍"));
    QCOMPARE(model.reactions().at(0).storyId, 42);
    QCOMPARE(spy.count(), 1);
}

void StoryReactionModelTest::setReactionUpdatesEntry()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(42, QStringLiteral("👍"), 300)});

    QSignalSpy spy(&model, &StoryReactionModel::changed);

    QVERIFY(model.setReaction(42, QStringLiteral("🔥")));

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.reactionFor(42), QStringLiteral("🔥"));
    // Updating the emoji keeps the entry, including its chat id.
    QCOMPARE(model.reactions().at(0).chatId, 300);
    QCOMPARE(spy.count(), 1);
}

void StoryReactionModelTest::setReactionRemovesOnEmptyEmoji()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 100)});

    QSignalSpy spy(&model, &StoryReactionModel::changed);

    QVERIFY(model.setReaction(1, QString()));

    QCOMPARE(model.count(), 1);
    QVERIFY(model.reactionFor(1).isEmpty());
    QCOMPARE(model.reactionFor(2), QStringLiteral("🔥"));
    QCOMPARE(spy.count(), 1);

    // A whitespace-only emoji removes as well.
    QVERIFY(model.setReaction(2, QStringLiteral("   ")));
    QCOMPARE(model.count(), 0);
    QCOMPARE(spy.count(), 2);
}

void StoryReactionModelTest::setReactionRemovingMissingIsNoOp()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100)});

    QSignalSpy spy(&model, &StoryReactionModel::changed);

    QVERIFY(!model.setReaction(2, QString()));
    QVERIFY(!model.setReaction(2, QStringLiteral("  ")));

    QCOMPARE(model.count(), 1);
    QCOMPARE(spy.count(), 0);
}

void StoryReactionModelTest::setReactionNoOpWhenUnchanged()
{
    StoryReactionModel model;
    model.setReaction(7, QStringLiteral("👍"));

    QSignalSpy spy(&model, &StoryReactionModel::changed);

    QVERIFY(!model.setReaction(7, QStringLiteral("👍")));
    QCOMPARE(spy.count(), 0);

    // Trimming happens before the comparison, so padding is not a change.
    QVERIFY(!model.setReaction(7, QStringLiteral("  👍  ")));
    QCOMPARE(spy.count(), 0);

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.reactionFor(7), QStringLiteral("👍"));
}

void StoryReactionModelTest::setReactionTrimsEmoji()
{
    StoryReactionModel model;

    QVERIFY(model.setReaction(7, QStringLiteral("\t👍 ")));

    QCOMPARE(model.reactionFor(7), QStringLiteral("👍"));
    QCOMPARE(model.reactions().at(0).emoji, QStringLiteral("👍"));
}

void StoryReactionModelTest::setReactionRejectsNonPositiveStoryId()
{
    StoryReactionModel model;
    QSignalSpy spy(&model, &StoryReactionModel::changed);

    QVERIFY(!model.setReaction(0, QStringLiteral("👍")));
    QVERIFY(!model.setReaction(-9, QStringLiteral("👍")));
    QVERIFY(!model.setReaction(0, QString()));

    QCOMPARE(model.count(), 0);
    QCOMPARE(spy.count(), 0);
}

void StoryReactionModelTest::setReactionAppendsNewEntries()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(5, QStringLiteral("👍"), 100),
                        makeReaction(6, QStringLiteral("🔥"), 100)});

    model.setReaction(1, QStringLiteral("❤"));
    model.setReaction(5, QStringLiteral("😀"));

    // New stories go to the end; updates stay in place.
    QCOMPARE(idsOf(model.reactions()), QVector<qint64>({5, 6, 1}));
    QCOMPARE(emojisOf(model.reactions()),
             QStringList({QStringLiteral("😀"), QStringLiteral("🔥"), QStringLiteral("❤")}));
}

void StoryReactionModelTest::forChatFiltersByChat()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 200),
                        makeReaction(3, QStringLiteral("❤"), 100)});

    const QVector<StoryReaction> first = model.forChat(100);
    QCOMPARE(first.size(), 2);
    QCOMPARE(idsOf(first), QVector<qint64>({1, 3}));
    QCOMPARE(emojisOf(first), QStringList({QStringLiteral("👍"), QStringLiteral("❤")}));

    const QVector<StoryReaction> second = model.forChat(200);
    QCOMPARE(second.size(), 1);
    QCOMPARE(second.at(0).storyId, 2);
    QCOMPARE(second.at(0).chatId, 200);
}

void StoryReactionModelTest::forChatPreservesOrder()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(30, QStringLiteral("👍"), 77),
                        makeReaction(10, QStringLiteral("🔥"), 77),
                        makeReaction(20, QStringLiteral("❤"), 77)});

    QCOMPARE(idsOf(model.forChat(77)), QVector<qint64>({30, 10, 20}));
}

void StoryReactionModelTest::forChatRejectsNonPositiveChatId()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 0),
                        makeReaction(2, QStringLiteral("🔥"), -5)});

    QVERIFY(model.forChat(0).isEmpty());
    QVERIFY(model.forChat(-5).isEmpty());
    QVERIFY(model.forChat(-1).isEmpty());
}

void StoryReactionModelTest::forChatWithoutMatchIsEmpty()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100)});

    QVERIFY(model.forChat(101).isEmpty());
}

void StoryReactionModelTest::clearDropsEverything()
{
    StoryReactionModel model;
    model.setReactions({makeReaction(1, QStringLiteral("👍"), 100),
                        makeReaction(2, QStringLiteral("🔥"), 200)});

    QSignalSpy spy(&model, &StoryReactionModel::changed);

    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.reactions().isEmpty());
    QVERIFY(model.reactionFor(1).isEmpty());
    QVERIFY(model.forChat(100).isEmpty());
    QCOMPARE(spy.count(), 1);
}

void StoryReactionModelTest::clearOnEmptyIsSilent()
{
    StoryReactionModel model;
    QSignalSpy spy(&model, &StoryReactionModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);

    model.setReaction(1, QStringLiteral("👍"));
    QCOMPARE(spy.count(), 1);
    model.clear();
    QCOMPARE(spy.count(), 2);
    model.clear();
    QCOMPARE(spy.count(), 2);
}

void StoryReactionModelTest::changedOnlyOnRealChanges()
{
    StoryReactionModel model;
    QSignalSpy spy(&model, &StoryReactionModel::changed);

    // Empty-model no-ops.
    model.clear();
    QVERIFY(!model.setReaction(1, QString()));
    QVERIFY(!model.setReaction(0, QStringLiteral("👍")));
    model.setReactions({});
    QCOMPARE(spy.count(), 0);

    model.setReaction(1, QStringLiteral("👍"));
    QCOMPARE(spy.count(), 1);

    model.setReaction(1, QStringLiteral("👍"));
    QCOMPARE(spy.count(), 1);

    model.setReaction(1, QStringLiteral("🔥"));
    QCOMPARE(spy.count(), 2);

    model.setReaction(2, QStringLiteral("❤"));
    QCOMPARE(spy.count(), 3);

    model.setReaction(2, QString());
    QCOMPARE(spy.count(), 4);

    model.setReactions({makeReaction(1, QStringLiteral("🔥"), 0)});
    QCOMPARE(spy.count(), 4);

    model.clear();
    QCOMPARE(spy.count(), 5);

    model.clear();
    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(StoryReactionModelTest)
#include "tst_story_reaction_model.moc"
