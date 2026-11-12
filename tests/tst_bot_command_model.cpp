#include "telegram/BotCommandModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

BotCommand command(const QString& name, const QString& description = {})
{
    return BotCommand{name, description};
}

QStringList namesOf(const QVector<BotCommand>& commands)
{
    QStringList names;
    names.reserve(commands.size());
    for (const BotCommand& command : commands)
        names.append(command.command);
    return names;
}

} // namespace

class BotCommandModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void keepsIncomingOrder();
    void replacesPreviousCommands();
    void dedupesNamesCaseInsensitively();
    void keepsFirstDuplicate();
    void filtersByPrefix();
    void filterIgnoresCase();
    void filterOfEmptyPrefixReturnsAll();
    void clearDropsEverything();
    void emitsChangedOnSet();
    void emitsChangedOncePerClear();
    void returnsIndependentCopies();
};

void BotCommandModelTest::startsEmpty()
{
    BotCommandModel model;
    QCOMPARE(model.count(), 0);
    QVERIFY(model.commands().isEmpty());
    QVERIFY(model.filtered(QString()).isEmpty());
    QVERIFY(model.filtered(QStringLiteral("st")).isEmpty());
}

void BotCommandModelTest::keepsIncomingOrder()
{
    BotCommandModel model;
    model.setCommands({
        command(QStringLiteral("zeta"), QStringLiteral("Last")),
        command(QStringLiteral("alpha"), QStringLiteral("First")),
        command(QStringLiteral("middle"), QStringLiteral("Middle")),
    });

    QCOMPARE(model.count(), 3);
    QCOMPARE(namesOf(model.commands()),
             QStringList({QStringLiteral("zeta"), QStringLiteral("alpha"),
                          QStringLiteral("middle")}));
    QCOMPARE(model.commands().at(0).description, QStringLiteral("Last"));
    QCOMPARE(model.commands().at(2).description, QStringLiteral("Middle"));
}

void BotCommandModelTest::replacesPreviousCommands()
{
    BotCommandModel model;
    model.setCommands({command(QStringLiteral("start")), command(QStringLiteral("help"))});
    model.setCommands({command(QStringLiteral("settings"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.commands().at(0).command, QStringLiteral("settings"));
}

void BotCommandModelTest::dedupesNamesCaseInsensitively()
{
    BotCommandModel model;
    model.setCommands({
        command(QStringLiteral("start")),
        command(QStringLiteral("Start"), QStringLiteral("Second spelling")),
        command(QStringLiteral("help")),
        command(QStringLiteral("START"), QStringLiteral("Third spelling")),
        command(QStringLiteral("help")),
    });

    QCOMPARE(model.count(), 2);
    QCOMPARE(namesOf(model.commands()),
             QStringList({QStringLiteral("start"), QStringLiteral("help")}));
}

void BotCommandModelTest::keepsFirstDuplicate()
{
    BotCommandModel model;
    model.setCommands({
        command(QStringLiteral("help"), QStringLiteral("First")),
        command(QStringLiteral("HELP"), QStringLiteral("Second")),
    });

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.commands().at(0).command, QStringLiteral("help"));
    QCOMPARE(model.commands().at(0).description, QStringLiteral("First"));
}

void BotCommandModelTest::filtersByPrefix()
{
    BotCommandModel model;
    model.setCommands({
        command(QStringLiteral("start")),
        command(QStringLiteral("settings")),
        command(QStringLiteral("stop")),
        command(QStringLiteral("help")),
    });

    QCOMPARE(namesOf(model.filtered(QStringLiteral("st"))),
             QStringList({QStringLiteral("start"), QStringLiteral("stop")}));
    QCOMPARE(namesOf(model.filtered(QStringLiteral("set"))),
             QStringList({QStringLiteral("settings")}));
    QCOMPARE(namesOf(model.filtered(QStringLiteral("settings"))),
             QStringList({QStringLiteral("settings")}));
    QVERIFY(model.filtered(QStringLiteral("zzz")).isEmpty());
    QVERIFY(model.filtered(QStringLiteral("start ")).isEmpty());

    // Filtering never reorders or mutates the stored list.
    QCOMPARE(model.count(), 4);
    QCOMPARE(model.filtered(QStringLiteral("st")).size(), 2);
}

void BotCommandModelTest::filterIgnoresCase()
{
    BotCommandModel model;
    model.setCommands({
        command(QStringLiteral("Start")),
        command(QStringLiteral("HELP")),
    });

    QCOMPARE(namesOf(model.filtered(QStringLiteral("st"))),
             QStringList({QStringLiteral("Start")}));
    QCOMPARE(namesOf(model.filtered(QStringLiteral("ST"))),
             QStringList({QStringLiteral("Start")}));
    QCOMPARE(namesOf(model.filtered(QStringLiteral("he"))),
             QStringList({QStringLiteral("HELP")}));
}

void BotCommandModelTest::filterOfEmptyPrefixReturnsAll()
{
    BotCommandModel model;
    model.setCommands({
        command(QStringLiteral("start")),
        command(QStringLiteral("help")),
    });

    QCOMPARE(namesOf(model.filtered(QString())),
             QStringList({QStringLiteral("start"), QStringLiteral("help")}));
}

void BotCommandModelTest::clearDropsEverything()
{
    BotCommandModel model;
    model.setCommands({command(QStringLiteral("start")), command(QStringLiteral("help"))});
    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.commands().isEmpty());
    QVERIFY(model.filtered(QString()).isEmpty());
}

void BotCommandModelTest::emitsChangedOnSet()
{
    BotCommandModel model;
    QSignalSpy spy(&model, &BotCommandModel::changed);

    model.setCommands({command(QStringLiteral("start"))});
    QCOMPARE(spy.count(), 1);

    // Replacing the list, even with the same content, is a change notification.
    model.setCommands({command(QStringLiteral("start"))});
    QCOMPARE(spy.count(), 2);

    // An empty assignment still clears whatever was there, so it notifies too.
    model.setCommands({});
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.count(), 0);
}

void BotCommandModelTest::emitsChangedOncePerClear()
{
    BotCommandModel model;
    QSignalSpy spy(&model, &BotCommandModel::changed);

    // Clearing an already empty model is not a change.
    model.clear();
    QCOMPARE(spy.count(), 0);

    model.setCommands({command(QStringLiteral("start"))});
    QCOMPARE(spy.count(), 1);

    model.clear();
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 2);
}

void BotCommandModelTest::returnsIndependentCopies()
{
    BotCommandModel model;
    model.setCommands({command(QStringLiteral("start")), command(QStringLiteral("help"))});

    QVector<BotCommand> snapshot = model.commands();
    snapshot[0].command = QStringLiteral("mutated");
    snapshot.append(command(QStringLiteral("extra")));

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.commands().at(0).command, QStringLiteral("start"));

    QVector<BotCommand> filtered = model.filtered(QStringLiteral("h"));
    filtered[0].command = QStringLiteral("mutated");

    QCOMPARE(model.commands().at(1).command, QStringLiteral("help"));
    QCOMPARE(model.filtered(QStringLiteral("h")).at(0).command, QStringLiteral("help"));
}

QTEST_GUILESS_MAIN(BotCommandModelTest)
#include "tst_bot_command_model.moc"
