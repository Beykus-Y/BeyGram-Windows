#include "core/Logging.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class LoggingTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void writesRedactedLinesToFile();
    void keepsBoundedRecentMessages();
    void reportsLogPath();

private:
    QTemporaryDir m_directory;
};

void LoggingTest::init()
{
    QVERIFY(m_directory.isValid());
    beygram::logging::initialize(m_directory.path(), true);
}

void LoggingTest::cleanup()
{
    beygram::logging::shutdown();
}

void LoggingTest::writesRedactedLinesToFile()
{
    qWarning("authorization token=super-secret-value");
    qInfo("ordinary line");

    const QStringList recent = beygram::logging::recentMessages();
    QVERIFY(!recent.isEmpty());

    const QString joined = recent.join(QLatin1Char('\n'));
    QVERIFY(joined.contains(QStringLiteral("ordinary line")));
    QVERIFY(!joined.contains(QStringLiteral("super-secret-value")));
    QVERIFY(joined.contains(QStringLiteral("[redacted]")));

    QFile file(beygram::logging::logFilePath());
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY(contents.contains(QStringLiteral("ordinary line")));
    QVERIFY(!contents.contains(QStringLiteral("super-secret-value")));
}

void LoggingTest::keepsBoundedRecentMessages()
{
    for (int index = 0; index < 260; ++index)
        qInfo("line %d", index);

    const QStringList recent = beygram::logging::recentMessages();
    QVERIFY(recent.size() <= 200);
    QVERIFY(recent.last().contains(QStringLiteral("line 259")));
}

void LoggingTest::reportsLogPath()
{
    QVERIFY(!beygram::logging::logFilePath().isEmpty());
    QVERIFY(beygram::logging::isVerbose());
}

QTEST_GUILESS_MAIN(LoggingTest)
#include "tst_logging.moc"
