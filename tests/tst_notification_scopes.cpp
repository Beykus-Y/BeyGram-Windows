#include "telegram/NotificationScopes.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Requests are compared after a JSON round trip so the assertions see what the
// transport actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonObject responseWithScopes(const QJsonArray& scopes)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("notificationScopes")},
        {QStringLiteral("scopes"), scopes},
    };
}

} // namespace

class NotificationScopesTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetRequestForMainScope();
    void buildsGetRequestForCustomScope();
    void rejectsNegativeScopeIdsInRequests();
    void buildsSetRequestWithEveryField();
    void writesDefaultSetFields();
    void trimsScopeTitle();
    void buildsSetScopeChatRequest();
    void rejectsZeroChatId();
    void parsesScopesInWireOrder();
    void trimsParsedTitlesAndKeepsDefaults();
    void acceptsNumericStringScopeIds();
    void skipsMalformedEntries();
    void skipsWronglyTypedFields();
    void dedupesByIdKeepingFirst();
    void returnsEmptyForMissingOrWronglyTypedList();
    void roundTripsScopeThroughSetRequest();
};

void NotificationScopesTest::buildsGetRequestForMainScope()
{
    const QJsonObject request = serialized(getNotificationScopeRequest(0));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getNotificationScope"));
    QCOMPARE(numberAt(request, QStringLiteral("scope_id")), qint64(0));
    QCOMPARE(int(request.size()), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void NotificationScopesTest::buildsGetRequestForCustomScope()
{
    // Scope ids are int64; they must not be truncated to 32 bits.
    const qint64 scopeId = 1234567890123LL;
    const QJsonObject request = serialized(getNotificationScopeRequest(scopeId));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getNotificationScope"));
    QCOMPARE(numberAt(request, QStringLiteral("scope_id")), scopeId);
    QCOMPARE(int(request.size()), 2);
}

void NotificationScopesTest::rejectsNegativeScopeIdsInRequests()
{
    QVERIFY(getNotificationScopeRequest(-1).isEmpty());

    NotificationScope scope;
    scope.scopeId = -7;
    QVERIFY(setNotificationScopeRequest(scope).isEmpty());
}

void NotificationScopesTest::buildsSetRequestWithEveryField()
{
    NotificationScope scope;
    scope.scopeId = 7;
    scope.title = QStringLiteral("  Work  ");
    scope.muted = true;
    scope.showPreview = false;
    scope.sound = false;

    const QJsonObject request = serialized(setNotificationScopeRequest(scope));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setNotificationScope"));
    QCOMPARE(numberAt(request, QStringLiteral("scope_id")), qint64(7));
    QCOMPARE(request.value(QStringLiteral("title")).toString(), QStringLiteral("Work"));
    QVERIFY(request.value(QStringLiteral("muted")).isBool());
    QVERIFY(request.value(QStringLiteral("muted")).toBool());
    QVERIFY(request.value(QStringLiteral("show_preview")).isBool());
    QVERIFY(!request.value(QStringLiteral("show_preview")).toBool());
    QVERIFY(request.value(QStringLiteral("sound")).isBool());
    QVERIFY(!request.value(QStringLiteral("sound")).toBool());

    // The request carries the whole scope: id plus the four stored fields.
    QCOMPARE(int(request.size()), 6);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void NotificationScopesTest::writesDefaultSetFields()
{
    const QJsonObject request = serialized(setNotificationScopeRequest(NotificationScope{}));

    // The main scope (id 0) is a real scope and must be written, not dropped.
    QCOMPARE(numberAt(request, QStringLiteral("scope_id")), qint64(0));
    QCOMPARE(request.value(QStringLiteral("title")).toString(), QString());
    QVERIFY(request.value(QStringLiteral("muted")).isBool());
    QVERIFY(!request.value(QStringLiteral("muted")).toBool());
    QVERIFY(request.value(QStringLiteral("show_preview")).isBool());
    QVERIFY(request.value(QStringLiteral("show_preview")).toBool());
    QVERIFY(request.value(QStringLiteral("sound")).isBool());
    QVERIFY(request.value(QStringLiteral("sound")).toBool());
    QCOMPARE(int(request.size()), 6);
}

void NotificationScopesTest::trimsScopeTitle()
{
    NotificationScope scope;
    scope.title = QStringLiteral("   ");
    QCOMPARE(setNotificationScopeRequest(scope)
                 .value(QStringLiteral("title"))
                 .toString(),
             QString());

    scope.title = QStringLiteral("\tCustom scope\n");
    QCOMPARE(setNotificationScopeRequest(scope)
                 .value(QStringLiteral("title"))
                 .toString(),
             QStringLiteral("Custom scope"));

    // Interior whitespace is part of the name and is preserved.
    scope.title = QStringLiteral("  Work  Chat  ");
    QCOMPARE(setNotificationScopeRequest(scope)
                 .value(QStringLiteral("title"))
                 .toString(),
             QStringLiteral("Work  Chat"));
}

void NotificationScopesTest::buildsSetScopeChatRequest()
{
    const qint64 chatId = -1001234567890LL;
    const QJsonObject request = serialized(setScopeChatRequest(4, chatId, true));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setScopeChat"));
    QCOMPARE(numberAt(request, QStringLiteral("scope_id")), qint64(4));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
    QVERIFY(request.value(QStringLiteral("enabled")).isBool());
    QVERIFY(request.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(int(request.size()), 4);

    // The main scope is allowed, and removing a chat writes enabled false.
    const QJsonObject removed = serialized(setScopeChatRequest(0, 42, false));
    QCOMPARE(numberAt(removed, QStringLiteral("scope_id")), qint64(0));
    QCOMPARE(numberAt(removed, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(removed.value(QStringLiteral("enabled")).isBool());
    QVERIFY(!removed.value(QStringLiteral("enabled")).toBool());
}

void NotificationScopesTest::rejectsZeroChatId()
{
    QVERIFY(setScopeChatRequest(0, 0, true).isEmpty());
    QVERIFY(setScopeChatRequest(3, 0, false).isEmpty());

    // A negative scope id names no scope even when the chat is valid.
    QVERIFY(setScopeChatRequest(-1, 42, true).isEmpty());
}

void NotificationScopesTest::parsesScopesInWireOrder()
{
    const QJsonObject response = parseObject(R"({
        "@type": "notificationScopes",
        "scopes": [
            {"scope_id": 0, "title": "  Main  ", "muted": false,
             "show_preview": true, "sound": true},
            {"scope_id": 9, "title": "Work", "muted": true,
             "show_preview": false, "sound": false},
            {"scope_id": 4, "title": "Family"}
        ]
    })");

    const QVector<NotificationScope> scopes = notificationScopesFromResponse(response);

    QCOMPARE(scopes.size(), 3);
    QCOMPARE(scopes.at(0).scopeId, qint64(0));
    QCOMPARE(scopes.at(0).title, QStringLiteral("Main"));
    QVERIFY(!scopes.at(0).muted);
    QVERIFY(scopes.at(0).showPreview);
    QVERIFY(scopes.at(0).sound);

    QCOMPARE(scopes.at(1).scopeId, qint64(9));
    QCOMPARE(scopes.at(1).title, QStringLiteral("Work"));
    QVERIFY(scopes.at(1).muted);
    QVERIFY(!scopes.at(1).showPreview);
    QVERIFY(!scopes.at(1).sound);

    QCOMPARE(scopes.at(2).scopeId, qint64(4));
    QCOMPARE(scopes.at(2).title, QStringLiteral("Family"));
}

void NotificationScopesTest::trimsParsedTitlesAndKeepsDefaults()
{
    const QJsonObject response = parseObject(R"({
        "scopes": [
            {"scope_id": 2, "title": "  Work  Chat  "},
            {"scope_id": 3}
        ]
    })");

    const QVector<NotificationScope> scopes = notificationScopesFromResponse(response);

    QCOMPARE(scopes.size(), 2);
    QCOMPARE(scopes.at(0).title, QStringLiteral("Work  Chat"));
    QCOMPARE(scopes.at(1).title, QString());

    for (const NotificationScope& scope : scopes) {
        QVERIFY(!scope.muted);
        QVERIFY(scope.showPreview);
        QVERIFY(scope.sound);
    }
}

void NotificationScopesTest::acceptsNumericStringScopeIds()
{
    const QJsonObject response = parseObject(R"({
        "scopes": [
            {"scope_id": "12", "title": "Text id"},
            {"scope_id": " 13 ", "title": "Spaced id"},
            {"scope_id": 0, "title": "Main"}
        ]
    })");

    const QVector<NotificationScope> scopes = notificationScopesFromResponse(response);

    QCOMPARE(scopes.size(), 3);
    QCOMPARE(scopes.at(0).scopeId, qint64(12));
    QCOMPARE(scopes.at(1).scopeId, qint64(13));
    QCOMPARE(scopes.at(2).scopeId, qint64(0));
}

void NotificationScopesTest::skipsMalformedEntries()
{
    const QJsonObject response = parseObject(R"({
        "scopes": [
            "not an object",
            7,
            null,
            [],
            {},
            {"title": "No id"},
            {"scope_id": -1, "title": "Negative"},
            {"scope_id": "abc", "title": "Bad string"},
            {"scope_id": 1.5, "title": "Fractional"},
            {"scope_id": 2, "title": "Kept", "muted": true}
        ]
    })");

    const QVector<NotificationScope> scopes = notificationScopesFromResponse(response);

    QCOMPARE(scopes.size(), 1);
    QCOMPARE(scopes.at(0).scopeId, qint64(2));
    QCOMPARE(scopes.at(0).title, QStringLiteral("Kept"));
    QVERIFY(scopes.at(0).muted);
}

void NotificationScopesTest::skipsWronglyTypedFields()
{
    const QJsonObject response = parseObject(R"({
        "scopes": [
            {"scope_id": 1, "title": 7},
            {"scope_id": 2, "title": null},
            {"scope_id": 3, "title": "Bad muted", "muted": 1},
            {"scope_id": 4, "title": "Bad preview", "show_preview": "yes"},
            {"scope_id": 5, "title": "Bad sound", "sound": null},
            {"scope_id": 6, "title": "Kept"}
        ]
    })");

    const QVector<NotificationScope> scopes = notificationScopesFromResponse(response);

    QCOMPARE(scopes.size(), 1);
    QCOMPARE(scopes.at(0).scopeId, qint64(6));
    QCOMPARE(scopes.at(0).title, QStringLiteral("Kept"));
}

void NotificationScopesTest::dedupesByIdKeepingFirst()
{
    const QJsonObject response = parseObject(R"({
        "scopes": [
            {"scope_id": 5, "title": "First", "muted": true,
             "show_preview": false, "sound": false},
            {"scope_id": 5, "title": "Second"},
            {"scope_id": 0, "title": "Main"},
            {"scope_id": 0, "title": "Main again", "muted": true}
        ]
    })");

    const QVector<NotificationScope> scopes = notificationScopesFromResponse(response);

    QCOMPARE(scopes.size(), 2);

    QCOMPARE(scopes.at(0).scopeId, qint64(5));
    QCOMPARE(scopes.at(0).title, QStringLiteral("First"));
    QVERIFY(scopes.at(0).muted);
    QVERIFY(!scopes.at(0).showPreview);
    QVERIFY(!scopes.at(0).sound);

    QCOMPARE(scopes.at(1).scopeId, qint64(0));
    QCOMPARE(scopes.at(1).title, QStringLiteral("Main"));
    QVERIFY(!scopes.at(1).muted);
}

void NotificationScopesTest::returnsEmptyForMissingOrWronglyTypedList()
{
    QVERIFY(notificationScopesFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(notificationScopesFromResponse(
                parseObject(R"({"@type": "notificationScopes"})"))
                .isEmpty());
    QVERIFY(notificationScopesFromResponse(parseObject(R"({"scopes": null})")).isEmpty());
    QVERIFY(notificationScopesFromResponse(parseObject(R"({"scopes": {}})")).isEmpty());
    QVERIFY(notificationScopesFromResponse(parseObject(R"({"scopes": "[]"})")).isEmpty());
    QVERIFY(notificationScopesFromResponse(parseObject(R"({"scopes": []})")).isEmpty());
}

void NotificationScopesTest::roundTripsScopeThroughSetRequest()
{
    NotificationScope original;
    original.scopeId = 77;
    original.title = QStringLiteral("  Travel  ");
    original.muted = true;
    original.showPreview = false;
    original.sound = false;

    QJsonObject entry = serialized(setNotificationScopeRequest(original));
    entry.remove(QStringLiteral("@type"));

    const QVector<NotificationScope> parsed =
        notificationScopesFromResponse(responseWithScopes(QJsonArray{entry}));

    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed.at(0).scopeId, original.scopeId);
    QCOMPARE(parsed.at(0).title, original.title.trimmed());
    QCOMPARE(parsed.at(0).muted, original.muted);
    QCOMPARE(parsed.at(0).showPreview, original.showPreview);
    QCOMPARE(parsed.at(0).sound, original.sound);
}

QTEST_GUILESS_MAIN(NotificationScopesTest)
#include "tst_notification_scopes.moc"
