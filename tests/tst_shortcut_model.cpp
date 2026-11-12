#include "telegram/ShortcutModel.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

class ShortcutModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void dropsRowsWithEmptyAction();
    void keepsFirstRowPerAction();
    void trimsSequencesOnInput();
    void keepsSuppliedOrder();
    void countMatchesShortcuts();
    void shortcutsReturnsCopy();
    void filteredReturnsEveryRowForEmptyQuery();
    void filteredMatchesActionSequenceAndDescription();
    void filteredIgnoresCase();
    void filteredKeepsOrder();
    void filteredReturnsNothingWithoutMatch();
    void filteredMatchesQueryAsWritten();
    void sequenceForReturnsBoundSequence();
    void sequenceForReturnsEmptyForUnknownAction();
    void sequenceForReturnsEmptyForUnboundAction();
    void rebindChangesSequence();
    void rebindTrimsSequence();
    void rebindBindsUnboundAction();
    void rebindRejectsUnknownAction();
    void rebindRejectsEmptySequence();
    void rebindRejectsSequenceOfAnotherAction();
    void rebindAllowsSequenceFreedByAnotherAction();
    void rebindIsNoOpForSameSequence();
    void setShortcutsEmitsOnlyForRealChange();
    void setShortcutsIgnoresRowsThatNormaliseAway();
    void setShortcutsReplacesPreviousRows();
    void resetToDefaultsRestoresFirstSet();
    void resetToDefaultsCapturesDefaultsOnFirstNonEmptySet();
    void resetToDefaultsKeepsDefaultsAcrossReloads();
    void resetToDefaultsIsNoOpWithoutDefaults();
    void resetToDefaultsIsNoOpWhenUnmodified();
    void resetToDefaultsEmitsChanged();
    void isModifiedTracksRebind();
    void isModifiedCountsDescriptionChange();
};

void ShortcutModelTest::startsEmpty()
{
    ShortcutModel model;

    QVERIFY(model.shortcuts().isEmpty());
    QCOMPARE(model.count(), 0);
    QVERIFY(!model.isModified());
    QVERIFY(model.sequenceFor(QStringLiteral("send")).isEmpty());
}

void ShortcutModelTest::dropsRowsWithEmptyAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QString(), QStringLiteral("Ctrl+X"), QStringLiteral("ignored")},
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.shortcuts().at(0).action, QStringLiteral("send"));
    QVERIFY(model.sequenceFor(QString()).isEmpty());
}

void ShortcutModelTest::keepsFirstRowPerAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+1"), QStringLiteral("first")},
        {QStringLiteral("send"), QStringLiteral("Ctrl+2"), QStringLiteral("second")},
        {QStringLiteral("send"), QStringLiteral("Ctrl+3"), QStringLiteral("third")},
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.shortcuts().at(0).sequence, QStringLiteral("Ctrl+1"));
    QCOMPARE(model.shortcuts().at(0).description, QStringLiteral("first"));
}

void ShortcutModelTest::trimsSequencesOnInput()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("  Ctrl+Enter\t"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("\n"), QStringLiteral("Reply")},
    });

    // Surrounding whitespace never reaches the model, and a whitespace-only
    // sequence becomes the "unbound" empty sequence rather than a stray blank.
    QCOMPARE(model.shortcuts().at(0).sequence, QStringLiteral("Ctrl+Enter"));
    QVERIFY(model.shortcuts().at(1).sequence.isEmpty());
}

void ShortcutModelTest::keepsSuppliedOrder()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("search"), QStringLiteral("Ctrl+F"), QStringLiteral("Search")},
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
    });

    QStringList actions;
    for (const ShortcutRow& row : model.shortcuts())
        actions.append(row.action);

    QCOMPARE(actions, QStringList({QStringLiteral("search"), QStringLiteral("send"),
                                   QStringLiteral("reply")}));
}

void ShortcutModelTest::countMatchesShortcuts()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+A"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+B"), QStringLiteral("Reply")},
        {QStringLiteral("send"), QStringLiteral("Ctrl+C"), QStringLiteral("duplicate")},
        {QString(), QStringLiteral("Ctrl+D"), QStringLiteral("no action")},
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.count(), static_cast<int>(model.shortcuts().size()));
}

void ShortcutModelTest::shortcutsReturnsCopy()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    QVector<ShortcutRow> copy = model.shortcuts();
    copy[0].sequence = QStringLiteral("Ctrl+Z");
    copy.append({QStringLiteral("reply"), QStringLiteral("Ctrl+R"),
                 QStringLiteral("Reply")});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
    QVERIFY(model.sequenceFor(QStringLiteral("reply")).isEmpty());
}

void ShortcutModelTest::filteredReturnsEveryRowForEmptyQuery()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+A"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+B"), QStringLiteral("Reply")},
    });

    QCOMPARE(model.filtered(QString()).size(), 2);
    QCOMPARE(model.filtered(QStringLiteral("")).size(), 2);
}

void ShortcutModelTest::filteredMatchesActionSequenceAndDescription()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"),
         QStringLiteral("Send message")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
        {QStringLiteral("search"), QStringLiteral("Ctrl+F"),
         QStringLiteral("Search chats")},
    });

    // Action only.
    QCOMPARE(model.filtered(QStringLiteral("search")).size(), 1);
    QCOMPARE(model.filtered(QStringLiteral("search")).at(0).sequence,
             QStringLiteral("Ctrl+F"));

    // Sequence only.
    QCOMPARE(model.filtered(QStringLiteral("Ctrl+R")).size(), 1);
    QCOMPARE(model.filtered(QStringLiteral("Ctrl+R")).at(0).action,
             QStringLiteral("reply"));

    // Description only.
    QCOMPARE(model.filtered(QStringLiteral("chats")).size(), 1);
    QCOMPARE(model.filtered(QStringLiteral("chats")).at(0).action,
             QStringLiteral("search"));

    // A row matching more than one field is still returned once.
    QCOMPARE(model.filtered(QStringLiteral("reply")).size(), 1);
    QCOMPARE(model.filtered(QStringLiteral("ctrl")).size(), 3);
}

void ShortcutModelTest::filteredIgnoresCase()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"),
         QStringLiteral("Send message")},
    });

    QCOMPARE(model.filtered(QStringLiteral("CTRL+ENTER")).size(), 1);
    QCOMPARE(model.filtered(QStringLiteral("MESSAGE")).size(), 1);
    QCOMPARE(model.filtered(QStringLiteral("sEnD")).size(), 1);
}

void ShortcutModelTest::filteredKeepsOrder()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+E"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
        {QStringLiteral("forward"), QStringLiteral("Ctrl+T"), QStringLiteral("Forward")},
    });

    // All three rows carry "Ctrl+", so the query matches every row and the
    // result must still be in the order the caller supplied.
    QStringList actions;
    for (const ShortcutRow& row : model.filtered(QStringLiteral("ctrl")))
        actions.append(row.action);

    QCOMPARE(actions, QStringList({QStringLiteral("send"), QStringLiteral("reply"),
                                   QStringLiteral("forward")}));
}

void ShortcutModelTest::filteredReturnsNothingWithoutMatch()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    QVERIFY(model.filtered(QStringLiteral("zzz")).isEmpty());
}

void ShortcutModelTest::filteredMatchesQueryAsWritten()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    // The query is not trimmed, so a stray leading or trailing space matches
    // nothing in this row.
    QVERIFY(model.filtered(QStringLiteral(" ctrl")).isEmpty());
    QVERIFY(model.filtered(QStringLiteral("send ")).isEmpty());
    QCOMPARE(model.filtered(QStringLiteral("ctrl")).size(), 1);
}

void ShortcutModelTest::sequenceForReturnsBoundSequence()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
    });

    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
    QCOMPARE(model.sequenceFor(QStringLiteral("reply")), QStringLiteral("Ctrl+R"));
}

void ShortcutModelTest::sequenceForReturnsEmptyForUnknownAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    QVERIFY(model.sequenceFor(QStringLiteral("delete")).isEmpty());
    // The action is matched exactly, never case-folded.
    QVERIFY(model.sequenceFor(QStringLiteral("Send")).isEmpty());
}

void ShortcutModelTest::sequenceForReturnsEmptyForUnboundAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QString(), QStringLiteral("Send")},
    });

    QCOMPARE(model.count(), 1);
    QVERIFY(model.sequenceFor(QStringLiteral("send")).isEmpty());
}

void ShortcutModelTest::rebindChangesSequence()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    QVERIFY(model.rebind(QStringLiteral("send"), QStringLiteral("Ctrl+Shift+Enter")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Shift+Enter"));
    // The description is display text and survives a rebind untouched.
    QCOMPARE(model.shortcuts().at(0).description, QStringLiteral("Send"));
}

void ShortcutModelTest::rebindTrimsSequence()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    QVERIFY(model.rebind(QStringLiteral("send"), QStringLiteral("  Alt+Enter\t")));
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Alt+Enter"));
}

void ShortcutModelTest::rebindBindsUnboundAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QString(), QStringLiteral("Send")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    QVERIFY(model.rebind(QStringLiteral("send"), QStringLiteral("Ctrl+Enter")));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
}

void ShortcutModelTest::rebindRejectsUnknownAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    QVERIFY(!model.rebind(QStringLiteral("delete"), QStringLiteral("Ctrl+D")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
    QVERIFY(model.sequenceFor(QStringLiteral("delete")).isEmpty());
}

void ShortcutModelTest::rebindRejectsEmptySequence()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QString(), QStringLiteral("Reply")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    QVERIFY(!model.rebind(QStringLiteral("send"), QString()));
    QVERIFY(!model.rebind(QStringLiteral("send"), QStringLiteral("   ")));
    QVERIFY(!model.rebind(QStringLiteral("reply"), QStringLiteral("\t\n")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
    QVERIFY(model.sequenceFor(QStringLiteral("reply")).isEmpty());
}

void ShortcutModelTest::rebindRejectsSequenceOfAnotherAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    QVERIFY(!model.rebind(QStringLiteral("reply"), QStringLiteral("Ctrl+Enter")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.sequenceFor(QStringLiteral("reply")), QStringLiteral("Ctrl+R"));
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));

    // Padding is trimmed before the conflict is judged, so it cannot smuggle a
    // duplicate past the check.
    QVERIFY(!model.rebind(QStringLiteral("reply"), QStringLiteral("  Ctrl+Enter ")));
    QCOMPARE(spy.count(), 0);
}

void ShortcutModelTest::rebindAllowsSequenceFreedByAnotherAction()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
    });

    // Once "send" no longer holds Ctrl+Enter, the key is free for "reply".
    QVERIFY(model.rebind(QStringLiteral("send"), QStringLiteral("Ctrl+Shift+Enter")));
    QVERIFY(model.rebind(QStringLiteral("reply"), QStringLiteral("Ctrl+Enter")));

    QCOMPARE(model.sequenceFor(QStringLiteral("reply")), QStringLiteral("Ctrl+Enter"));
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Shift+Enter"));
}

void ShortcutModelTest::rebindIsNoOpForSameSequence()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    QVERIFY(!model.rebind(QStringLiteral("send"), QStringLiteral("Ctrl+Enter")));
    // The same binding spelled with padding is still the same binding.
    QVERIFY(!model.rebind(QStringLiteral("send"), QStringLiteral("  Ctrl+Enter  ")));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
}

void ShortcutModelTest::setShortcutsEmitsOnlyForRealChange()
{
    ShortcutModel model;
    QSignalSpy spy(&model, &ShortcutModel::changed);

    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QCOMPARE(spy.count(), 1);

    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QCOMPARE(spy.count(), 1);

    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Shift+Enter"),
         QStringLiteral("Send")},
    });
    QCOMPARE(spy.count(), 2);
}

void ShortcutModelTest::setShortcutsIgnoresRowsThatNormaliseAway()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    // A duplicate action and an action-less row are both dropped, so the stored
    // rows are unchanged and nothing is announced.
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral(" Ctrl+Enter "), QStringLiteral("Send")},
        {QStringLiteral("send"), QStringLiteral("Ctrl+2"), QStringLiteral("second")},
        {QString(), QStringLiteral("Ctrl+3"), QStringLiteral("no action")},
    });

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
}

void ShortcutModelTest::setShortcutsReplacesPreviousRows()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    model.setShortcuts({
        {QStringLiteral("search"), QStringLiteral("Ctrl+F"), QStringLiteral("Search")},
    });

    QCOMPARE(model.count(), 1);
    QVERIFY(model.sequenceFor(QStringLiteral("send")).isEmpty());
    QCOMPARE(model.sequenceFor(QStringLiteral("search")), QStringLiteral("Ctrl+F"));
}

void ShortcutModelTest::resetToDefaultsRestoresFirstSet()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
    });
    model.rebind(QStringLiteral("send"), QStringLiteral("Alt+Enter"));
    model.rebind(QStringLiteral("reply"), QStringLiteral("Alt+R"));

    QVERIFY(model.isModified());
    model.resetToDefaults();

    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
    QCOMPARE(model.sequenceFor(QStringLiteral("reply")), QStringLiteral("Ctrl+R"));
    QVERIFY(!model.isModified());
}

void ShortcutModelTest::resetToDefaultsCapturesDefaultsOnFirstNonEmptySet()
{
    ShortcutModel model;

    // An empty set carries no defaults to capture.
    model.setShortcuts({});
    QVERIFY(!model.isModified());

    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    model.rebind(QStringLiteral("send"), QStringLiteral("Alt+Enter"));

    // The later, non-empty set is what the model remembers as the defaults.
    model.resetToDefaults();
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
}

void ShortcutModelTest::resetToDefaultsKeepsDefaultsAcrossReloads()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    // A caller reloading saved bindings replaces the rows, not the defaults.
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+S"), QStringLiteral("Send")},
        {QStringLiteral("reply"), QStringLiteral("Ctrl+R"), QStringLiteral("Reply")},
    });
    QVERIFY(model.isModified());

    model.resetToDefaults();

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.sequenceFor(QStringLiteral("send")), QStringLiteral("Ctrl+Enter"));
    QVERIFY(model.sequenceFor(QStringLiteral("reply")).isEmpty());
}

void ShortcutModelTest::resetToDefaultsIsNoOpWithoutDefaults()
{
    ShortcutModel model;
    QSignalSpy spy(&model, &ShortcutModel::changed);

    model.resetToDefaults();
    model.resetToDefaults();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.shortcuts().isEmpty());
    QVERIFY(!model.isModified());
}

void ShortcutModelTest::resetToDefaultsIsNoOpWhenUnmodified()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QSignalSpy spy(&model, &ShortcutModel::changed);

    model.resetToDefaults();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void ShortcutModelTest::resetToDefaultsEmitsChanged()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    model.rebind(QStringLiteral("send"), QStringLiteral("Alt+Enter"));
    QSignalSpy spy(&model, &ShortcutModel::changed);

    model.resetToDefaults();
    QCOMPARE(spy.count(), 1);

    // Already back at the defaults, so a second reset is silent.
    model.resetToDefaults();
    QCOMPARE(spy.count(), 1);
}

void ShortcutModelTest::isModifiedTracksRebind()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });
    QVERIFY(!model.isModified());

    QVERIFY(model.rebind(QStringLiteral("send"), QStringLiteral("Alt+Enter")));
    QVERIFY(model.isModified());

    // Rebinding back to the shipped key clears the flag again.
    QVERIFY(model.rebind(QStringLiteral("send"), QStringLiteral("Ctrl+Enter")));
    QVERIFY(!model.isModified());
}

void ShortcutModelTest::isModifiedCountsDescriptionChange()
{
    ShortcutModel model;
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send")},
    });

    // The rows are compared whole, so a new label is a modification even though
    // the binding itself still matches what shipped.
    model.setShortcuts({
        {QStringLiteral("send"), QStringLiteral("Ctrl+Enter"), QStringLiteral("Send now")},
    });

    QVERIFY(model.isModified());
    QCOMPARE(model.shortcuts().at(0).description, QStringLiteral("Send now"));

    model.resetToDefaults();
    QCOMPARE(model.shortcuts().at(0).description, QStringLiteral("Send"));
    QVERIFY(!model.isModified());
}

QTEST_GUILESS_MAIN(ShortcutModelTest)
#include "tst_shortcut_model.moc"
