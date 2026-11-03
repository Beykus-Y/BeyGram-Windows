#include "core/Redaction.h"

#include <QtTest>

using beygram::logging::redact;

class RedactionTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsHarmlessText();
    void redactsKeyValueSecrets();
    void redactsJsonSecrets();
    void redactsTelegramLoginLink();
    void redactsPhoneNumbers();
    void redactsBotTokens();
    void redactsApiHash();
    void doesNotRedactMessageIds();
};

void RedactionTest::keepsHarmlessText()
{
    const QString input = QStringLiteral("Connected to Telegram in 412 ms (attempt 2)");
    QCOMPARE(redact(input), input);
}

void RedactionTest::redactsKeyValueSecrets()
{
    const QString text = redact(QStringLiteral("api_hash=0123456789abcdef password: hunter2"));
    QVERIFY(!text.contains(QStringLiteral("0123456789abcdef")));
    QVERIFY(!text.contains(QStringLiteral("hunter2")));
    QVERIFY(text.contains(QStringLiteral("[redacted]")));
}

void RedactionTest::redactsJsonSecrets()
{
    const QString text =
        redact(QStringLiteral(R"({"token":"abc123","note":"visible"})"));
    QVERIFY(!text.contains(QStringLiteral("abc123")));
    QVERIFY(text.contains(QStringLiteral("visible")));
}

void RedactionTest::redactsTelegramLoginLink()
{
    const QString text = redact(QStringLiteral("open tg://login?token=AAHsecretToken123 now"));
    QVERIFY(!text.contains(QStringLiteral("AAHsecretToken123")));
    QVERIFY(text.contains(QStringLiteral("tg://login?token=[redacted]")));
}

void RedactionTest::redactsPhoneNumbers()
{
    const QString text = redact(QStringLiteral("phone +7 999 123-45-67"));
    QVERIFY(!text.contains(QStringLiteral("123-45-67")));
    QVERIFY(text.contains(QStringLiteral("[redacted-phone]")));
}

void RedactionTest::redactsBotTokens()
{
    const QString text =
        redact(QStringLiteral("bot 123456789:AAHdqTcvCH1vGWJxfSeofSAs0K5PALDsaw"));
    QVERIFY(!text.contains(QStringLiteral("AAHdqTcvCH1vGWJxfSeofSAs0K5PALDsaw")));
    QVERIFY(text.contains(QStringLiteral("[redacted-bot-token]")));
}

void RedactionTest::redactsApiHash()
{
    const QString hash = QStringLiteral("0123456789abcdef0123456789abcdef");
    const QString text = redact(QStringLiteral("hash %1 here").arg(hash));
    QVERIFY(!text.contains(hash));
    QVERIFY(text.contains(QStringLiteral("[redacted-hex-secret]")));
}

void RedactionTest::doesNotRedactMessageIds()
{
    const QString input = QStringLiteral("message_id=123456 chat_id=-1001234567890");
    const QString text = redact(input);
    // chat ids are long numeric values and may be treated as phone-like;
    // message ids must survive because they are ordinary diagnostics.
    QVERIFY(text.contains(QStringLiteral("message_id=123456")));
}

QTEST_MAIN(RedactionTest)
#include "tst_redaction.moc"
