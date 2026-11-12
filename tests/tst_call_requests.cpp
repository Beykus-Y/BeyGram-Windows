#include "telegram/CallRequests.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    return QJsonDocument::fromJson(QJsonDocument(object).toJson(QJsonDocument::Compact))
        .object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonObject callProtocol()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("callProtocol")},
        {QStringLiteral("udp_p2p"), true},
        {QStringLiteral("udp_reflector"), false},
        {QStringLiteral("min_layer"), 65},
        {QStringLiteral("max_layer"), 92},
        {QStringLiteral("library_versions"), QJsonArray{QStringLiteral("2.4.4")}},
    };
}

QString emoji()
{
    return QString::fromUtf8("\xF0\x9F\x98\x80");
}

} // namespace

class CallRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsGetCallRequest();
    void rejectsInvalidCallIds();
    void passesProtocolThrough();
    void buildsDiscardCallRequest();
    void clampsCallDuration();
    void clampsCallRating();
    void trimsAndCapsCallComment();
    void keepsEmptyCallComment();
    void sendsDebugInformationVerbatim();
    void trimsCallLogPath();
    void omitsExtraAndRoundTripsJson();
};

void CallRequestsTest::buildsGetCallRequest()
{
    const QJsonObject request = serialized(getCallRequest(1234567890123LL));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getCall"));
    QCOMPARE(numberAt(request, QStringLiteral("call_id")), qint64(1234567890123LL));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void CallRequestsTest::rejectsInvalidCallIds()
{
    const QJsonObject protocol = callProtocol();

    QVERIFY(getCallRequest(0).isEmpty());
    QVERIFY(getCallRequest(-1).isEmpty());
    QVERIFY(acceptCallRequest(0, protocol).isEmpty());
    QVERIFY(acceptCallRequest(-7, protocol).isEmpty());
    QVERIFY(discardCallRequest(0, true, 10, true).isEmpty());
    QVERIFY(discardCallRequest(-3, false, 0, false).isEmpty());
    QVERIFY(sendCallRatingRequest(0, 5, QStringLiteral("good")).isEmpty());
    QVERIFY(sendCallRatingRequest(-1, 1, QString()).isEmpty());
    QVERIFY(sendCallDebugInformationRequest(0, QStringLiteral("debug")).isEmpty());
    QVERIFY(sendCallDebugInformationRequest(-2, QString()).isEmpty());
}

void CallRequestsTest::passesProtocolThrough()
{
    const QJsonObject protocol = callProtocol();
    const QJsonObject request = serialized(acceptCallRequest(42, protocol));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("acceptCall"));
    QCOMPARE(numberAt(request, QStringLiteral("call_id")), qint64(42));
    QCOMPARE(request.value(QStringLiteral("protocol")).toObject(),
             serialized(protocol));
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    // The builder does not require a populated protocol; the caller resolves it
    // before accepting, and an empty object is still forwarded unchanged.
    const QJsonObject emptyProtocol = serialized(acceptCallRequest(7, QJsonObject{}));
    QVERIFY(emptyProtocol.value(QStringLiteral("protocol")).isObject());
    QCOMPARE(emptyProtocol.value(QStringLiteral("protocol")).toObject().size(), 0);
}

void CallRequestsTest::buildsDiscardCallRequest()
{
    const QJsonObject request = serialized(discardCallRequest(77, true, 12, true));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("discardCall"));
    QCOMPARE(numberAt(request, QStringLiteral("call_id")), qint64(77));
    QCOMPARE(request.value(QStringLiteral("is_disconnected")).toBool(false), true);
    QCOMPARE(request.value(QStringLiteral("duration")).toInt(), 12);
    QCOMPARE(request.value(QStringLiteral("is_video")).toBool(false), true);
    QCOMPARE(request.size(), 5);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    // A locally ended audio call keeps both flags false.
    const QJsonObject ended = serialized(discardCallRequest(78, false, 0, false));
    QCOMPARE(ended.value(QStringLiteral("is_disconnected")).toBool(true), false);
    QCOMPARE(ended.value(QStringLiteral("is_video")).toBool(true), false);
    QCOMPARE(ended.value(QStringLiteral("duration")).toInt(), 0);
}

void CallRequestsTest::clampsCallDuration()
{
    const QVector<QPair<int, int>> cases{
        {std::numeric_limits<int>::min(), 0},
        {-1, 0},
        {0, 0},
        {1, 1},
        {3600, 3600},
        {86400, 86400},
        {86401, 86400},
        {std::numeric_limits<int>::max(), 86400},
    };

    for (const auto& testCase : cases) {
        const QJsonObject request =
            serialized(discardCallRequest(9, false, testCase.first, false));
        QCOMPARE(request.value(QStringLiteral("duration")).toInt(), testCase.second);
    }
}

void CallRequestsTest::clampsCallRating()
{
    const QVector<QPair<int, int>> cases{
        {std::numeric_limits<int>::min(), 1},
        {-5, 1},
        {0, 1},
        {1, 1},
        {3, 3},
        {5, 5},
        {6, 5},
        {100, 5},
        {std::numeric_limits<int>::max(), 5},
    };

    for (const auto& testCase : cases) {
        const QJsonObject request =
            serialized(sendCallRatingRequest(9, testCase.first, QString()));
        QCOMPARE(request.value(QStringLiteral("rating")).toInt(), testCase.second);
    }
}

void CallRequestsTest::trimsAndCapsCallComment()
{
    const QJsonObject trimmed = serialized(
        sendCallRatingRequest(5, 4, QStringLiteral("  thanks for the call\n\t ")));
    QCOMPARE(trimmed.value(QStringLiteral("comment")).toString(),
             QStringLiteral("thanks for the call"));
    QCOMPARE(trimmed.size(), 4);

    // Exactly 4096 characters fit unchanged.
    const QString atLimit(4096, QLatin1Char('x'));
    QCOMPARE(sendCallRatingRequest(5, 4, atLimit)
                 .value(QStringLiteral("comment"))
                 .toString(),
             atLimit);

    // Anything longer is cut, not rejected.
    const QString tooLong(5000, QLatin1Char('x'));
    QCOMPARE(sendCallRatingRequest(5, 4, tooLong)
                 .value(QStringLiteral("comment"))
                 .toString()
                 .size(),
             4096);

    // The cut counts code points, so an emoji is never split in half.
    QString manyEmoji;
    for (int i = 0; i < 4100; ++i)
        manyEmoji += emoji();
    const QString capped = sendCallRatingRequest(5, 4, manyEmoji)
                               .value(QStringLiteral("comment"))
                               .toString();
    QCOMPARE(capped.size(), 8192);
    QVERIFY(capped.at(capped.size() - 2).isHighSurrogate());
    QVERIFY(capped.at(capped.size() - 1).isLowSurrogate());

    // A surrogate pair that straddles the limit is dropped as a whole.
    const QString boundary = QString(4095, QLatin1Char('x')) + emoji()
        + QLatin1Char('y');
    const QString boundaryCapped = sendCallRatingRequest(5, 4, boundary)
                                       .value(QStringLiteral("comment"))
                                       .toString();
    QCOMPARE(boundaryCapped.size(), 4097);
    QVERIFY(boundaryCapped.at(boundaryCapped.size() - 1).isLowSurrogate());
    QVERIFY(!boundaryCapped.endsWith(QLatin1Char('y')));
}

void CallRequestsTest::keepsEmptyCallComment()
{
    const QJsonObject whitespace =
        serialized(sendCallRatingRequest(9, 5, QStringLiteral("   \n\t")));
    QVERIFY(whitespace.contains(QStringLiteral("comment")));
    QVERIFY(whitespace.value(QStringLiteral("comment")).isString());
    QCOMPARE(whitespace.value(QStringLiteral("comment")).toString(), QString());
    QCOMPARE(whitespace.value(QStringLiteral("rating")).toInt(), 5);
    QCOMPARE(whitespace.size(), 4);

    const QJsonObject empty = serialized(sendCallRatingRequest(9, 1, QString()));
    QVERIFY(empty.contains(QStringLiteral("comment")));
    QCOMPARE(empty.value(QStringLiteral("comment")).toString(), QString());
}

void CallRequestsTest::sendsDebugInformationVerbatim()
{
    const QString debug = QStringLiteral("  first line\nsecond line\t ");
    const QJsonObject request = serialized(sendCallDebugInformationRequest(11, debug));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendCallDebugInformation"));
    QCOMPARE(numberAt(request, QStringLiteral("call_id")), qint64(11));
    // Debug text is machine output; the builder must not reformat it.
    QCOMPARE(request.value(QStringLiteral("debug_information")).toString(), debug);
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QJsonObject empty = serialized(sendCallDebugInformationRequest(11, QString()));
    QVERIFY(empty.contains(QStringLiteral("debug_information")));
    QCOMPARE(empty.value(QStringLiteral("debug_information")).toString(), QString());
}

void CallRequestsTest::trimsCallLogPath()
{
    QVERIFY(sendCallLogRequest(QString()).isEmpty());
    QVERIFY(sendCallLogRequest(QStringLiteral("   \t\n")).isEmpty());

    const QJsonObject request =
        serialized(sendCallLogRequest(QStringLiteral("  C:\\Program Files\\call.log  ")));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendCallLog"));
    QCOMPARE(request.value(QStringLiteral("log_file")).toString(),
             QStringLiteral("C:\\Program Files\\call.log"));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void CallRequestsTest::omitsExtraAndRoundTripsJson()
{
    const QVector<QJsonObject> requests{
        getCallRequest(1234567890123LL),
        acceptCallRequest(1234567890123LL, callProtocol()),
        discardCallRequest(1234567890123LL, true, 86400, true),
        sendCallRatingRequest(1234567890123LL, 5, QStringLiteral("great")),
        sendCallDebugInformationRequest(1234567890123LL, QStringLiteral("debug")),
        sendCallLogRequest(QStringLiteral("call.log")),
    };

    for (const QJsonObject& request : requests) {
        QVERIFY(!request.isEmpty());
        QVERIFY(!request.contains(QStringLiteral("@extra")));

        const QJsonObject wire = serialized(request);
        QCOMPARE(wire.value(QStringLiteral("@type")).toString(),
                 request.value(QStringLiteral("@type")).toString());
        QCOMPARE(wire.size(), request.size());
    }

    // Call ids are int64 and must survive the JSON encoding intact.
    QCOMPARE(numberAt(serialized(getCallRequest(9007199254740991LL)),
                      QStringLiteral("call_id")),
             qint64(9007199254740991LL));
}

QTEST_GUILESS_MAIN(CallRequestsTest)
#include "tst_call_requests.moc"
