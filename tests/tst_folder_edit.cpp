#include "telegram/FolderEdit.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject folderOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("folder")).toObject();
}

QJsonObject nameOf(const QJsonObject& folder)
{
    return folder.value(QStringLiteral("name")).toObject();
}

QString titleOf(const QJsonObject& folder)
{
    return nameOf(folder)
        .value(QStringLiteral("text"))
        .toObject()
        .value(QStringLiteral("text"))
        .toString();
}

} // namespace

class FolderEditTest final : public QObject {
    Q_OBJECT

private slots:
    void createRequestHasMinimalFolderShape();
    void folderNameIsPlainFormattedText();
    void titleIsFoldedAndClamped();
    void emptyTitleIsNotInvented();
    void editRequestCarriesFolderId();
    void deleteRequestDoesNotLeaveChats();
    void getRequestShape();
    void requestsHaveNoExtra();
    void requestsSurviveSerialization();
};

void FolderEditTest::createRequestHasMinimalFolderShape()
{
    const QJsonObject request = createChatFolderRequest(QStringLiteral("Work"));

    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("createChatFolder"));
    QVERIFY(!request.contains(QStringLiteral("chat_folder_id")));

    const QJsonObject folder = folderOf(request);
    // name, icon, color_id, is_shareable, three id lists and eight flags.
    QCOMPARE(folder.size(), 16);
    QCOMPARE(folder.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatFolder"));

    const QJsonObject name = nameOf(folder);
    QCOMPARE(name.size(), 3);
    QCOMPARE(name.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatFolderName"));
    QVERIFY(name.value(QStringLiteral("animate_custom_emoji")).isBool());
    QCOMPARE(name.value(QStringLiteral("animate_custom_emoji")).toBool(), false);

    const QJsonObject text = name.value(QStringLiteral("text")).toObject();
    QCOMPARE(text.size(), 3);
    QCOMPARE(text.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
    QCOMPARE(text.value(QStringLiteral("text")).toString(), QStringLiteral("Work"));
    QVERIFY(text.value(QStringLiteral("entities")).isArray());
    QCOMPARE(text.value(QStringLiteral("entities")).toArray().size(), 0);

    // This schema has no title:string field; the name object above replaces it.
    QVERIFY(!folder.contains(QStringLiteral("title")));

    // A null icon lets TDLib pick the folder's default icon.
    QVERIFY(folder.contains(QStringLiteral("icon")));
    QVERIFY(folder.value(QStringLiteral("icon")).isNull());

    QCOMPARE(folder.value(QStringLiteral("color_id")).toInt(), -1);
    QCOMPARE(folder.value(QStringLiteral("is_shareable")).toBool(), false);

    const QStringList chatIdLists{
        QStringLiteral("pinned_chat_ids"),
        QStringLiteral("included_chat_ids"),
        QStringLiteral("excluded_chat_ids"),
    };
    for (const QString& key : chatIdLists) {
        QVERIFY(folder.value(key).isArray());
        QCOMPARE(folder.value(key).toArray().size(), 0);
    }

    const QStringList flags{
        QStringLiteral("exclude_muted"),
        QStringLiteral("exclude_read"),
        QStringLiteral("exclude_archived"),
        QStringLiteral("include_contacts"),
        QStringLiteral("include_non_contacts"),
        QStringLiteral("include_bots"),
        QStringLiteral("include_groups"),
        QStringLiteral("include_channels"),
    };
    for (const QString& key : flags) {
        QVERIFY(folder.value(key).isBool());
        QCOMPARE(folder.value(key).toBool(), false);
    }
}

void FolderEditTest::folderNameIsPlainFormattedText()
{
    const QJsonObject folder =
        folderOf(createChatFolderRequest(QStringLiteral("  Work  ")));
    QCOMPARE(titleOf(folder), QStringLiteral("Work"));

    const QJsonObject text = nameOf(folder).value(QStringLiteral("text")).toObject();
    QVERIFY(text.value(QStringLiteral("entities")).toArray().isEmpty());
    QCOMPARE(text.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
}

void FolderEditTest::titleIsFoldedAndClamped()
{
    // Line feeds are forbidden in a folder name; every whitespace run becomes
    // one space and the ends are trimmed.
    QCOMPARE(titleOf(folderOf(createChatFolderRequest(QStringLiteral("  Work \n Chat  ")))),
             QStringLiteral("Work Chat"));
    QCOMPARE(titleOf(folderOf(createChatFolderRequest(QStringLiteral("A\t\tB\r\nC")))),
             QStringLiteral("A B C"));

    // The schema allows 12 characters; longer input is cut at the limit.
    QCOMPARE(titleOf(folderOf(createChatFolderRequest(QStringLiteral("ABCDEFGHIJKL")))),
             QStringLiteral("ABCDEFGHIJKL"));
    QCOMPARE(titleOf(folderOf(createChatFolderRequest(QStringLiteral("ABCDEFGHIJKLM")))),
             QStringLiteral("ABCDEFGHIJKL"));

    // A surrogate pair is one character and must never be split by the limit.
    const QString emoji = QString::fromUtf8("\xF0\x9F\x98\x80");
    QString thirteenEmoji;
    for (int i = 0; i < 13; ++i)
        thirteenEmoji += emoji;

    const QString clamped = titleOf(folderOf(createChatFolderRequest(thirteenEmoji)));
    QCOMPARE(clamped, thirteenEmoji.left(24)); // 12 emoji, 24 UTF-16 code units
    QCOMPARE(clamped.size(), 24);
    QVERIFY(clamped.endsWith(emoji));
}

void FolderEditTest::emptyTitleIsNotInvented()
{
    const QJsonObject empty = folderOf(createChatFolderRequest(QString()));
    QCOMPARE(nameOf(empty)
                 .value(QStringLiteral("text"))
                 .toObject()
                 .value(QStringLiteral("text"))
                 .toString(),
             QString());
    QVERIFY(empty.value(QStringLiteral("icon")).isNull());
    QCOMPARE(empty.value(QStringLiteral("color_id")).toInt(), -1);

    // Whitespace-only input collapses to an empty name, which TDLib rejects;
    // the request itself stays complete.
    const QJsonObject spaces =
        folderOf(createChatFolderRequest(QStringLiteral(" \t\r\n ")));
    QCOMPARE(titleOf(spaces), QString());
    QCOMPARE(spaces.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatFolder"));
}

void FolderEditTest::editRequestCarriesFolderId()
{
    const QJsonObject request =
        editChatFolderRequest(42, QStringLiteral("  Work  "));

    QCOMPARE(request.size(), 3);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("editChatFolder"));
    QCOMPARE(request.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(42));

    // The folder body is the same minimal shape as creation.
    QVERIFY(folderOf(request) == folderOf(createChatFolderRequest(QStringLiteral("Work"))));
    QVERIFY(!folderOf(request).contains(QStringLiteral("chat_folder_id")));

    // A 32-bit folder identifier must survive the JSON value unchanged.
    const QJsonObject large =
        editChatFolderRequest(2147483647LL, QStringLiteral("Work"));
    QCOMPARE(large.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(2147483647LL));
}

void FolderEditTest::deleteRequestDoesNotLeaveChats()
{
    const QJsonObject request = deleteChatFolderRequest(7);

    QCOMPARE(request.size(), 3);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("deleteChatFolder"));
    QCOMPARE(request.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(7));

    // The schema requires leave_chat_ids, and this client never leaves chats.
    QVERIFY(request.contains(QStringLiteral("leave_chat_ids")));
    QVERIFY(request.value(QStringLiteral("leave_chat_ids")).isArray());
    QCOMPARE(request.value(QStringLiteral("leave_chat_ids")).toArray().size(), 0);
}

void FolderEditTest::getRequestShape()
{
    const QJsonObject request = getChatFolderRequest(11);

    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatFolder"));
    QCOMPARE(request.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(11));
    QVERIFY(!request.contains(QStringLiteral("folder")));
}

void FolderEditTest::requestsHaveNoExtra()
{
    const QList<QJsonObject> requests{
        createChatFolderRequest(QStringLiteral("Work")),
        editChatFolderRequest(1, QStringLiteral("Work")),
        deleteChatFolderRequest(1),
        getChatFolderRequest(1),
    };

    for (const QJsonObject& request : requests) {
        QVERIFY(!request.contains(QStringLiteral("@extra")));

        const QJsonObject folder = folderOf(request);
        if (folder.isEmpty())
            continue;

        QVERIFY(!folder.contains(QStringLiteral("@extra")));
        const QJsonObject name = nameOf(folder);
        QVERIFY(!name.contains(QStringLiteral("@extra")));
        QVERIFY(!name.value(QStringLiteral("text"))
                     .toObject()
                     .contains(QStringLiteral("@extra")));
    }
}

void FolderEditTest::requestsSurviveSerialization()
{
    const QJsonObject create =
        roundTrip(createChatFolderRequest(QStringLiteral("  Work  ")));
    const QJsonObject folder = folderOf(create);
    QCOMPARE(folder.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatFolder"));
    QCOMPARE(titleOf(folder), QStringLiteral("Work"));

    // A null icon and the empty lists must stay explicit through the transport.
    QVERIFY(folder.value(QStringLiteral("icon")).isNull());
    QCOMPARE(folder.value(QStringLiteral("color_id")).toInt(), -1);
    QCOMPARE(folder.value(QStringLiteral("is_shareable")).toBool(), false);
    QCOMPARE(folder.value(QStringLiteral("pinned_chat_ids")).toArray().size(), 0);
    QCOMPARE(folder.value(QStringLiteral("included_chat_ids")).toArray().size(), 0);
    QCOMPARE(folder.value(QStringLiteral("excluded_chat_ids")).toArray().size(), 0);
    QCOMPARE(folder.value(QStringLiteral("include_channels")).toBool(), false);

    const QJsonObject edit =
        roundTrip(editChatFolderRequest(99, QStringLiteral("Work")));
    QCOMPARE(edit.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(99));
    QCOMPARE(titleOf(folderOf(edit)), QStringLiteral("Work"));

    const QJsonObject remove = roundTrip(deleteChatFolderRequest(99));
    QVERIFY(remove.value(QStringLiteral("leave_chat_ids")).isArray());
    QCOMPARE(remove.value(QStringLiteral("leave_chat_ids")).toArray().size(), 0);

    const QJsonObject get = roundTrip(getChatFolderRequest(99));
    QCOMPARE(get.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getChatFolder"));
    QCOMPARE(get.value(QStringLiteral("chat_folder_id")).toVariant().toLongLong(),
             qint64(99));
}

QTEST_GUILESS_MAIN(FolderEditTest)
#include "tst_folder_edit.moc"
