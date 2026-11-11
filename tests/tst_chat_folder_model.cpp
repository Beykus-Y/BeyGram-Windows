#include "telegram/ChatFolderListModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

ChatFolderInfo makeFolder(qint64 id, const QString& title, int position, bool isMain = false)
{
    ChatFolderInfo folder;
    folder.id = id;
    folder.title = title;
    folder.position = position;
    folder.isMain = isMain;
    return folder;
}

ChatFoldersState makeState(const QVector<ChatFolderInfo>& folders)
{
    ChatFoldersState state;
    state.folders = folders;
    return state;
}

} // namespace

class ChatFolderModelTest final : public QObject {
    Q_OBJECT

private slots:
    void mainFolderIsFirstAndSynthesized();
    void customFoldersSortByPosition();
    void exposesRolesAndFallbacks();
    void folderIdAtBounds();
    void identicalStateEmitsNothing();
    void fieldChangeResetsWithoutCountChange();
    void clearResetsOnce();
};

void ChatFolderModelTest::mainFolderIsFirstAndSynthesized()
{
    ChatFolderListModel model;

    // No main folder in the state: it must be synthesized in front.
    ChatFoldersState state = makeState({
        makeFolder(2, QStringLiteral("Work"), 2),
        makeFolder(1, QStringLiteral("Friends"), 1),
    });
    model.setState(state);
    QCOMPARE(model.count(), 3);
    QCOMPARE(model.folderIdAt(0), qint64(0));
    QCOMPARE(model.data(model.index(0), ChatFolderListModel::FolderIdRole).toLongLong(),
             qint64(0));
    QCOMPARE(model.data(model.index(0), ChatFolderListModel::TitleRole).toString(),
             QStringLiteral("All chats"));
    QVERIFY(model.data(model.index(0), ChatFolderListModel::IsMainRole).toBool());
    QCOMPARE(model.folderIdAt(1), qint64(1));
    QCOMPARE(model.folderIdAt(2), qint64(2));

    // Main folder arrives last: it still moves to row 0.
    state = makeState({
        makeFolder(3, QStringLiteral("Later"), 5),
        makeFolder(0, QStringLiteral("All chats"), 0, true),
    });
    model.setState(state);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.folderIdAt(0), qint64(0));
    QCOMPARE(model.folderIdAt(1), qint64(3));
    QVERIFY(model.data(model.index(0), ChatFolderListModel::IsMainRole).toBool());
}

void ChatFolderModelTest::customFoldersSortByPosition()
{
    ChatFolderListModel model;

    // Positions are already ascending: the parser order is preserved.
    model.setState(makeState({
        makeFolder(0, QStringLiteral("All chats"), 0, true),
        makeFolder(5, QStringLiteral("Second"), 1),
        makeFolder(4, QStringLiteral("Third"), 2),
    }));
    QCOMPARE(model.count(), 3);
    QCOMPARE(model.folderIdAt(1), qint64(5));
    QCOMPARE(model.folderIdAt(2), qint64(4));

    // Out-of-order positions are sorted; equal positions stay in parser order.
    model.setState(makeState({
        makeFolder(0, QStringLiteral("All chats"), 0, true),
        makeFolder(7, QStringLiteral("Late"), 3),
        makeFolder(6, QStringLiteral("Early"), 1),
        makeFolder(8, QStringLiteral("Tie"), 3),
    }));
    QCOMPARE(model.count(), 4);
    QCOMPARE(model.folderIdAt(0), qint64(0));
    QCOMPARE(model.folderIdAt(1), qint64(6));
    QCOMPARE(model.folderIdAt(2), qint64(7));
    QCOMPARE(model.folderIdAt(3), qint64(8));
}

void ChatFolderModelTest::exposesRolesAndFallbacks()
{
    ChatFolderInfo custom = makeFolder(7, QString(), 1);
    custom.icon = QStringLiteral("5368324170671202286");
    custom.unreadCount = 4;

    ChatFolderInfo named = makeFolder(8, QStringLiteral("Family"), 2);
    named.icon = QStringLiteral("123");

    ChatFolderListModel model;
    model.setState(makeState({
        makeFolder(0, QString(), 0, true),
        custom,
        named,
    }));
    QCOMPARE(model.count(), 3);

    // Missing main title falls back to "All chats"; a missing custom title to "Folder".
    QCOMPARE(model.data(model.index(0), ChatFolderListModel::TitleRole).toString(),
             QStringLiteral("All chats"));
    QCOMPARE(model.data(model.index(1), ChatFolderListModel::TitleRole).toString(),
             QStringLiteral("Folder"));
    QCOMPARE(model.data(model.index(2), ChatFolderListModel::TitleRole).toString(),
             QStringLiteral("Family"));

    QCOMPARE(model.data(model.index(1), ChatFolderListModel::FolderIdRole).toLongLong(),
             qint64(7));
    QCOMPARE(model.data(model.index(1), ChatFolderListModel::IconRole).toString(),
             QStringLiteral("5368324170671202286"));
    QCOMPARE(model.data(model.index(1), ChatFolderListModel::UnreadCountRole).toInt(), 4);
    QVERIFY(!model.data(model.index(1), ChatFolderListModel::IsMainRole).toBool());
    QVERIFY(model.data(model.index(0), ChatFolderListModel::IsMainRole).toBool());

    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.value(ChatFolderListModel::FolderIdRole), QByteArray("folderId"));
    QCOMPARE(roles.value(ChatFolderListModel::TitleRole), QByteArray("title"));
    QCOMPARE(roles.value(ChatFolderListModel::IconRole), QByteArray("icon"));
    QCOMPARE(roles.value(ChatFolderListModel::UnreadCountRole), QByteArray("unreadCount"));
    QCOMPARE(roles.value(ChatFolderListModel::IsMainRole), QByteArray("isMain"));

    QVERIFY(!model.data(QModelIndex(), ChatFolderListModel::TitleRole).isValid());
}

void ChatFolderModelTest::folderIdAtBounds()
{
    ChatFolderListModel model;
    QCOMPARE(model.folderIdAt(0), qint64(0));
    QCOMPARE(model.folderIdAt(-1), qint64(0));

    model.setState(makeState({
        makeFolder(0, QStringLiteral("All chats"), 0, true),
        makeFolder(42, QStringLiteral("Work"), 1),
    }));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.folderIdAt(0), qint64(0));
    QCOMPARE(model.folderIdAt(1), qint64(42));
    QCOMPARE(model.folderIdAt(2), qint64(0));
    QCOMPARE(model.folderIdAt(-1), qint64(0));
    QCOMPARE(model.rowCount(QModelIndex()), 2);
    QCOMPARE(model.rowCount(model.index(0)), 0);
}

void ChatFolderModelTest::identicalStateEmitsNothing()
{
    ChatFolderListModel model;
    const ChatFoldersState state = makeState({
        makeFolder(0, QStringLiteral("All chats"), 0, true),
        makeFolder(1, QStringLiteral("Friends"), 1),
    });
    model.setState(state);
    QCOMPARE(model.count(), 2);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &ChatFolderListModel::countChanged);

    model.setState(state);
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);

    // A fresh but field-identical state is still a no-op.
    ChatFolderInfo main = makeFolder(0, QStringLiteral("All chats"), 0, true);
    ChatFolderInfo friends = makeFolder(1, QStringLiteral("Friends"), 1);
    friends.hasArchive = true;
    model.setState(makeState({main, friends}));
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 0);
}

void ChatFolderModelTest::fieldChangeResetsWithoutCountChange()
{
    ChatFolderListModel model;
    model.setState(makeState({
        makeFolder(0, QStringLiteral("All chats"), 0, true),
        makeFolder(1, QStringLiteral("Friends"), 1),
    }));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &ChatFolderListModel::countChanged);

    ChatFolderInfo friends = makeFolder(1, QStringLiteral("Friends"), 1);
    friends.unreadCount = 9;
    model.setState(makeState({
        makeFolder(0, QStringLiteral("All chats"), 0, true),
        friends,
    }));

    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.data(model.index(1), ChatFolderListModel::UnreadCountRole).toInt(), 9);
}

void ChatFolderModelTest::clearResetsOnce()
{
    ChatFolderListModel model;
    model.setState(makeState({
        makeFolder(0, QStringLiteral("All chats"), 0, true),
        makeFolder(1, QStringLiteral("Friends"), 1),
    }));
    QCOMPARE(model.count(), 2);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &ChatFolderListModel::countChanged);

    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.rowCount(QModelIndex()), 0);
    QCOMPARE(model.folderIdAt(0), qint64(0));
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    model.clear();
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
}

QTEST_GUILESS_MAIN(ChatFolderModelTest)
#include "tst_chat_folder_model.moc"
