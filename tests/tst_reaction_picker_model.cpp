#include "telegram/ReactionPickerModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

#include <utility>

using namespace beygram::telegram;

namespace {

// Emoji are built from UTF-8 bytes so the tests do not depend on the source
// encoding the compiler assumes.
QString grinning()
{
    return QString::fromUtf8("\xF0\x9F\x98\x80"); // U+1F600
}

QString joy()
{
    return QString::fromUtf8("\xF0\x9F\x98\x82"); // U+1F602
}

QString thumbsUp()
{
    return QString::fromUtf8("\xF0\x9F\x91\x8D"); // U+1F44D
}

QString heart()
{
    return QString::fromUtf8("\xE2\x9D\xA4"); // U+2764
}

AvailableReaction emojiReaction(const QString& emoji, const QString& title = {})
{
    AvailableReaction reaction;
    reaction.type = QStringLiteral("emoji");
    reaction.emoji = emoji;
    reaction.title = title;
    return reaction;
}

AvailableReaction customReaction(qint64 id)
{
    AvailableReaction reaction;
    reaction.type = QStringLiteral("custom_emoji");
    reaction.customEmojiId = id;
    return reaction;
}

// AvailableReaction has no operator==, so comparisons spell out its fields.
bool sameReaction(const AvailableReaction& lhs, const AvailableReaction& rhs)
{
    return lhs.type == rhs.type
        && lhs.emoji == rhs.emoji
        && lhs.customEmojiId == rhs.customEmojiId
        && lhs.title == rhs.title
        && lhs.isActive == rhs.isActive
        && lhs.isPremium == rhs.isPremium;
}

bool sameReactions(const QVector<AvailableReaction>& lhs,
                   const QVector<AvailableReaction>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype i = 0; i < lhs.size(); ++i) {
        if (!sameReaction(lhs.at(i), rhs.at(i)))
            return false;
    }

    return true;
}

ReactionCatalogState catalog(QVector<AvailableReaction> top,
                             QVector<AvailableReaction> recent = {},
                             int maxCount = 0)
{
    ReactionCatalogState state;
    state.topReactions = std::move(top);
    state.recentReactions = std::move(recent);
    state.maxReactionCount = maxCount;
    return state;
}

} // namespace

class ReactionPickerModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void copiesCatalogAsIs();
    void keepsServerOrderAndDuplicates();
    void keepsInactiveAndPremiumEntries();
    void copiesMaxCountVerbatim();
    void emitsChangedForNewCatalog();
    void doesNotEmitForIdenticalCatalog();
    void emitsWhenOnlyMaxCountChanges();
    void emitsWhenOnlyRecentChanges();
    void selectsOfferedTopEmoji();
    void selectsOfferedRecentEmoji();
    void selectsDuplicateEmojiOfferedByBothLists();
    void rejectsEmojiOutsideCatalog();
    void ignoresEmptySelectionWhenNothingSelected();
    void clearsSelectionWithEmptyEmoji();
    void reselectingSameEmojiIsNoOp();
    void isSelectedRejectsEmptyEmoji();
    void toggleSelectsThenClears();
    void toggleSelectsRecentEmoji();
    void toggleRejectsUnavailableEmoji();
    void toggleIgnoresEmptyEmoji();
    void expansionStartsCollapsed();
    void setExpandedEmitsOnFlipOnly();
    void setCatalogKeepsExpandedFlag();
    void clearResetsEverything();
    void clearIsNoOpWhenAlreadyEmpty();
    void catalogChangeDropsVanishedSelection();
    void catalogChangeKeepsSurvivingSelection();
    void catalogChangeKeepsSelectionFromRecent();
    void returnsIndependentCopies();
};

void ReactionPickerModelTest::startsEmpty()
{
    ReactionPickerModel model;

    QVERIFY(model.top().isEmpty());
    QVERIFY(model.recent().isEmpty());
    QCOMPARE(model.maxCount(), 0);
    QVERIFY(model.selected().isEmpty());
    QVERIFY(!model.isSelected(grinning()));
    QVERIFY(!model.isSelected(QString()));
    QVERIFY(!model.expanded());
}

void ReactionPickerModelTest::copiesCatalogAsIs()
{
    ReactionPickerModel model;
    const QVector<AvailableReaction> top{
        emojiReaction(grinning(), QStringLiteral("Grinning face")),
        customReaction(42),
        emojiReaction(joy()),
    };
    const QVector<AvailableReaction> recent{emojiReaction(heart())};

    model.setCatalog(catalog(top, recent, 3));

    QVERIFY(sameReactions(model.top(), top));
    QVERIFY(sameReactions(model.recent(), recent));
    QCOMPARE(model.maxCount(), 3);
}

void ReactionPickerModelTest::keepsServerOrderAndDuplicates()
{
    ReactionPickerModel model;
    const QVector<AvailableReaction> top{
        emojiReaction(heart()),
        emojiReaction(grinning()),
        emojiReaction(heart()),
    };

    model.setCatalog(catalog(top, top));

    QCOMPARE(model.top().size(), 3);
    QCOMPARE(model.top().at(0).emoji, heart());
    QCOMPARE(model.top().at(1).emoji, grinning());
    QCOMPARE(model.top().at(2).emoji, heart());
    // Both lists are stored independently, so overlapping entries stay in both.
    QCOMPARE(model.recent().size(), 3);
    QCOMPARE(model.recent().at(1).emoji, grinning());
}

void ReactionPickerModelTest::keepsInactiveAndPremiumEntries()
{
    AvailableReaction inactive = emojiReaction(heart());
    inactive.isActive = false;
    AvailableReaction premium = emojiReaction(thumbsUp());
    premium.isPremium = true;

    ReactionPickerModel model;
    model.setCatalog(catalog({inactive, premium}));

    QCOMPARE(model.top().size(), 2);
    QVERIFY(!model.top().at(0).isActive);
    QVERIFY(model.top().at(1).isPremium);

    // Nothing is filtered, so an inactive or premium entry is still selectable.
    model.setSelected(thumbsUp());
    QCOMPARE(model.selected(), thumbsUp());
}

void ReactionPickerModelTest::copiesMaxCountVerbatim()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}, {}, 7));
    QCOMPARE(model.maxCount(), 7);

    model.setCatalog(catalog({emojiReaction(grinning())}, {}, 0));
    QCOMPARE(model.maxCount(), 0);
}

void ReactionPickerModelTest::emitsChangedForNewCatalog()
{
    ReactionPickerModel model;
    QSignalSpy spy(&model, &ReactionPickerModel::changed);

    model.setCatalog(catalog({emojiReaction(grinning())}));

    QCOMPARE(spy.count(), 1);
}

void ReactionPickerModelTest::doesNotEmitForIdenticalCatalog()
{
    ReactionPickerModel model;
    QSignalSpy spy(&model, &ReactionPickerModel::changed);

    // Setting an empty catalog on an empty model is not a change either.
    model.setCatalog(catalog(QVector<AvailableReaction>()));
    QCOMPARE(spy.count(), 0);

    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(joy())}, 4));
    QCOMPARE(spy.count(), 1);

    // A freshly built but equal catalog must not notify.
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(joy())}, 4));
    QCOMPARE(spy.count(), 1);
}

void ReactionPickerModelTest::emitsWhenOnlyMaxCountChanges()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}, {}, 2));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setCatalog(catalog({emojiReaction(grinning())}, {}, 5));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.maxCount(), 5);
}

void ReactionPickerModelTest::emitsWhenOnlyRecentChanges()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(joy())}));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(joy()), emojiReaction(heart())}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.recent().size(), 2);
}

void ReactionPickerModelTest::selectsOfferedTopEmoji()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning()), emojiReaction(joy())}));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setSelected(joy());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selected(), joy());
    QVERIFY(model.isSelected(joy()));
    QVERIFY(!model.isSelected(grinning()));
}

void ReactionPickerModelTest::selectsOfferedRecentEmoji()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(thumbsUp())}));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setSelected(thumbsUp());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selected(), thumbsUp());
}

void ReactionPickerModelTest::selectsDuplicateEmojiOfferedByBothLists()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(grinning())}));

    model.setSelected(grinning());

    QCOMPARE(model.selected(), grinning());
    QVERIFY(model.isSelected(grinning()));
}

void ReactionPickerModelTest::rejectsEmojiOutsideCatalog()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setSelected(heart());
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.selected().isEmpty());

    // A rejected emoji must not replace a valid selection either.
    model.setSelected(grinning());
    spy.clear();
    model.setSelected(heart());
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selected(), grinning());
}

void ReactionPickerModelTest::ignoresEmptySelectionWhenNothingSelected()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setSelected(QString());

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.selected().isEmpty());
}

void ReactionPickerModelTest::clearsSelectionWithEmptyEmoji()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}));
    model.setSelected(grinning());

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setSelected(QString());

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.selected().isEmpty());
    QVERIFY(!model.isSelected(grinning()));
}

void ReactionPickerModelTest::reselectingSameEmojiIsNoOp()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}));
    model.setSelected(grinning());

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setSelected(grinning());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selected(), grinning());
}

void ReactionPickerModelTest::isSelectedRejectsEmptyEmoji()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}));

    QVERIFY(!model.isSelected(QString()));

    model.setSelected(grinning());
    QVERIFY(!model.isSelected(QString()));
    QVERIFY(model.isSelected(grinning()));
}

void ReactionPickerModelTest::toggleSelectsThenClears()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning()), emojiReaction(joy())}));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);

    model.toggle(joy());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selected(), joy());

    model.toggle(joy());
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.selected().isEmpty());

    model.toggle(joy());
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.selected(), joy());
}

void ReactionPickerModelTest::toggleSelectsRecentEmoji()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(thumbsUp())}));

    model.toggle(thumbsUp());

    QCOMPARE(model.selected(), thumbsUp());
}

void ReactionPickerModelTest::toggleRejectsUnavailableEmoji()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}));

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.toggle(heart());
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.selected().isEmpty());

    model.setSelected(grinning());
    spy.clear();
    model.toggle(heart());
    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selected(), grinning());
}

void ReactionPickerModelTest::toggleIgnoresEmptyEmoji()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())}));
    model.setSelected(grinning());

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.toggle(QString());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.selected(), grinning());
}

void ReactionPickerModelTest::expansionStartsCollapsed()
{
    ReactionPickerModel model;

    QVERIFY(!model.expanded());
}

void ReactionPickerModelTest::setExpandedEmitsOnFlipOnly()
{
    ReactionPickerModel model;
    QSignalSpy spy(&model, &ReactionPickerModel::changed);

    model.setExpanded(false);
    QCOMPARE(spy.count(), 0);

    model.setExpanded(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.expanded());

    model.setExpanded(true);
    QCOMPARE(spy.count(), 1);

    model.setExpanded(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.expanded());
}

void ReactionPickerModelTest::setCatalogKeepsExpandedFlag()
{
    ReactionPickerModel model;
    model.setExpanded(true);

    model.setCatalog(catalog({emojiReaction(grinning())}));

    QVERIFY(model.expanded());
}

void ReactionPickerModelTest::clearResetsEverything()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning()), emojiReaction(joy())},
                             {emojiReaction(heart())}, 3));
    model.setSelected(joy());
    model.setExpanded(true);

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.top().isEmpty());
    QVERIFY(model.recent().isEmpty());
    QCOMPARE(model.maxCount(), 0);
    QVERIFY(model.selected().isEmpty());
    QVERIFY(!model.isSelected(joy()));
    QVERIFY(!model.expanded());
}

void ReactionPickerModelTest::clearIsNoOpWhenAlreadyEmpty()
{
    ReactionPickerModel model;
    QSignalSpy spy(&model, &ReactionPickerModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
}

void ReactionPickerModelTest::catalogChangeDropsVanishedSelection()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning()), emojiReaction(joy())}));
    model.setSelected(joy());

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setCatalog(catalog({emojiReaction(grinning())}));

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.selected().isEmpty());
    QVERIFY(!model.isSelected(joy()));
}

void ReactionPickerModelTest::catalogChangeKeepsSurvivingSelection()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning()), emojiReaction(joy())}));
    model.setSelected(joy());

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setCatalog(catalog({emojiReaction(joy()), emojiReaction(heart())}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selected(), joy());
    QVERIFY(model.isSelected(joy()));
}

void ReactionPickerModelTest::catalogChangeKeepsSelectionFromRecent()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(thumbsUp())}));
    model.setSelected(thumbsUp());

    QSignalSpy spy(&model, &ReactionPickerModel::changed);
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(thumbsUp()), emojiReaction(heart())}));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.selected(), thumbsUp());
}

void ReactionPickerModelTest::returnsIndependentCopies()
{
    ReactionPickerModel model;
    model.setCatalog(catalog({emojiReaction(grinning())},
                             {emojiReaction(joy())}));

    QVector<AvailableReaction> top = model.top();
    top[0].emoji = heart();
    top.append(emojiReaction(thumbsUp()));
    QCOMPARE(model.top().size(), 1);
    QCOMPARE(model.top().at(0).emoji, grinning());

    QVector<AvailableReaction> recent = model.recent();
    recent.clear();
    QCOMPARE(model.recent().size(), 1);
    QCOMPARE(model.recent().at(0).emoji, joy());
}

QTEST_GUILESS_MAIN(ReactionPickerModelTest)
#include "tst_reaction_picker_model.moc"
