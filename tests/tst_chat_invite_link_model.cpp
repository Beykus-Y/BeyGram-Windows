#include "telegram/ChatInviteLinkModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

InviteLinkRow makeLink(const QString& link, const QString& title = QString(),
                       bool revoked = false)
{
    InviteLinkRow row;
    row.link = link;
    row.title = title;
    row.revoked = revoked;
    return row;
}

QString firstLink()
{
    return QStringLiteral("https://t.me/+FirstLink");
}

QString secondLink()
{
    return QStringLiteral("https://t.me/+SecondLink");
}

QString thirdLink()
{
    return QStringLiteral("https://t.me/+ThirdLink");
}

} // namespace

class ChatInviteLinkModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setLinksKeepsOrder();
    void setLinksDropsEmptyLinks();
    void setLinksKeepsFirstDuplicate();
    void setLinksClampsNegativeMemberCount();
    void setLinksClampsNegativeMemberLimit();
    void setLinksKeepsOtherFields();
    void setLinksEmitsChangedOnce();
    void setLinksSameLinksIsSilent();
    void setLinksClampedDuplicateIsSilent();
    void setLinksReorderedIsAChange();
    void setLinksEmptyClearsAndEmits();
    void setLinksReplacesPrevious();
    void linksReturnsCopy();
    void countMatchesLinks();
    void activeExcludesRevoked();
    void activeKeepsOrder();
    void activeEmptyWhenAllRevoked();
    void primaryReturnsFirstActive();
    void primarySkipsRevoked();
    void primaryIsDefaultWhenAllRevoked();
    void primaryIsDefaultWhenEmpty();
    void revokeMarksLink();
    void revokeKeepsCountAndOrder();
    void revokeEmitsChangedOnce();
    void revokeIsCaseSensitive();
    void revokeUnknownLinkIsNoOp();
    void revokeAlreadyRevokedIsNoOp();
    void clearForgetsLinks();
    void clearOnEmptyModelIsSilent();
    void modelIsUsableAfterClear();
};

void ChatInviteLinkModelTest::startsEmpty()
{
    ChatInviteLinkModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.links().isEmpty());
    QVERIFY(model.active().isEmpty());
    QVERIFY(model.primary().link.isEmpty());
}

void ChatInviteLinkModelTest::setLinksKeepsOrder()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(thirdLink(), QStringLiteral("Third")),
                    makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"))});

    const QVector<InviteLinkRow> links = model.links();
    QCOMPARE(int(links.size()), 3);
    // The server's order is the screen's order: nothing here sorts by link.
    QCOMPARE(links.at(0).link, thirdLink());
    QCOMPARE(links.at(1).link, firstLink());
    QCOMPARE(links.at(2).link, secondLink());
}

void ChatInviteLinkModelTest::setLinksDropsEmptyLinks()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(QString(), QStringLiteral("No link")),
                    makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(QString(), QStringLiteral("Also no link"))});

    // A row without a link can be neither shown, copied nor revoked.
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.links().at(0).link, firstLink());
    QCOMPARE(model.links().at(0).title, QStringLiteral("First"));
}

void ChatInviteLinkModelTest::setLinksKeepsFirstDuplicate()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(firstLink(), QStringLiteral("Second")),
                    makeLink(secondLink(), QStringLiteral("Other"))});

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.links().at(0).title, QStringLiteral("First"));
    QCOMPARE(model.links().at(1).link, secondLink());
}

void ChatInviteLinkModelTest::setLinksClampsNegativeMemberCount()
{
    ChatInviteLinkModel model;
    InviteLinkRow row = makeLink(firstLink(), QStringLiteral("First"));
    row.memberCount = -7;
    model.setLinks({row});

    QCOMPARE(model.links().at(0).memberCount, 0);
}

void ChatInviteLinkModelTest::setLinksClampsNegativeMemberLimit()
{
    ChatInviteLinkModel model;
    InviteLinkRow row = makeLink(firstLink(), QStringLiteral("First"));
    row.memberLimit = -100;
    model.setLinks({row});

    QCOMPARE(model.links().at(0).memberLimit, qint64(0));
}

void ChatInviteLinkModelTest::setLinksKeepsOtherFields()
{
    ChatInviteLinkModel model;
    InviteLinkRow row = makeLink(firstLink(), QStringLiteral("First"));
    row.memberLimit = 25;
    row.memberCount = 4;
    row.expiresAt = 1893456000;
    model.setLinks({row});

    const InviteLinkRow stored = model.links().at(0);
    QCOMPARE(stored.title, QStringLiteral("First"));
    QCOMPARE(stored.memberLimit, qint64(25));
    QCOMPARE(stored.memberCount, 4);
    QCOMPARE(stored.expiresAt, qint64(1893456000));
    QVERIFY(!stored.revoked);
}

void ChatInviteLinkModelTest::setLinksEmitsChangedOnce()
{
    ChatInviteLinkModel model;
    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);

    model.setLinks({makeLink(firstLink()), makeLink(secondLink())});

    QCOMPARE(spy.count(), 1);
}

void ChatInviteLinkModelTest::setLinksSameLinksIsSilent()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"))});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"))});

    // A refresh that brings the same data must not repaint the list.
    QCOMPARE(spy.count(), 0);
}

void ChatInviteLinkModelTest::setLinksClampedDuplicateIsSilent()
{
    ChatInviteLinkModel model;
    InviteLinkRow negative = makeLink(firstLink(), QStringLiteral("First"));
    negative.memberCount = -1;
    model.setLinks({negative});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    model.setLinks({makeLink(firstLink(), QStringLiteral("First"))});

    // -1 and 0 both store as 0, so the visible row did not change.
    QCOMPARE(model.links().at(0).memberCount, 0);
    QCOMPARE(spy.count(), 0);
}

void ChatInviteLinkModelTest::setLinksReorderedIsAChange()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink()), makeLink(secondLink())});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    model.setLinks({makeLink(secondLink()), makeLink(firstLink())});

    // Order decides which link primary() reports, so it is a visible change.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.primary().link, secondLink());
}

void ChatInviteLinkModelTest::setLinksEmptyClearsAndEmits()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink())});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    model.setLinks({});

    QCOMPARE(model.count(), 0);
    QCOMPARE(spy.count(), 1);
}

void ChatInviteLinkModelTest::setLinksReplacesPrevious()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"))});
    model.setLinks({makeLink(thirdLink(), QStringLiteral("Third"))});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.links().at(0).link, thirdLink());
    QCOMPARE(model.links().at(0).title, QStringLiteral("Third"));
}

void ChatInviteLinkModelTest::linksReturnsCopy()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First"))});

    QVector<InviteLinkRow> copy = model.links();
    copy[0].title = QStringLiteral("Changed");

    QCOMPARE(model.links().at(0).title, QStringLiteral("First"));
}

void ChatInviteLinkModelTest::countMatchesLinks()
{
    ChatInviteLinkModel model;
    QCOMPARE(model.count(), 0);

    model.setLinks({makeLink(firstLink()), makeLink(secondLink())});
    QCOMPARE(model.count(), int(model.links().size()));

    QVERIFY(model.revoke(firstLink()));
    QCOMPARE(model.count(), int(model.links().size()));
}

void ChatInviteLinkModelTest::activeExcludesRevoked()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"), true),
                    makeLink(thirdLink(), QStringLiteral("Third"))});

    const QVector<InviteLinkRow> active = model.active();
    QCOMPARE(int(active.size()), 2);
    QCOMPARE(active.at(0).link, firstLink());
    QCOMPARE(active.at(1).link, thirdLink());
}

void ChatInviteLinkModelTest::activeKeepsOrder()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(thirdLink()), makeLink(firstLink()),
                    makeLink(secondLink())});

    const QVector<InviteLinkRow> active = model.active();
    QCOMPARE(int(active.size()), 3);
    QCOMPARE(active.at(0).link, thirdLink());
    QCOMPARE(active.at(1).link, firstLink());
    QCOMPARE(active.at(2).link, secondLink());
}

void ChatInviteLinkModelTest::activeEmptyWhenAllRevoked()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First"), true),
                    makeLink(secondLink(), QStringLiteral("Second"), true)});

    QVERIFY(model.active().isEmpty());
    // Revoked links stay in the full list so the screen can still show them.
    QCOMPARE(model.count(), 2);
}

void ChatInviteLinkModelTest::primaryReturnsFirstActive()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"))});

    QCOMPARE(model.primary().link, firstLink());
    QCOMPARE(model.primary().title, QStringLiteral("First"));
}

void ChatInviteLinkModelTest::primarySkipsRevoked()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First"), true),
                    makeLink(secondLink(), QStringLiteral("Second")),
                    makeLink(thirdLink(), QStringLiteral("Third"))});

    QCOMPARE(model.primary().link, secondLink());
}

void ChatInviteLinkModelTest::primaryIsDefaultWhenAllRevoked()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First"), true)});

    const InviteLinkRow primary = model.primary();
    QVERIFY(primary.link.isEmpty());
    QVERIFY(primary.title.isEmpty());
    QCOMPARE(primary.memberLimit, qint64(0));
    QCOMPARE(primary.memberCount, 0);
    QVERIFY(!primary.revoked);
    QCOMPARE(primary.expiresAt, qint64(0));
}

void ChatInviteLinkModelTest::primaryIsDefaultWhenEmpty()
{
    ChatInviteLinkModel model;

    QVERIFY(model.primary().link.isEmpty());
    QVERIFY(model.primary().title.isEmpty());
}

void ChatInviteLinkModelTest::revokeMarksLink()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"))});

    QVERIFY(model.revoke(secondLink()));

    const QVector<InviteLinkRow> links = model.links();
    QVERIFY(!links.at(0).revoked);
    QVERIFY(links.at(1).revoked);
    QCOMPARE(model.active().at(0).link, firstLink());
    QCOMPARE(int(model.active().size()), 1);
}

void ChatInviteLinkModelTest::revokeKeepsCountAndOrder()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First")),
                    makeLink(secondLink(), QStringLiteral("Second"))});

    QVERIFY(model.revoke(firstLink()));

    // Revoking never removes or reorders a row; it only stops being active.
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.links().at(0).link, firstLink());
    QCOMPARE(model.links().at(1).link, secondLink());
    QCOMPARE(model.primary().link, secondLink());
}

void ChatInviteLinkModelTest::revokeEmitsChangedOnce()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink())});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    QVERIFY(model.revoke(firstLink()));

    QCOMPARE(spy.count(), 1);
}

void ChatInviteLinkModelTest::revokeIsCaseSensitive()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(QStringLiteral("https://t.me/+AbCdEf"))});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    QVERIFY(!model.revoke(QStringLiteral("https://t.me/+abcdef")));

    QVERIFY(!model.links().at(0).revoked);
    QCOMPARE(spy.count(), 0);
}

void ChatInviteLinkModelTest::revokeUnknownLinkIsNoOp()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink())});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    QVERIFY(!model.revoke(secondLink()));

    QCOMPARE(model.count(), 1);
    QVERIFY(!model.links().at(0).revoked);
    QCOMPARE(spy.count(), 0);
}

void ChatInviteLinkModelTest::revokeAlreadyRevokedIsNoOp()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First"), true)});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    QVERIFY(!model.revoke(firstLink()));

    QVERIFY(model.links().at(0).revoked);
    QCOMPARE(spy.count(), 0);
}

void ChatInviteLinkModelTest::clearForgetsLinks()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink()), makeLink(secondLink())});

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    model.clear();

    QCOMPARE(model.count(), 0);
    QVERIFY(model.links().isEmpty());
    QVERIFY(model.active().isEmpty());
    QVERIFY(model.primary().link.isEmpty());
    QCOMPARE(spy.count(), 1);
}

void ChatInviteLinkModelTest::clearOnEmptyModelIsSilent()
{
    ChatInviteLinkModel model;

    QSignalSpy spy(&model, &ChatInviteLinkModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 0);
}

void ChatInviteLinkModelTest::modelIsUsableAfterClear()
{
    ChatInviteLinkModel model;
    model.setLinks({makeLink(firstLink(), QStringLiteral("First"), true)});
    model.clear();

    // A fresh snapshot after closing the chat behaves like a first load.
    model.setLinks({makeLink(secondLink(), QStringLiteral("Second"))});
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.primary().link, secondLink());
    QVERIFY(model.revoke(secondLink()));
    QVERIFY(model.active().isEmpty());
}

QTEST_GUILESS_MAIN(ChatInviteLinkModelTest)
#include "tst_chat_invite_link_model.moc"
