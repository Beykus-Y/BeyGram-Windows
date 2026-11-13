#include "ForumTopicsFacade.h"

#include <QtGlobal>

#include <utility>

namespace beygram::ui {
namespace {

/// Canonical decimal id, or {} when `raw` is not a positive 64-bit integer.
QString normalisedId(const QString& raw)
{
    bool ok = false;
    const qint64 id = raw.trimmed().toLongLong(&ok);
    if (!ok || id <= 0)
        return {};
    return QString::number(id);
}

/// setTopics() receives counts as strings. Anything unparsable or non-positive
/// means "no unread", never a negative badge.
int parseUnread(const QString& raw)
{
    bool ok = false;
    const int count = raw.trimmed().toInt(&ok);
    return ok && count > 0 ? count : 0;
}

/// Accepts the two spellings QVariant produces for a bool plus the schema's
/// numeric form. Everything else is false.
bool parsePinned(const QString& raw)
{
    const QString value = raw.trimmed().toLower();
    return value == QLatin1String("true") || value == QLatin1String("1");
}

} // namespace

ForumTopicsFacade::ForumTopicsFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList ForumTopicsFacade::titles() const
{
    QStringList out;
    out.reserve(m_rows.size());
    for (const Row& row : m_rows)
        out.append(row.title);
    return out;
}

QStringList ForumTopicsFacade::ids() const
{
    QStringList out;
    out.reserve(m_rows.size());
    for (const Row& row : m_rows)
        out.append(row.id);
    return out;
}

int ForumTopicsFacade::count() const
{
    return static_cast<int>(m_rows.size());
}

QVariantList ForumTopicsFacade::unreadCounts() const
{
    QVariantList out;
    out.reserve(m_rows.size());
    for (const Row& row : m_rows)
        out.append(row.unread);
    return out;
}

QVariantList ForumTopicsFacade::pinnedFlags() const
{
    QVariantList out;
    out.reserve(m_rows.size());
    for (const Row& row : m_rows)
        out.append(row.pinned);
    return out;
}

void ForumTopicsFacade::setTopics(const QStringList& ids, const QStringList& titles,
                                  const QStringList& unread, const QStringList& pinned)
{
    // ids and titles are the essential pair: a row needs both, so the shorter
    // list wins. The auxiliary lists may be short; a missing count is 0 and a
    // missing pin flag is false, which keeps a real topic visible instead of
    // dropping it over an omitted badge value.
    const qsizetype rowCount = qMin(ids.size(), titles.size());

    QVector<Row> next;
    next.reserve(rowCount);
    for (qsizetype i = 0; i < rowCount; ++i) {
        Row row;
        row.id = normalisedId(ids.at(i));
        row.title = titles.at(i);

        // A row without an address or a visible title cannot be opened or
        // pinned, so it is dropped the way forumTopicsFromResponse() does.
        if (row.id.isEmpty() || row.title.trimmed().isEmpty())
            continue;

        row.unread = i < unread.size() ? parseUnread(unread.at(i)) : 0;
        row.pinned = i < pinned.size() ? parsePinned(pinned.at(i)) : false;
        next.append(row);
    }

    if (next == m_rows)
        return;

    m_rows = std::move(next);
    emit changed();
}

void ForumTopicsFacade::openTopic(const QString& topicId)
{
    const int index = indexOf(topicId);
    if (index < 0)
        return;

    emit topicActivated(m_rows.at(index).id);
}

void ForumTopicsFacade::togglePinned(const QString& topicId)
{
    const int index = indexOf(topicId);
    if (index < 0)
        return;

    // Copy before emitting: a changed() handler may reload the list and
    // invalidate the row reference.
    const QString id = m_rows.at(index).id;
    const bool pinned = !m_rows.at(index).pinned;
    m_rows[index].pinned = pinned;

    emit changed();
    emit pinRequested(id, pinned);
}

void ForumTopicsFacade::clear()
{
    if (m_rows.isEmpty())
        return;

    m_rows.clear();
    emit changed();
}

int ForumTopicsFacade::indexOf(const QString& topicId) const
{
    const QString id = normalisedId(topicId);
    if (id.isEmpty())
        return -1;

    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace beygram::ui
