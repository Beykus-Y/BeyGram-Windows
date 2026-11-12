#include "ChatInviteLinkModel.h"

#include <QSet>

#include <utility>

namespace beygram::telegram {
namespace {

// InviteLinkRow carries no operator== (it is a wire-shaped value type), so the
// model compares every field it stores. A reload that only moves expiresAt or
// flips revoked is still a real change for a row that renders those values.
bool sameRow(const InviteLinkRow& left, const InviteLinkRow& right)
{
    return left.link == right.link
        && left.title == right.title
        && left.memberLimit == right.memberLimit
        && left.memberCount == right.memberCount
        && left.revoked == right.revoked
        && left.expiresAt == right.expiresAt;
}

bool sameRows(const QVector<InviteLinkRow>& left, const QVector<InviteLinkRow>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype i = 0; i < left.size(); ++i) {
        if (!sameRow(left.at(i), right.at(i)))
            return false;
    }
    return true;
}

// TDLib uses 0 for "no limit", and a malformed payload can carry any negative
// number. The screen renders both values as counters, so negatives are folded
// to 0 instead of being shown or kept around for a later comparison to trip on.
InviteLinkRow normalized(InviteLinkRow row)
{
    row.memberLimit = qMax<qint64>(0, row.memberLimit);
    row.memberCount = qMax(0, row.memberCount);
    return row;
}

} // namespace

ChatInviteLinkModel::ChatInviteLinkModel(QObject* parent)
    : QObject(parent)
{
}

void ChatInviteLinkModel::setLinks(QVector<InviteLinkRow> links)
{
    // Keep the first row of each link: rows arrive in the order the caller
    // merged its pages, and the first answer is the one already on screen, so
    // it wins over a later duplicate instead of making the visible row depend
    // on merge order. The link is compared exactly, the same way revoke()
    // compares it, so a link that is only reachable through active() is also
    // the one revoke() will find.
    QVector<InviteLinkRow> unique;
    unique.reserve(links.size());
    QSet<QString> seen;
    for (InviteLinkRow& row : links) {
        if (row.link.isEmpty())
            continue;
        if (seen.contains(row.link))
            continue;
        seen.insert(row.link);
        unique.append(normalized(std::move(row)));
    }

    // A reload that brings the same data is not a change: repainting the list
    // for it would also restart every animation bound to changed().
    if (sameRows(unique, m_links))
        return;

    m_links = std::move(unique);
    emit changed();
}

QVector<InviteLinkRow> ChatInviteLinkModel::links() const
{
    return m_links;
}

int ChatInviteLinkModel::count() const
{
    return static_cast<int>(m_links.size());
}

QVector<InviteLinkRow> ChatInviteLinkModel::active() const
{
    QVector<InviteLinkRow> activeLinks;
    activeLinks.reserve(m_links.size());
    for (const InviteLinkRow& row : m_links) {
        if (!row.revoked)
            activeLinks.append(row);
    }
    return activeLinks;
}

InviteLinkRow ChatInviteLinkModel::primary() const
{
    // The list keeps the server's order, so the first active row is the chat's
    // main link. Revoked rows may sit ahead of it and are skipped here rather
    // than being moved, so links() keeps showing them where they were.
    for (const InviteLinkRow& row : m_links) {
        if (!row.revoked)
            return row;
    }
    return InviteLinkRow{};
}

bool ChatInviteLinkModel::revoke(const QString& link)
{
    // QString::operator== is case-sensitive, which is what an invite link
    // needs: "https://t.me/+AbC" and "https://t.me/+abc" are different links.
    for (InviteLinkRow& row : m_links) {
        if (row.link != link)
            continue;
        // Revoking twice changes nothing on screen and would send a second
        // revokeChatInviteLink for a link TDLib already reports as revoked.
        if (row.revoked)
            return false;

        row.revoked = true;
        emit changed();
        return true;
    }
    return false;
}

void ChatInviteLinkModel::clear()
{
    if (m_links.isEmpty())
        return;

    m_links.clear();
    emit changed();
}

} // namespace beygram::telegram
