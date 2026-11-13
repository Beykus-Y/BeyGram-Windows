#include "telegram/EditHistoryModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using beygram::telegram::EditHistoryModel;
using beygram::telegram::EditVersion;

namespace {

QStringList textsOf(const QVector<EditVersion>& versions)
{
    QStringList texts;
    texts.reserve(versions.size());
    for (const EditVersion& version : versions)
        texts.append(version.text);
    return texts;
}

} // namespace

class EditHistoryModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void recordIgnoresNonPositiveMessageId();
    void recordIgnoresNonPositiveEditedAt();
    void recordStoresFirstVersion();
    void recordKeepsTextVerbatim();
    void recordAppendsVersionsInOrder();
    void recordIgnoresConsecutiveDuplicateText();
    void recordAllowsRepeatedNonConsecutiveText();
    void recordAcceptsEmptyText();
    void recordCapsOldestVersions();
    void recordDefaultCapIsTwenty();
    void recordClampsNonPositiveMaxVersions();
    void recordCapOfOneKeepsLatest();
    void messagesAreIndependent();
    void versionsReturnsIndependentCopy();
    void clearMessageRemovesOnlyThatMessage();
    void clearMessageMissingIsNoOp();
    void clearRemovesEveryMessage();
    void clearOnEmptyDoesNotSignal();
    void recordAfterClearMessageStartsFresh();
    void changedOnlyOnRealChanges();
};

void EditHistoryModelTest::startsEmpty()
{
    EditHistoryModel model;

    QCOMPARE(model.versionCount(1), 0);
    QVERIFY(model.versions(1).isEmpty());
    QVERIFY(model.originalText(1).isEmpty());
    QVERIFY(model.latestText(1).isEmpty());

    QVERIFY(model.versions(0).isEmpty());
    QVERIFY(model.versions(-3).isEmpty());
    QCOMPARE(model.versionCount(0), 0);
}

void EditHistoryModelTest::recordIgnoresNonPositiveMessageId()
{
    EditHistoryModel model;
    QSignalSpy spy(&model, &EditHistoryModel::changed);

    model.record(0, QStringLiteral("zero"), 10);
    model.record(-1, QStringLiteral("negative"), 10);
    model.record(-999, QStringLiteral("far"), 10);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.versionCount(0), 0);
    QCOMPARE(model.versionCount(-1), 0);
    QVERIFY(model.versions(0).isEmpty());
    QVERIFY(model.latestText(-1).isEmpty());
}

void EditHistoryModelTest::recordIgnoresNonPositiveEditedAt()
{
    EditHistoryModel model;
    QSignalSpy spy(&model, &EditHistoryModel::changed);

    model.record(1, QStringLiteral("no clock"), 0);
    model.record(1, QStringLiteral("before epoch"), -5);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.versionCount(1), 0);
    QVERIFY(model.versions(1).isEmpty());
    QVERIFY(model.originalText(1).isEmpty());
    QVERIFY(model.latestText(1).isEmpty());
}

void EditHistoryModelTest::recordStoresFirstVersion()
{
    EditHistoryModel model;
    QSignalSpy spy(&model, &EditHistoryModel::changed);

    model.record(42, QStringLiteral("hello"), 1700000000);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.versionCount(42), 1);
    QCOMPARE(model.originalText(42), QStringLiteral("hello"));
    QCOMPARE(model.latestText(42), QStringLiteral("hello"));

    const QVector<EditVersion> versions = model.versions(42);
    QCOMPARE(int(versions.size()), 1);
    QCOMPARE(versions.at(0).text, QStringLiteral("hello"));
    QCOMPARE(versions.at(0).editedAt, qint64(1700000000));
}

void EditHistoryModelTest::recordKeepsTextVerbatim()
{
    EditHistoryModel model;
    const QString text = QStringLiteral("  leading\nsecond line\ttrailing  ");

    model.record(5, text, 7);

    QCOMPARE(model.originalText(5), text);
    QCOMPARE(model.latestText(5), text);
    QCOMPARE(model.versions(5).at(0).text, text);
    QVERIFY(model.versions(5).at(0).text.startsWith(QStringLiteral("  ")));
    QVERIFY(model.versions(5).at(0).text.endsWith(QStringLiteral("  ")));
    QCOMPARE(model.versions(5).at(0).text.size(), text.size());
}

void EditHistoryModelTest::recordAppendsVersionsInOrder()
{
    EditHistoryModel model;

    model.record(3, QStringLiteral("one"), 100);
    model.record(3, QStringLiteral("two"), 200);
    model.record(3, QStringLiteral("three"), 300);

    QCOMPARE(model.versionCount(3), 3);
    QCOMPARE(model.originalText(3), QStringLiteral("one"));
    QCOMPARE(model.latestText(3), QStringLiteral("three"));

    const QVector<EditVersion> versions = model.versions(3);
    const QStringList expected{QStringLiteral("one"), QStringLiteral("two"),
                               QStringLiteral("three")};
    QCOMPARE(textsOf(versions), expected);
    QCOMPARE(versions.at(0).editedAt, qint64(100));
    QCOMPARE(versions.at(1).editedAt, qint64(200));
    QCOMPARE(versions.at(2).editedAt, qint64(300));
}

void EditHistoryModelTest::recordIgnoresConsecutiveDuplicateText()
{
    EditHistoryModel model;
    model.record(8, QStringLiteral("same"), 10);

    QSignalSpy spy(&model, &EditHistoryModel::changed);
    model.record(8, QStringLiteral("same"), 20);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.versionCount(8), 1);
    QCOMPARE(model.versions(8).at(0).editedAt, qint64(10));
}

void EditHistoryModelTest::recordAllowsRepeatedNonConsecutiveText()
{
    EditHistoryModel model;

    model.record(9, QStringLiteral("a"), 1);
    model.record(9, QStringLiteral("b"), 2);
    model.record(9, QStringLiteral("a"), 3);

    QCOMPARE(model.versionCount(9), 3);
    const QStringList expected{QStringLiteral("a"), QStringLiteral("b"),
                               QStringLiteral("a")};
    QCOMPARE(textsOf(model.versions(9)), expected);
    QCOMPARE(model.originalText(9), QStringLiteral("a"));
    QCOMPARE(model.latestText(9), QStringLiteral("a"));
}

void EditHistoryModelTest::recordAcceptsEmptyText()
{
    EditHistoryModel model;
    QSignalSpy spy(&model, &EditHistoryModel::changed);

    model.record(4, QString(), 10);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.versionCount(4), 1);
    QVERIFY(model.originalText(4).isEmpty());
    QVERIFY(model.latestText(4).isEmpty());
    QCOMPARE(model.versions(4).at(0).text, QString());

    model.record(4, QString(), 20); // consecutive duplicate
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.versionCount(4), 1);

    model.record(4, QStringLiteral("x"), 30);
    QCOMPARE(model.versionCount(4), 2);
    QCOMPARE(model.latestText(4), QStringLiteral("x"));

    model.record(4, QString(), 40);
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.versionCount(4), 3);
    QVERIFY(model.latestText(4).isEmpty());
    QCOMPARE(model.originalText(4), QString());
}

void EditHistoryModelTest::recordCapsOldestVersions()
{
    EditHistoryModel model;

    model.record(1, QStringLiteral("v0"), 1, 3);
    model.record(1, QStringLiteral("v1"), 2, 3);
    model.record(1, QStringLiteral("v2"), 3, 3);
    model.record(1, QStringLiteral("v3"), 4, 3);

    QSignalSpy spy(&model, &EditHistoryModel::changed);
    model.record(1, QStringLiteral("v4"), 5, 3);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.versionCount(1), 3);
    const QStringList expected{QStringLiteral("v2"), QStringLiteral("v3"),
                               QStringLiteral("v4")};
    QCOMPARE(textsOf(model.versions(1)), expected);
    QCOMPARE(model.originalText(1), QStringLiteral("v2"));
    QCOMPARE(model.latestText(1), QStringLiteral("v4"));
    QCOMPARE(model.versions(1).at(0).editedAt, qint64(3));
    QCOMPARE(model.versions(1).at(2).editedAt, qint64(5));
}

void EditHistoryModelTest::recordDefaultCapIsTwenty()
{
    EditHistoryModel model;

    for (int i = 0; i < 25; ++i)
        model.record(2, QStringLiteral("v%1").arg(i), i + 1);

    QCOMPARE(model.versionCount(2), 20);
    QCOMPARE(model.originalText(2), QStringLiteral("v5"));
    QCOMPARE(model.latestText(2), QStringLiteral("v24"));

    const QVector<EditVersion> versions = model.versions(2);
    QCOMPARE(versions.at(0).editedAt, qint64(6));
    QCOMPARE(versions.constLast().editedAt, qint64(25));
}

void EditHistoryModelTest::recordClampsNonPositiveMaxVersions()
{
    EditHistoryModel model;

    model.record(6, QStringLiteral("a"), 1, 0);
    QCOMPARE(model.versionCount(6), 1);
    QCOMPARE(model.latestText(6), QStringLiteral("a"));

    model.record(6, QStringLiteral("b"), 2, -10);
    QCOMPARE(model.versionCount(6), 1);
    QCOMPARE(model.originalText(6), QStringLiteral("b"));
    QCOMPARE(model.latestText(6), QStringLiteral("b"));
    QCOMPARE(model.versions(6).at(0).editedAt, qint64(2));
}

void EditHistoryModelTest::recordCapOfOneKeepsLatest()
{
    EditHistoryModel model;
    QSignalSpy spy(&model, &EditHistoryModel::changed);

    model.record(7, QStringLiteral("old"), 1, 1);
    QCOMPARE(model.versionCount(7), 1);

    model.record(7, QStringLiteral("new"), 2, 1);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.versionCount(7), 1);
    QCOMPARE(model.originalText(7), QStringLiteral("new"));
    QCOMPARE(model.latestText(7), QStringLiteral("new"));
    QCOMPARE(model.versions(7).at(0).editedAt, qint64(2));
}

void EditHistoryModelTest::messagesAreIndependent()
{
    EditHistoryModel model;

    model.record(1, QStringLiteral("one-a"), 1);
    model.record(2, QStringLiteral("two-a"), 2);
    model.record(1, QStringLiteral("one-b"), 3);

    const QStringList first{QStringLiteral("one-a"), QStringLiteral("one-b")};
    QCOMPARE(textsOf(model.versions(1)), first);
    const QStringList second{QStringLiteral("two-a")};
    QCOMPARE(textsOf(model.versions(2)), second);
    QCOMPARE(model.originalText(2), QStringLiteral("two-a"));
    QCOMPARE(model.latestText(2), QStringLiteral("two-a"));
}

void EditHistoryModelTest::versionsReturnsIndependentCopy()
{
    EditHistoryModel model;
    model.record(1, QStringLiteral("a"), 1);

    QVector<EditVersion> copy = model.versions(1);
    QCOMPARE(int(copy.size()), 1);
    copy[0].text = QStringLiteral("mutated");
    copy.append(EditVersion{QStringLiteral("b"), 2});

    QCOMPARE(model.versionCount(1), 1);
    QCOMPARE(model.versions(1).at(0).text, QStringLiteral("a"));
    QCOMPARE(model.latestText(1), QStringLiteral("a"));
}

void EditHistoryModelTest::clearMessageRemovesOnlyThatMessage()
{
    EditHistoryModel model;
    model.record(1, QStringLiteral("a"), 1);
    model.record(2, QStringLiteral("b"), 2);

    QSignalSpy spy(&model, &EditHistoryModel::changed);
    model.clearMessage(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.versionCount(1), 0);
    QVERIFY(model.versions(1).isEmpty());
    QVERIFY(model.originalText(1).isEmpty());
    QCOMPARE(model.versionCount(2), 1);
    QCOMPARE(model.latestText(2), QStringLiteral("b"));
}

void EditHistoryModelTest::clearMessageMissingIsNoOp()
{
    EditHistoryModel model;
    model.record(1, QStringLiteral("a"), 1);

    QSignalSpy spy(&model, &EditHistoryModel::changed);
    model.clearMessage(2);
    model.clearMessage(0);
    model.clearMessage(-7);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.versionCount(1), 1);
    QCOMPARE(model.latestText(1), QStringLiteral("a"));
}

void EditHistoryModelTest::clearRemovesEveryMessage()
{
    EditHistoryModel model;
    model.record(1, QStringLiteral("a"), 1);
    model.record(2, QStringLiteral("b"), 2);

    QSignalSpy spy(&model, &EditHistoryModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.versionCount(1), 0);
    QCOMPARE(model.versionCount(2), 0);
    QVERIFY(model.versions(1).isEmpty());
    QVERIFY(model.latestText(2).isEmpty());
}

void EditHistoryModelTest::clearOnEmptyDoesNotSignal()
{
    EditHistoryModel model;
    QSignalSpy spy(&model, &EditHistoryModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
}

void EditHistoryModelTest::recordAfterClearMessageStartsFresh()
{
    EditHistoryModel model;
    model.record(1, QStringLiteral("a"), 1);
    model.record(1, QStringLiteral("b"), 2);
    model.clearMessage(1);

    QSignalSpy spy(&model, &EditHistoryModel::changed);
    model.record(1, QStringLiteral("b"), 3);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.versionCount(1), 1);
    QCOMPARE(model.originalText(1), QStringLiteral("b"));
    QCOMPARE(model.latestText(1), QStringLiteral("b"));
    QCOMPARE(model.versions(1).at(0).editedAt, qint64(3));
}

void EditHistoryModelTest::changedOnlyOnRealChanges()
{
    EditHistoryModel model;
    QSignalSpy spy(&model, &EditHistoryModel::changed);

    model.record(1, QStringLiteral("a"), 1);   // new history
    model.record(1, QStringLiteral("a"), 2);   // duplicate text
    model.record(0, QStringLiteral("b"), 3);   // invalid id
    model.record(2, QStringLiteral("c"), 0);   // invalid timestamp
    model.record(2, QStringLiteral("c"), 4);   // new history
    model.record(1, QStringLiteral("b"), 5);   // appended version
    model.clearMessage(3);                     // absent message
    model.clearMessage(2);                     // present message
    model.clearMessage(2);                     // already gone
    model.clear();                             // one message left
    model.clear();                             // already empty

    QCOMPARE(spy.count(), 5);
}

QTEST_GUILESS_MAIN(EditHistoryModelTest)
#include "tst_edit_history_model.moc"
