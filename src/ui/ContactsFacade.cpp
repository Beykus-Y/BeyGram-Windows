#include "ContactsFacade.h"

#include <utility>

namespace beygram::ui {

namespace {

using telegram::ContactSort;

/// Parses a QML-provided id. Anything that is not a positive integer yields 0,
/// which is how every caller here spells "no such contact".
qint64 parseId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

/// QML has no bool list, so the online column arrives as strings. Only an
/// explicit yes counts as online; an empty or unrecognised value is offline,
/// which is the safe default for a presence dot.
bool parseOnline(const QString& value)
{
    const QString token = value.trimmed().toLower();
    return token == QLatin1String("true") || token == QLatin1String("1")
        || token == QLatin1String("yes") || token == QLatin1String("online");
}

/// Unix seconds, or 0 for "unknown". Negative timestamps are not a real
/// observation and would sort ahead of every real one, so they are dropped.
qint64 parseLastSeen(const QString& value)
{
    bool ok = false;
    const qint64 seconds = value.trimmed().toLongLong(&ok);
    return ok && seconds > 0 ? seconds : 0;
}

QString sortToken(ContactSort sort)
{
    switch (sort) {
    case ContactSort::LastSeen:
        return QStringLiteral("lastSeen");
    case ContactSort::OnlineFirst:
        return QStringLiteral("onlineFirst");
    case ContactSort::Name:
        break;
    }
    return QStringLiteral("name");
}

/// Maps a mode token back onto the enum. Case, underscores, dashes and spaces
/// are ignored, so "last_seen" and "Online-First" are understood, while a
/// genuinely unknown token is rejected instead of silently becoming Name.
bool parseSortToken(const QString& mode, ContactSort* sort)
{
    QString token = mode.trimmed().toLower();
    token.remove(QLatin1Char('_'));
    token.remove(QLatin1Char('-'));
    token.remove(QLatin1Char(' '));

    if (token == QLatin1String("name")) {
        *sort = ContactSort::Name;
        return true;
    }
    if (token == QLatin1String("lastseen")) {
        *sort = ContactSort::LastSeen;
        return true;
    }
    if (token == QLatin1String("onlinefirst")) {
        *sort = ContactSort::OnlineFirst;
        return true;
    }
    return false;
}

} // namespace

ContactsFacade::ContactsFacade(QObject* parent)
    : QObject(parent)
    , m_model(new telegram::ContactSortModel(this))
{
    // The model is the single source of truth for the order and the online
    // filter, so a real change there invalidates the cached rows and re-notifies
    // the view. The model already suppresses no-op setters, which is what keeps
    // changed() at one emission per transition.
    connect(m_model, &telegram::ContactSortModel::changed, this, [this] {
        invalidate();
        emit changed();
    });
}

bool ContactsFacade::matchesQuery(const QString& name) const
{
    return m_query.isEmpty() || name.contains(m_query, Qt::CaseInsensitive);
}

const QVector<telegram::ContactRow>& ContactsFacade::visibleRows() const
{
    if (m_visibleValid)
        return m_visible;

    QVector<telegram::ContactRow> filtered;
    filtered.reserve(m_contacts.size());
    for (const telegram::ContactRow& row : m_contacts) {
        if (matchesQuery(row.name))
            filtered.append(row);
    }

    // sorted() filters again for onlineOnly() and sorts a copy, so the snapshot
    // in m_contacts stays exactly as the caller handed it over.
    m_visible = m_model->sorted(std::move(filtered));
    m_visibleValid = true;
    return m_visible;
}

void ContactsFacade::invalidate()
{
    m_visibleValid = false;
}

QStringList ContactsFacade::names() const
{
    const QVector<telegram::ContactRow>& rows = visibleRows();

    QStringList result;
    result.reserve(rows.size());
    for (const telegram::ContactRow& row : rows)
        result.append(row.name);
    return result;
}

QStringList ContactsFacade::indexLetters() const
{
    // The rows are already filtered by the query; the model applies onlineOnly()
    // itself, which is idempotent here and keeps the strip in step with
    // names().
    return m_model->indexLetters(visibleRows());
}

void ContactsFacade::setContacts(const QStringList& ids, const QStringList& names,
                                 const QStringList& online, const QStringList& lastSeen)
{
    QVector<telegram::ContactRow> rows;
    rows.reserve(ids.size());

    for (qsizetype i = 0; i < ids.size(); ++i) {
        const qint64 id = parseId(ids.at(i));
        if (id <= 0)
            continue;

        telegram::ContactRow row;
        row.userId = id;
        if (i < names.size())
            row.name = names.at(i);
        if (i < online.size())
            row.online = parseOnline(online.at(i));
        if (i < lastSeen.size())
            row.lastSeen = parseLastSeen(lastSeen.at(i));
        rows.append(row);
    }

    m_contacts = std::move(rows);
    invalidate();
    emit changed();
}

QString ContactsFacade::sortMode() const
{
    return sortToken(m_model->sort());
}

void ContactsFacade::setSortMode(const QString& mode)
{
    telegram::ContactSort sort = telegram::ContactSort::Name;
    if (!parseSortToken(mode, &sort))
        return;

    // Emits changed() through the model connection, and only when the order
    // really changes.
    m_model->setSort(sort);
}

bool ContactsFacade::onlineOnly() const
{
    return m_model->onlineOnly();
}

void ContactsFacade::setOnlineOnly(bool on)
{
    m_model->setOnlineOnly(on);
}

QString ContactsFacade::query() const
{
    return m_query;
}

void ContactsFacade::setQuery(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (trimmed == m_query)
        return;

    m_query = trimmed;
    invalidate();

    // The request names the query this call installed, even if a changed()
    // handler re-enters setQuery() before the request goes out.
    const QString request = trimmed;

    emit changed();
    if (!request.isEmpty())
        emit searchRequested(request);
}

bool ContactsFacade::loading() const
{
    return m_loading;
}

void ContactsFacade::setLoading(bool loading)
{
    if (loading == m_loading)
        return;

    m_loading = loading;
    emit changed();
}

QString ContactsFacade::userIdAt(int index) const
{
    const QVector<telegram::ContactRow>& rows = visibleRows();
    if (index < 0 || index >= rows.size())
        return QString();

    return QString::number(rows.at(index).userId);
}

void ContactsFacade::activate(int index)
{
    // Snapshot the id before emitting: the handler may reload the list and
    // re-sort it under us.
    const QString userId = userIdAt(index);
    if (userId.isEmpty())
        return;

    emit contactActivated(userId);
}

} // namespace beygram::ui
