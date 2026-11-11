#include "telegram/SearchResultModel.h"

#include <QtTest>

using namespace beygram::telegram;

class SearchResultModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesHitRoles();
    void clearsHits();
};

void SearchResultModelTest::exposesHitRoles()
{
    SearchResultModel model;
    SearchHit hit;
    hit.chatId = 42;
    hit.messageId = 900;
    hit.date = 1700000000;
    hit.chatTitle = QStringLiteral("Alice");
    hit.senderName = QStringLiteral("Alice");
    hit.text = QStringLiteral("hello");
    hit.textHtml = QStringLiteral("hello");

    model.setHits({hit});
    QCOMPARE(model.count(), 1);
    const QModelIndex index = model.index(0);
    QCOMPARE(index.data(SearchResultModel::ChatIdRole).toLongLong(), qint64(42));
    QCOMPARE(index.data(SearchResultModel::MessageIdRole).toLongLong(), qint64(900));
    QCOMPARE(index.data(SearchResultModel::TextRole).toString(), QStringLiteral("hello"));
    QCOMPARE(index.data(SearchResultModel::ChatTitleRole).toString(), QStringLiteral("Alice"));
}

void SearchResultModelTest::clearsHits()
{
    SearchResultModel model;
    SearchHit hit;
    hit.chatId = 1;
    hit.messageId = 2;
    hit.text = QStringLiteral("x");
    model.setHits({hit});
    QCOMPARE(model.count(), 1);
    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(SearchResultModelTest)
#include "tst_search_model.moc"
