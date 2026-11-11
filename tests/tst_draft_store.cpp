#include "telegram/DraftStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using beygram::telegram::Draft;
using beygram::telegram::DraftStore;

namespace {

QString draftPath(const QTemporaryDir& directory)
{
    return QDir(directory.path()).filePath(QStringLiteral("state/drafts.json"));
}

bool writeFile(const QString& path, const QByteArray& payload)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(payload) == payload.size();
}

} // namespace

class DraftStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void storesDraftWithReply();
    void emptyTextClearsOnce();
    void persistsAcrossInstances();
    void ignoresCorruptFile();
    void ignoresUnsupportedVersion();
    void dropsOldestDraftBeyondLimit();
    void removesChat();
    void draftsAreNewestFirst();
    void acceptsNumericIds();
};

void DraftStoreTest::startsEmpty()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DraftStore store(draftPath(directory));
    store.load();
    QCOMPARE(store.count(), 0);
    QVERIFY(store.drafts().isEmpty());
    QVERIFY(store.draft(12345).text.isEmpty());
    QCOMPARE(store.draft(12345).chatId, qint64(12345));
    QCOMPARE(store.filePath(), draftPath(directory));
}

void DraftStoreTest::storesDraftWithReply()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DraftStore store(draftPath(directory));
    store.load();
    QSignalSpy spy(&store, &DraftStore::draftChanged);

    store.setDraft(42, QStringLiteral("Hello there"), 77);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), qint64(42));

    const Draft draft = store.draft(42);
    QCOMPARE(draft.chatId, qint64(42));
    QCOMPARE(draft.replyToMessageId, qint64(77));
    QCOMPARE(draft.text, QStringLiteral("Hello there"));
    QVERIFY(draft.updatedAt > 0);
    QCOMPARE(store.count(), 1);

    // Rewriting the same content is not a change.
    store.setDraft(42, QStringLiteral("Hello there"), 77);
    QCOMPARE(spy.count(), 1);

    store.setDraft(42, QStringLiteral("Hello there"), 78);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(store.draft(42).replyToMessageId, qint64(78));
}

void DraftStoreTest::emptyTextClearsOnce()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DraftStore store(draftPath(directory));
    store.load();
    store.setDraft(7, QStringLiteral("to be dropped"), 3);

    QSignalSpy spy(&store, &DraftStore::draftChanged);
    store.setDraft(7, QStringLiteral("   "));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), qint64(7));
    QCOMPARE(store.count(), 0);
    QVERIFY(store.draft(7).text.isEmpty());

    // Clearing an already absent draft changes nothing.
    store.setDraft(7, QString());
    QCOMPARE(spy.count(), 1);
}

void DraftStoreTest::persistsAcrossInstances()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = draftPath(directory);
    {
        DraftStore store(path);
        store.load();
        store.setDraft(1, QStringLiteral("first draft"), 5);
        store.setDraft(2, QStringLiteral("second draft"));
    }
    QVERIFY(QFile::exists(path));

    DraftStore reloaded(path);
    reloaded.load();
    QCOMPARE(reloaded.count(), 2);
    QCOMPARE(reloaded.draft(1).text, QStringLiteral("first draft"));
    QCOMPARE(reloaded.draft(1).replyToMessageId, qint64(5));
    QCOMPARE(reloaded.draft(2).text, QStringLiteral("second draft"));
    QCOMPARE(reloaded.draft(2).replyToMessageId, qint64(0));

    reloaded.clearDraft(1);
    DraftStore again(path);
    again.load();
    QCOMPARE(again.count(), 1);
    QVERIFY(again.draft(1).text.isEmpty());
    QCOMPARE(again.draft(2).text, QStringLiteral("second draft"));
}

void DraftStoreTest::ignoresCorruptFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = draftPath(directory);
    QVERIFY(writeFile(path, QByteArrayLiteral("{ this is not json")));

    DraftStore store(path);
    store.load();
    QCOMPARE(store.count(), 0);
    QVERIFY(store.drafts().isEmpty());

    // The store stays usable and rewrites a valid file.
    store.setDraft(9, QStringLiteral("recovered"));
    QCOMPARE(store.count(), 1);

    DraftStore reloaded(path);
    reloaded.load();
    QCOMPARE(reloaded.count(), 1);
    QCOMPARE(reloaded.draft(9).text, QStringLiteral("recovered"));
}

void DraftStoreTest::ignoresUnsupportedVersion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = draftPath(directory);
    QVERIFY(writeFile(path, QByteArrayLiteral(
                             R"({"version":2,"drafts":[{"chatId":"5","text":"future","updatedAt":10}]})")));

    DraftStore store(path);
    store.load();
    QCOMPARE(store.count(), 0);
    QVERIFY(store.draft(5).text.isEmpty());
}

void DraftStoreTest::dropsOldestDraftBeyondLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = draftPath(directory);
    DraftStore store(path);
    store.load();
    for (int i = 1; i <= 501; ++i)
        store.setDraft(i, QStringLiteral("draft %1").arg(i));

    QCOMPARE(store.count(), 500);
    QVERIFY(store.draft(1).text.isEmpty());
    QCOMPARE(store.draft(501).text, QStringLiteral("draft 501"));
    QCOMPARE(store.draft(2).text, QStringLiteral("draft 2"));

    DraftStore reloaded(path);
    reloaded.load();
    QCOMPARE(reloaded.count(), 500);
    QVERIFY(reloaded.draft(1).text.isEmpty());
    QCOMPARE(reloaded.draft(501).text, QStringLiteral("draft 501"));
}

void DraftStoreTest::removesChat()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    DraftStore store(draftPath(directory));
    store.load();
    store.setDraft(3, QStringLiteral("keep me"));
    store.setDraft(4, QStringLiteral("remove me"));

    QSignalSpy spy(&store, &DraftStore::draftChanged);
    store.removeChat(4);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), qint64(4));
    QCOMPARE(store.count(), 1);
    QVERIFY(store.draft(4).text.isEmpty());
    QCOMPARE(store.draft(3).text, QStringLiteral("keep me"));

    // Removing an unknown chat is a no-op.
    store.removeChat(4);
    QCOMPARE(spy.count(), 1);
}

void DraftStoreTest::draftsAreNewestFirst()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = draftPath(directory);
    QVERIFY(writeFile(path, QByteArrayLiteral(R"({
        "version": 1,
        "drafts": [
            {"chatId": "100", "text": "oldest", "replyToMessageId": 0, "updatedAt": 1000},
            {"chatId": "200", "text": "newest", "replyToMessageId": 0, "updatedAt": 3000},
            {"chatId": "300", "text": "middle", "replyToMessageId": 0, "updatedAt": 2000}
        ]
    })")));

    DraftStore store(path);
    store.load();

    const QVector<Draft> ordered = store.drafts();
    QCOMPARE(ordered.size(), 3);
    QCOMPARE(ordered.at(0).chatId, qint64(200));
    QCOMPARE(ordered.at(1).chatId, qint64(300));
    QCOMPARE(ordered.at(2).chatId, qint64(100));

    // A freshly edited chat moves to the front.
    store.setDraft(100, QStringLiteral("oldest, edited now"));
    QCOMPARE(store.drafts().first().chatId, qint64(100));
}

void DraftStoreTest::acceptsNumericIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = draftPath(directory);
    QVERIFY(writeFile(path, QByteArrayLiteral(R"({
        "version": 1,
        "drafts": [
            {"chatId": 777, "text": "numeric chat", "replyToMessageId": "888", "updatedAt": "1234"}
        ]
    })")));

    DraftStore store(path);
    store.load();

    QCOMPARE(store.count(), 1);
    const Draft draft = store.draft(777);
    QCOMPARE(draft.chatId, qint64(777));
    QCOMPARE(draft.replyToMessageId, qint64(888));
    QCOMPARE(draft.text, QStringLiteral("numeric chat"));
    QCOMPARE(draft.updatedAt, qint64(1234));
}

QTEST_GUILESS_MAIN(DraftStoreTest)
#include "tst_draft_store.moc"
