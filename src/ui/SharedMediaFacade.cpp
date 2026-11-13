#include "SharedMediaFacade.h"

#include <QVector>

namespace beygram::ui {
namespace {

/// The tab strip is fixed and matches the enum order, so the QML index is also
/// the telegram::SharedMediaTab value.
const QStringList& tabLabels()
{
    static const QStringList labels{
        QStringLiteral("Photos"),
        QStringLiteral("Videos"),
        QStringLiteral("Files"),
        QStringLiteral("Links"),
        QStringLiteral("Voice"),
        QStringLiteral("Music"),
        QStringLiteral("GIFs"),
    };
    return labels;
}

static_assert(static_cast<int>(telegram::SharedMediaTab::Photos) == 0,
              "tabs() order must match SharedMediaTab");
static_assert(static_cast<int>(telegram::SharedMediaTab::Gifs) == 6,
              "tabs() order must match SharedMediaTab");

/// QML passes message ids as strings. A value that is not a positive number
/// cannot address a message, so it is read as 0 and dropped by the caller.
qint64 parseMessageId(const QString& raw)
{
    bool ok = false;
    const qint64 id = raw.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

/// Dates are Unix seconds. An unreadable value is treated as 0, which sorts the
/// row last instead of dropping it: the attachment is still valid.
qint64 parseDate(const QString& raw)
{
    bool ok = false;
    const qint64 date = raw.trimmed().toLongLong(&ok);
    return ok && date > 0 ? date : 0;
}

QVector<telegram::SharedMediaItem> toItems(const QStringList& ids,
                                           const QStringList& titles,
                                           const QStringList& dates)
{
    QVector<telegram::SharedMediaItem> items;
    items.reserve(ids.size());

    // The caller has already checked that the three lists are parallel.
    for (qsizetype index = 0; index < ids.size(); ++index) {
        const qint64 messageId = parseMessageId(ids.at(index));
        if (messageId <= 0)
            continue;

        telegram::SharedMediaItem item;
        item.messageId = messageId;
        item.title = titles.at(index);
        item.date = parseDate(dates.at(index));
        items.append(item);
    }

    return items;
}

QStringList idStrings(const QVector<telegram::SharedMediaItem>& items)
{
    QStringList out;
    out.reserve(items.size());
    for (const telegram::SharedMediaItem& item : items)
        out.append(QString::number(item.messageId));
    return out;
}

QStringList titleStrings(const QVector<telegram::SharedMediaItem>& items)
{
    QStringList out;
    out.reserve(items.size());
    for (const telegram::SharedMediaItem& item : items)
        out.append(item.title);
    return out;
}

} // namespace

SharedMediaFacade::SharedMediaFacade(QObject* parent) : QObject(parent)
{
}

QStringList SharedMediaFacade::tabs() const
{
    return tabLabels();
}

int SharedMediaFacade::currentTab() const
{
    return static_cast<int>(m_model.tab());
}

QStringList SharedMediaFacade::titles() const
{
    return titleStrings(m_model.items());
}

QStringList SharedMediaFacade::ids() const
{
    return idStrings(m_model.items());
}

void SharedMediaFacade::selectTab(int index)
{
    if (index < 0 || index >= tabLabels().size())
        return;

    const State before = state();
    // The model drops the previous tab's items itself; setting the current tab
    // again is silent there as well.
    m_model.setTab(static_cast<telegram::SharedMediaTab>(index));
    emitIfChanged(before);
}

void SharedMediaFacade::setItems(const QStringList& ids, const QStringList& titles,
                                 const QStringList& dates)
{
    // The lists are parallel, so a different length describes inconsistent
    // data. Rejecting it keeps the loaded page instead of half of it.
    if (ids.size() != titles.size() || ids.size() != dates.size())
        return;

    const State before = state();
    m_model.setItems(toItems(ids, titles, dates));
    emitIfChanged(before);
}

void SharedMediaFacade::appendItems(const QStringList& ids, const QStringList& titles,
                                    const QStringList& dates)
{
    if (ids.size() != titles.size() || ids.size() != dates.size())
        return;

    const State before = state();
    m_model.appendItems(toItems(ids, titles, dates));
    emitIfChanged(before);
}

void SharedMediaFacade::activate(int index)
{
    const QStringList loaded = ids();
    if (index < 0 || index >= loaded.size())
        return;

    emit itemActivated(loaded.at(index));
}

SharedMediaFacade::State SharedMediaFacade::state() const
{
    State snapshot;
    snapshot.tab = currentTab();

    const QVector<telegram::SharedMediaItem> items = m_model.items();
    snapshot.ids = idStrings(items);
    snapshot.titles = titleStrings(items);
    return snapshot;
}

void SharedMediaFacade::emitIfChanged(const State& before)
{
    if (state() == before)
        return;
    emit changed();
}

} // namespace beygram::ui
