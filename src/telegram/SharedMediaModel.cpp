#include "SharedMediaModel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

QString filterTypeForTab(SharedMediaTab tab)
{
    switch (tab) {
    case SharedMediaTab::Photos:
        return QStringLiteral("searchMessagesFilterPhoto");
    case SharedMediaTab::Videos:
        return QStringLiteral("searchMessagesFilterVideo");
    case SharedMediaTab::Files:
        return QStringLiteral("searchMessagesFilterDocument");
    case SharedMediaTab::Links:
        return QStringLiteral("searchMessagesFilterUrl");
    case SharedMediaTab::Voice:
        return QStringLiteral("searchMessagesFilterVoiceNote");
    case SharedMediaTab::Music:
        return QStringLiteral("searchMessagesFilterAudio");
    case SharedMediaTab::Gifs:
        return QStringLiteral("searchMessagesFilterAnimation");
    }

    // Only an out-of-range value can get here; the first tab is the least
    // surprising filter for a stale caller.
    return QStringLiteral("searchMessagesFilterPhoto");
}

bool sameItem(const SharedMediaItem& left, const SharedMediaItem& right)
{
    return left.messageId == right.messageId && left.date == right.date
        && left.title == right.title && left.subtitle == right.subtitle
        && left.fileId == right.fileId && left.sizeBytes == right.sizeBytes
        && left.durationSeconds == right.durationSeconds;
}

bool sameItems(const QVector<SharedMediaItem>& left, const QVector<SharedMediaItem>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameItem(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

/// Newest first; equal timestamps fall back to the larger message id, which is
/// the newer message in TDLib.
bool newerFirst(const SharedMediaItem& left, const SharedMediaItem& right)
{
    if (left.date != right.date)
        return left.date > right.date;
    return left.messageId > right.messageId;
}

/// Keeps the first copy of every message id, then normalizes the order.
QVector<SharedMediaItem> normalized(QVector<SharedMediaItem> items)
{
    QVector<SharedMediaItem> kept;
    kept.reserve(items.size());

    QSet<qint64> seenMessageIds;
    seenMessageIds.reserve(items.size());

    for (const SharedMediaItem& item : items) {
        if (seenMessageIds.contains(item.messageId))
            continue;
        seenMessageIds.insert(item.messageId);
        kept.append(item);
    }

    std::sort(kept.begin(), kept.end(), newerFirst);
    return kept;
}

} // namespace

QString sharedMediaFilterJson(SharedMediaTab tab)
{
    const QJsonObject filter{
        {QStringLiteral("@type"), filterTypeForTab(tab)},
    };
    return QString::fromUtf8(QJsonDocument(filter).toJson(QJsonDocument::Compact));
}

SharedMediaModel::SharedMediaModel(QObject* parent) : QObject(parent)
{
}

void SharedMediaModel::setTab(SharedMediaTab tab)
{
    if (m_tab == tab)
        return;

    m_tab = tab;
    m_items.clear();
    m_hasMore = false;
    emit changed();
}

SharedMediaTab SharedMediaModel::tab() const
{
    return m_tab;
}

void SharedMediaModel::setItems(QVector<SharedMediaItem> items)
{
    QVector<SharedMediaItem> next = normalized(std::move(items));
    if (sameItems(next, m_items))
        return;

    m_items = std::move(next);
    m_hasMore = !m_items.isEmpty();
    emit changed();
}

void SharedMediaModel::appendItems(QVector<SharedMediaItem> items)
{
    QSet<qint64> knownIds;
    knownIds.reserve(m_items.size());
    for (const SharedMediaItem& item : m_items)
        knownIds.insert(item.messageId);

    QVector<SharedMediaItem> fresh;
    fresh.reserve(items.size());
    for (const SharedMediaItem& item : items) {
        // A page can overlap itself as well as the loaded list, so the first
        // copy of an id wins here too.
        if (knownIds.contains(item.messageId))
            continue;
        knownIds.insert(item.messageId);
        fresh.append(item);
    }

    if (fresh.isEmpty())
        return;

    m_items += fresh;
    std::sort(m_items.begin(), m_items.end(), newerFirst);
    m_hasMore = true;
    emit changed();
}

QVector<SharedMediaItem> SharedMediaModel::items() const
{
    return m_items;
}

int SharedMediaModel::count() const
{
    return static_cast<int>(m_items.size());
}

qint64 SharedMediaModel::nextFromMessageId() const
{
    if (m_items.isEmpty())
        return 0;
    return m_items.constLast().messageId;
}

void SharedMediaModel::clear()
{
    if (m_items.isEmpty())
        return;

    m_items.clear();
    m_hasMore = false;
    emit changed();
}

} // namespace beygram::telegram
