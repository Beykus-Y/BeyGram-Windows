#include "ui/NotificationScopesFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtTest>

using beygram::ui::NotificationScopesFacade;

namespace {

// Two scopes in wire order: the main scope with every notification on, and a
// muted custom scope with preview and sound off.
void loadSample(NotificationScopesFacade& facade)
{
    facade.setScopes(QStringList{QStringLiteral("0"), QStringLiteral("42")},
                     QStringList{QStringLiteral("Default"), QStringLiteral("Work")},
                     QStringList{QStringLiteral("false"), QStringLiteral("true")},
                     QStringList{QStringLiteral("true"), QStringLiteral("false")},
                     QStringList{QStringLiteral("true"), QStringLiteral("false")});
}

} // namespace

class NotificationScopesFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void loadsScopesInOrder();
    void trimsTitlesAndKeepsEmpty();
    void dropsInvalidIds();
    void keepsFirstDuplicate();
    void missingEntriesKeepDefaults();
    void extraEntriesAreIgnored();
    void parsesFlagSpellings();
    void selectLoadsStoredFlags();
    void selectInvalidIsIgnored();
    void selectClearSelection();
    void selectSameRowIsSilent();
    void selectDropsUnsavedDraft();
    void settersEditDraft();
    void settersAreSilentWhenUnchanged();
    void settersWithoutSelectionAreIgnored();
    void saveEmitsPayload();
    void saveMainScopeUsesZero();
    void saveIsSilentWhenClean();
    void saveWithoutSelectionIsRefused();
    void saveMarksBaseline();
    void saveCanonicalisesId();
    void keepsLargeIdsAsText();
    void saveAnnouncesOncePerEdit();
    void reloadKeepsSelection();
    void reloadClampsSelection();
    void reloadResetsDraft();
    void reloadEmptyClearsSelection();
    void reloadSameScopesIsSilent();
};

void NotificationScopesFacadeTest::startsEmpty()
{
    NotificationScopesFacade facade;
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    QCOMPARE(facade.ids(), QStringList{});
    QCOMPARE(facade.titles(), QStringList{});
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.muted());
    QVERIFY(facade.preview());
    QVERIFY(facade.sound());
    QCOMPARE(changed.count(), 0);
}

void NotificationScopesFacadeTest::loadsScopesInOrder()
{
    NotificationScopesFacade facade;
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    loadSample(facade);

    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("0"), QStringLiteral("42")}));
    QCOMPARE(facade.titles(), (QStringList{QStringLiteral("Default"), QStringLiteral("Work")}));
    // Nothing was selected before, so the first row becomes the editor target.
    QCOMPARE(facade.selectedIndex(), 0);
    QVERIFY(!facade.muted());
    QVERIFY(facade.preview());
    QVERIFY(facade.sound());
    QCOMPARE(changed.count(), 1);
}

void NotificationScopesFacadeTest::trimsTitlesAndKeepsEmpty()
{
    NotificationScopesFacade facade;

    // Interior whitespace is content; an empty title stays empty for the panel
    // to label, exactly as notificationScopesFromResponse stores it.
    facade.setScopes(QStringList{QStringLiteral("0"), QStringLiteral("7")},
                     QStringList{QStringLiteral("  Default chats  "), QString()},
                     QStringList{}, QStringList{}, QStringList{});

    QCOMPARE(facade.titles(), (QStringList{QStringLiteral("Default chats"), QString()}));
    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("0"), QStringLiteral("7")}));
    QCOMPARE(facade.selectedIndex(), 0);
}

void NotificationScopesFacadeTest::dropsInvalidIds()
{
    NotificationScopesFacade facade;

    // Only the first and the fourth entries are scopes. The labels and flags
    // stay attached to their own index, so dropping an entry never shifts them.
    facade.setScopes(QStringList{QStringLiteral("0"), QStringLiteral("-1"),
                                 QStringLiteral("abc"), QStringLiteral(" 7 "), QString()},
                     QStringList{QStringLiteral("Main"), QStringLiteral("Negative"),
                                 QStringLiteral("Text"), QStringLiteral("Custom"),
                                 QStringLiteral("Empty")},
                     QStringList{QStringLiteral("false"), QStringLiteral("true"),
                                 QStringLiteral("true"), QStringLiteral("true"),
                                 QStringLiteral("true")},
                     QStringList{QStringLiteral("true"), QStringLiteral("false"),
                                 QStringLiteral("false"), QStringLiteral("false"),
                                 QStringLiteral("false")},
                     QStringList{QStringLiteral("true"), QStringLiteral("false"),
                                 QStringLiteral("false"), QStringLiteral("false"),
                                 QStringLiteral("false")});

    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("0"), QStringLiteral("7")}));
    QCOMPARE(facade.titles(), (QStringList{QStringLiteral("Main"), QStringLiteral("Custom")}));

    facade.select(1);
    QVERIFY(facade.muted());
    QVERIFY(!facade.preview());
    QVERIFY(!facade.sound());
}

void NotificationScopesFacadeTest::keepsFirstDuplicate()
{
    NotificationScopesFacade facade;

    // "5", "5" and "05" are the same scope; the first entry owns its title and
    // flags, as notificationScopesFromResponse specifies.
    facade.setScopes(QStringList{QStringLiteral("5"), QStringLiteral("5"),
                                 QStringLiteral("05")},
                     QStringList{QStringLiteral("First"), QStringLiteral("Second"),
                                 QStringLiteral("Third")},
                     QStringList{QStringLiteral("false"), QStringLiteral("true"),
                                 QStringLiteral("true")},
                     QStringList{QStringLiteral("true"), QStringLiteral("false"),
                                 QStringLiteral("false")},
                     QStringList{QStringLiteral("true"), QStringLiteral("false"),
                                 QStringLiteral("false")});

    QCOMPARE(facade.ids(), QStringList{QStringLiteral("5")});
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("First")});
    QCOMPARE(facade.selectedIndex(), 0);
    QVERIFY(!facade.muted());
    QVERIFY(facade.preview());
    QVERIFY(facade.sound());
}

void NotificationScopesFacadeTest::missingEntriesKeepDefaults()
{
    NotificationScopesFacade facade;

    // The ids define the rows. Missing titles read empty and missing flags keep
    // the NotificationScope defaults.
    facade.setScopes(QStringList{QStringLiteral("0"), QStringLiteral("3")},
                     QStringList{QStringLiteral("Main")},
                     QStringList{QStringLiteral("true")},
                     QStringList{QStringLiteral("false")},
                     QStringList{QStringLiteral("false"), QStringLiteral("true")});

    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("0"), QStringLiteral("3")}));
    QCOMPARE(facade.titles(), (QStringList{QStringLiteral("Main"), QString()}));
    QVERIFY(facade.muted());
    QVERIFY(!facade.preview());
    QVERIFY(!facade.sound());

    facade.select(1);
    QVERIFY(!facade.muted());
    QVERIFY(facade.preview());
    QVERIFY(facade.sound());
}

void NotificationScopesFacadeTest::extraEntriesAreIgnored()
{
    NotificationScopesFacade facade;

    // The ids define the rows, so an orphan title or flag past the last id
    // cannot create a row of its own.
    facade.setScopes(QStringList{QStringLiteral("0")},
                     QStringList{QStringLiteral("Main"), QStringLiteral("Orphan")},
                     QStringList{QStringLiteral("true"), QStringLiteral("false")},
                     QStringList{QStringLiteral("false"), QStringLiteral("true")},
                     QStringList{QStringLiteral("false"), QStringLiteral("true")});

    QCOMPARE(facade.ids(), QStringList{QStringLiteral("0")});
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Main")});
    QCOMPARE(facade.selectedIndex(), 0);
    QVERIFY(facade.muted());
    QVERIFY(!facade.preview());
    QVERIFY(!facade.sound());
}

void NotificationScopesFacadeTest::parsesFlagSpellings()
{
    NotificationScopesFacade facade;

    facade.setScopes(
        QStringList{QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"),
                    QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("5"),
                    QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")},
        QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
                    QStringLiteral("D"), QStringLiteral("E"), QStringLiteral("F"),
                    QStringLiteral("G"), QStringLiteral("H"), QStringLiteral("I")},
        QStringList{QStringLiteral("true"), QStringLiteral("1"), QStringLiteral("yes"),
                    QStringLiteral("on"), QStringLiteral("false"), QStringLiteral("0"),
                    QStringLiteral("no"), QStringLiteral("off"),
                    QStringLiteral("maybe")},
        QStringList{QStringLiteral("false"), QStringLiteral("0"), QStringLiteral("no"),
                    QStringLiteral("off"), QStringLiteral("true"), QStringLiteral("1"),
                    QStringLiteral("yes"), QStringLiteral("on"),
                    QStringLiteral("maybe")},
        QStringList{QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("true"),
                    QStringLiteral("false"), QStringLiteral("true"), QStringLiteral("false"),
                    QStringLiteral("true"), QStringLiteral("false"), QString()});

    const bool expectedMuted[] = {true, true, true, true, false, false, false, false, false};
    const bool expectedPreview[] = {false, false, false, false, true, true, true, true, true};
    const bool expectedSound[] = {true, false, true, false, true, false, true, false, true};

    for (int index = 0; index < 9; ++index) {
        facade.select(index);
        QCOMPARE(facade.selectedIndex(), index);
        QCOMPARE(facade.muted(), expectedMuted[index]);
        QCOMPARE(facade.preview(), expectedPreview[index]);
        QCOMPARE(facade.sound(), expectedSound[index]);
    }
}

void NotificationScopesFacadeTest::selectLoadsStoredFlags()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.select(1);

    QCOMPARE(facade.selectedIndex(), 1);
    QVERIFY(facade.muted());
    QVERIFY(!facade.preview());
    QVERIFY(!facade.sound());
    QCOMPARE(changed.count(), 1);
}

void NotificationScopesFacadeTest::selectInvalidIsIgnored()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    // A row that is not listed cannot become the editor target.
    facade.select(2);
    facade.select(-2);
    facade.select(99);

    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(changed.count(), 0);
}

void NotificationScopesFacadeTest::selectClearSelection()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.select(1);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.select(-1);

    QCOMPARE(facade.selectedIndex(), -1);
    // No row means the defaults, not the flags of the row that was cleared.
    QVERIFY(!facade.muted());
    QVERIFY(facade.preview());
    QVERIFY(facade.sound());
    QCOMPARE(changed.count(), 1);

    // Nothing can be edited or saved without a selection.
    facade.setMuted(true);
    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);
    QVERIFY(!facade.save());
    QCOMPARE(requests.count(), 0);
    QVERIFY(!facade.muted());
}

void NotificationScopesFacadeTest::selectSameRowIsSilent()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.select(0);

    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(changed.count(), 0);
}

void NotificationScopesFacadeTest::selectDropsUnsavedDraft()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.setMuted(true);
    QVERIFY(facade.muted());

    facade.select(1);
    facade.select(0);

    // The main scope is loaded from its stored flags again, so the abandoned
    // edit did not leak into another visit.
    QVERIFY(!facade.muted());
}

void NotificationScopesFacadeTest::settersEditDraft()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.setMuted(true);
    facade.setPreview(false);
    facade.setSound(false);

    QVERIFY(facade.muted());
    QVERIFY(!facade.preview());
    QVERIFY(!facade.sound());
    QCOMPARE(changed.count(), 3);
}

void NotificationScopesFacadeTest::settersAreSilentWhenUnchanged()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.select(1);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.setMuted(true);
    facade.setPreview(false);
    facade.setSound(false);

    QCOMPARE(changed.count(), 0);
}

void NotificationScopesFacadeTest::settersWithoutSelectionAreIgnored()
{
    NotificationScopesFacade facade;
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.setMuted(true);
    facade.setPreview(false);
    facade.setSound(false);

    QVERIFY(!facade.muted());
    QVERIFY(facade.preview());
    QVERIFY(facade.sound());
    QCOMPARE(changed.count(), 0);
}

void NotificationScopesFacadeTest::saveEmitsPayload()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.select(1);
    facade.setMuted(false);
    facade.setPreview(true);
    facade.setSound(true);

    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    QVERIFY(facade.save());

    QCOMPARE(requests.count(), 1);
    const QList<QVariant> payload = requests.at(0);
    QCOMPARE(payload.at(0).toString(), QStringLiteral("42"));
    QVERIFY(!payload.at(1).toBool());
    QVERIFY(payload.at(2).toBool());
    QVERIFY(payload.at(3).toBool());

    // Only the hidden baseline moved; every exposed property is unchanged, so
    // the save does not repaint the panel.
    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.selectedIndex(), 1);
}

void NotificationScopesFacadeTest::saveMainScopeUsesZero()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.setSound(false);

    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);
    QVERIFY(facade.save());

    QCOMPARE(requests.count(), 1);
    // Scope 0 is the account-wide default and is sent as "0", not as empty.
    QCOMPARE(requests.at(0).at(0).toString(), QStringLiteral("0"));
    QVERIFY(!requests.at(0).at(1).toBool());
    QVERIFY(requests.at(0).at(2).toBool());
    QVERIFY(!requests.at(0).at(3).toBool());
}

void NotificationScopesFacadeTest::saveIsSilentWhenClean()
{
    NotificationScopesFacade facade;
    loadSample(facade);

    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    QVERIFY(!facade.save());

    QCOMPARE(requests.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void NotificationScopesFacadeTest::saveWithoutSelectionIsRefused()
{
    NotificationScopesFacade facade;

    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);

    QVERIFY(!facade.save());

    QCOMPARE(requests.count(), 0);
}

void NotificationScopesFacadeTest::saveMarksBaseline()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.select(1);
    facade.setMuted(false);

    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);

    QVERIFY(facade.save());
    QCOMPARE(requests.count(), 1);

    // The current flags are now the stored baseline, so a second click sends
    // nothing.
    QVERIFY(!facade.save());
    QCOMPARE(requests.count(), 1);

    // A later edit is dirty again and keeps the flags saved in between.
    facade.setPreview(true);
    QVERIFY(facade.save());
    QCOMPARE(requests.count(), 2);

    const QList<QVariant> payload = requests.at(1);
    QCOMPARE(payload.at(0).toString(), QStringLiteral("42"));
    QVERIFY(!payload.at(1).toBool());
    QVERIFY(payload.at(2).toBool());
    QVERIFY(!payload.at(3).toBool());
}

void NotificationScopesFacadeTest::saveCanonicalisesId()
{
    NotificationScopesFacade facade;

    // The id arrives with whitespace and leading zeroes; the payload is the
    // canonical decimal scope id the request builder expects.
    facade.setScopes(QStringList{QStringLiteral(" 007 ")},
                     QStringList{QStringLiteral("Seven")},
                     QStringList{QStringLiteral("false")},
                     QStringList{QStringLiteral("true")},
                     QStringList{QStringLiteral("true")});
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("7")});

    facade.setMuted(true);
    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);
    QVERIFY(facade.save());

    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).toString(), QStringLiteral("7"));
    QVERIFY(requests.at(0).at(1).toBool());
    QVERIFY(requests.at(0).at(2).toBool());
    QVERIFY(requests.at(0).at(3).toBool());
}

void NotificationScopesFacadeTest::keepsLargeIdsAsText()
{
    NotificationScopesFacade facade;

    // A scope id is int64, so the decimal text must survive the trip through
    // QML and back without being rounded through a JavaScript number.
    const QString id = QStringLiteral("9223372036854775807");
    facade.setScopes(QStringList{id}, QStringList{QStringLiteral("Big")},
                     QStringList{QStringLiteral("false")}, QStringList{QStringLiteral("true")},
                     QStringList{QStringLiteral("true")});

    QCOMPARE(facade.ids(), QStringList{id});

    facade.setMuted(true);
    QSignalSpy requests(&facade, &NotificationScopesFacade::saveRequested);
    QVERIFY(facade.save());
    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.at(0).at(0).toString(), id);
}

void NotificationScopesFacadeTest::saveAnnouncesOncePerEdit()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.select(1);
    facade.setMuted(false);

    int announcements = 0;
    connect(&facade, &NotificationScopesFacade::saveRequested, &facade,
            [&facade, &announcements]() {
                ++announcements;
                // A handler that reports the save back synchronously must not
                // make the same edit leave the facade twice.
                facade.save();
            });

    QVERIFY(facade.save());
    QCOMPARE(announcements, 1);
}

void NotificationScopesFacadeTest::reloadKeepsSelection()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.select(1);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    loadSample(facade);

    QCOMPARE(facade.selectedIndex(), 1);
    QCOMPARE(facade.ids(), (QStringList{QStringLiteral("0"), QStringLiteral("42")}));
    QCOMPARE(changed.count(), 0);
}

void NotificationScopesFacadeTest::reloadClampsSelection()
{
    NotificationScopesFacade facade;
    facade.setScopes(QStringList{QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2")},
                     QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")},
                     QStringList{QStringLiteral("false"), QStringLiteral("false"),
                                 QStringLiteral("false")},
                     QStringList{QStringLiteral("true"), QStringLiteral("true"),
                                 QStringLiteral("true")},
                     QStringList{QStringLiteral("true"), QStringLiteral("true"),
                                 QStringLiteral("true")});
    facade.select(2);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.setScopes(QStringList{QStringLiteral("9")}, QStringList{QStringLiteral("Only")},
                     QStringList{QStringLiteral("true")}, QStringList{QStringLiteral("true")},
                     QStringList{QStringLiteral("true")});

    // Row 2 no longer exists, so the first row takes its place.
    QCOMPARE(facade.selectedIndex(), 0);
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("9")});
    QVERIFY(facade.muted());
    QCOMPARE(changed.count(), 1);
}

void NotificationScopesFacadeTest::reloadResetsDraft()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.setMuted(true);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    // The reload is authoritative: the unsaved edit is replaced by the stored
    // flags the answer brought.
    loadSample(facade);

    QVERIFY(!facade.muted());
    QCOMPARE(changed.count(), 1);
}

void NotificationScopesFacadeTest::reloadEmptyClearsSelection()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    facade.select(1);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    facade.setScopes(QStringList{}, QStringList{}, QStringList{}, QStringList{},
                     QStringList{});

    QCOMPARE(facade.ids(), QStringList{});
    QCOMPARE(facade.titles(), QStringList{});
    QCOMPARE(facade.selectedIndex(), -1);
    QVERIFY(!facade.muted());
    QVERIFY(facade.preview());
    QVERIFY(facade.sound());
    QCOMPARE(changed.count(), 1);
}

void NotificationScopesFacadeTest::reloadSameScopesIsSilent()
{
    NotificationScopesFacade facade;
    loadSample(facade);
    QSignalSpy changed(&facade, &NotificationScopesFacade::changed);

    loadSample(facade);

    QCOMPARE(changed.count(), 0);
    QCOMPARE(facade.selectedIndex(), 0);
}

QTEST_GUILESS_MAIN(NotificationScopesFacadeTest)
#include "tst_notification_scopes_facade.moc"
