#include "telegram/TelegramSession.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using namespace beygram::telegram;

class TelegramSessionTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void reportsMissingRuntimeHonestly();
    void rejectsInvalidPhoneWithoutTdlib();
    void shutdownWithoutClientIsSafe();

private:
    QByteArray m_savedOverride;
};

void TelegramSessionTest::init()
{
    m_savedOverride = qgetenv("BEYGRAM_TDLIB_DLL");
}

void TelegramSessionTest::cleanup()
{
    if (m_savedOverride.isEmpty())
        qunsetenv("BEYGRAM_TDLIB_DLL");
    else
        qputenv("BEYGRAM_TDLIB_DLL", m_savedOverride);
}

void TelegramSessionTest::reportsMissingRuntimeHonestly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    qputenv("BEYGRAM_TDLIB_DLL",
            QDir(directory.path()).filePath(QStringLiteral("does-not-exist/tdjson.dll")).toUtf8());

    TelegramConfig config;
    config.credentials.apiId = 1;
    config.credentials.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");
    config.accountDirectory = directory.path();
    config.databaseDirectory = QDir(directory.path()).filePath(QStringLiteral("database"));
    config.filesDirectory = QDir(directory.path()).filePath(QStringLiteral("files"));
    config.databaseEncryptionKey = QString(64, QLatin1Char('a'));

    TelegramSession session(config);
    QSignalSpy spy(&session, &TelegramSession::stageChanged);
    session.start();

    QCOMPARE(session.stage(), AuthStage::Unavailable);
    QVERIFY(!session.stageDetail().isEmpty());
    QVERIFY(!session.isRunning());
    QVERIFY(spy.count() >= 1);
}

void TelegramSessionTest::rejectsInvalidPhoneWithoutTdlib()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    qputenv("BEYGRAM_TDLIB_DLL",
            QDir(directory.path()).filePath(QStringLiteral("does-not-exist/tdjson.dll")).toUtf8());

    TelegramConfig config;
    config.credentials.apiId = 1;
    config.credentials.apiHash = QStringLiteral("0123456789abcdef0123456789abcdef");
    config.accountDirectory = directory.path();
    config.databaseDirectory = QDir(directory.path()).filePath(QStringLiteral("database"));
    config.filesDirectory = QDir(directory.path()).filePath(QStringLiteral("files"));
    config.databaseEncryptionKey = QString(64, QLatin1Char('a'));

    TelegramSession session(config);
    session.start();
    session.submitPhoneNumber(QStringLiteral("not a phone"));
    QVERIFY(session.lastError().contains(QStringLiteral("international format")));
}

void TelegramSessionTest::shutdownWithoutClientIsSafe()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    qputenv("BEYGRAM_TDLIB_DLL",
            QDir(directory.path()).filePath(QStringLiteral("does-not-exist/tdjson.dll")).toUtf8());

    TelegramConfig config;
    config.accountDirectory = directory.path();
    config.databaseDirectory = QDir(directory.path()).filePath(QStringLiteral("database"));
    config.filesDirectory = QDir(directory.path()).filePath(QStringLiteral("files"));
    config.databaseEncryptionKey = QString(64, QLatin1Char('a'));

    TelegramSession session(config);
    session.start();
    QVERIFY(session.shutdown(100));
    QVERIFY(!session.isRunning());
}

QTEST_GUILESS_MAIN(TelegramSessionTest)
#include "tst_telegram_session.moc"
