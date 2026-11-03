#include "telegram/TdJsonTransport.h"

#include <QtTest>

using beygram::telegram::TdJsonTransport;

class TdJsonTransportTest final : public QObject {
    Q_OBJECT

private slots:
    void reportsAvailabilityHonestly();
    void executeFailsWithoutRuntime();
    void sendWithoutClientEmitsError();
};

void TdJsonTransportTest::reportsAvailabilityHonestly()
{
    TdJsonTransport transport;
    QVERIFY(!transport.isRunning());

    QString error;
    const bool loaded = transport.load(&error);

    if (loaded) {
        QVERIFY(transport.isAvailable());
        QVERIFY(error.isEmpty());
    } else {
        QVERIFY(!transport.isAvailable());
        QVERIFY(!error.isEmpty());
    }
}

void TdJsonTransportTest::executeFailsWithoutRuntime()
{
    TdJsonTransport transport;
    QString error;
    if (transport.load(&error))
        QSKIP("TDLib is installed in this environment; the missing-runtime path is not exercised");

    const auto result = transport.execute(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("getOption")},
                    {QStringLiteral("name"), QStringLiteral("version")}});
    QVERIFY(!result.hasValue());
    QCOMPARE(result.error().code, beygram::ErrorCode::Unavailable);
    QVERIFY(!result.error().message.isEmpty());
}

void TdJsonTransportTest::sendWithoutClientEmitsError()
{
    TdJsonTransport transport;
    QSignalSpy spy(&transport, &TdJsonTransport::transportError);
    transport.send(QJsonObject{{QStringLiteral("@type"), QStringLiteral("getMe")}});
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(TdJsonTransportTest)
#include "tst_tdjson_transport.moc"
