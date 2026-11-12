#include "StickerPanelFacade.h"

#include <QVector>

namespace beygram::ui {
namespace {

QStringList toIdStrings(const QVector<qint64>& ids)
{
    QStringList out;
    out.reserve(ids.size());
    for (qint64 id : ids)
        out.append(QString::number(id));
    return out;
}

QVector<qint64> toIds(const QStringList& ids)
{
    QVector<qint64> out;
    out.reserve(ids.size());
    for (const QString& raw : ids) {
        bool ok = false;
        const qint64 id = raw.trimmed().toLongLong(&ok);
        if (ok && id > 0)
            out.append(id);
    }
    return out;
}

// One place that turns the QML-facing string into a sticker id, so recordSent()
// and setSetStickers() cannot disagree about what a valid id is.
bool parseId(const QString& raw, qint64* id)
{
    bool ok = false;
    const qint64 parsed = raw.trimmed().toLongLong(&ok);
    if (!ok || parsed <= 0)
        return false;
    *id = parsed;
    return true;
}

} // namespace

StickerPanelFacade::StickerPanelFacade(QObject* parent)
    : QObject(parent)
{
}

bool StickerPanelFacade::open() const
{
    return m_open;
}

void StickerPanelFacade::setOpen(bool open)
{
    if (open == m_open)
        return;
    const State before = state();
    m_open = open;
    emitIfChanged(before);
}

QString StickerPanelFacade::query() const
{
    return m_search.query();
}

void StickerPanelFacade::setQuery(const QString& query)
{
    // The search controller trims, treats an empty query as "no search" and
    // drops the previous results plus the loading flag on a real change; the
    // facade only mirrors that and reports it through changed().
    const State before = state();
    m_search.setQuery(query);
    emitIfChanged(before);
}

QStringList StickerPanelFacade::recentStickers() const
{
    return toIdStrings(m_recents.stickers());
}

QStringList StickerPanelFacade::setTitles() const
{
    return m_setTitles;
}

int StickerPanelFacade::currentSetIndex() const
{
    return m_currentSetIndex;
}

void StickerPanelFacade::setCurrentSetIndex(int index)
{
    if (index < 0 || index >= m_setTitles.size())
        return;
    if (index == m_currentSetIndex)
        return;

    const State before = state();
    m_currentSetIndex = index;
    // Set ids are positional, so the tab index maps straight onto the id. The
    // picker drops the previous set's stickers, which is why state().stickers
    // really changes here.
    m_picker.selectSet(index + 1);
    emitIfChanged(before);
}

bool StickerPanelFacade::loading() const
{
    return m_search.loading();
}

QStringList StickerPanelFacade::stickers() const
{
    return toIdStrings(m_picker.stickers());
}

void StickerPanelFacade::recordSent(const QString& stickerId)
{
    qint64 id = 0;
    if (!parseId(stickerId, &id))
        return;

    const State before = state();
    // add() moves an existing entry to the front instead of duplicating it, so
    // re-sending the newest sticker leaves the order untouched.
    m_recents.add(id);
    emitIfChanged(before);
    // Even a re-sent sticker asks the owner to send it again.
    emit stickerActivated(QString::number(id));
}

void StickerPanelFacade::setSets(const QStringList& titles)
{
    const State before = state();

    QVector<telegram::StickerSetRow> rows;
    QStringList kept;
    rows.reserve(titles.size());
    kept.reserve(titles.size());
    for (const QString& raw : titles) {
        const QString title = raw.trimmed();
        if (title.isEmpty())
            continue;

        telegram::StickerSetRow row;
        row.id = kept.size() + 1;
        row.title = title;
        rows.append(row);
        kept.append(title);
    }

    m_setTitles = kept;
    m_picker.setSets(rows);

    if (m_setTitles.isEmpty()) {
        m_currentSetIndex = -1;
        // Back to the recent tab, which also drops the last set's stickers so
        // stickers() cannot outlive the strip that showed them.
        m_picker.selectSet(0);
    } else {
        // The controller keeps a selection that vanished from the strip, so the
        // facade clamps it itself instead of leaving QML on a dead tab.
        m_currentSetIndex = qBound(0, m_currentSetIndex, int(m_setTitles.size()) - 1);
        m_picker.selectSet(m_currentSetIndex + 1);
    }

    emitIfChanged(before);
}

void StickerPanelFacade::setSetStickers(const QStringList& stickerIds)
{
    const State before = state();
    m_picker.setStickers(toIds(stickerIds));
    emitIfChanged(before);
}

void StickerPanelFacade::clear()
{
    const State before = state();
    m_open = false;
    m_currentSetIndex = -1;
    m_setTitles.clear();
    m_picker.clear();
    m_search.clear();
    m_recents.clear();
    emitIfChanged(before);
}

StickerPanelFacade::State StickerPanelFacade::state() const
{
    State s;
    s.open = m_open;
    s.query = query();
    s.recents = recentStickers();
    s.titles = m_setTitles;
    s.index = m_currentSetIndex;
    s.loading = loading();
    s.stickers = stickers();
    return s;
}

void StickerPanelFacade::emitIfChanged(const State& before)
{
    if (state() == before)
        return;
    emit changed();
}

} // namespace beygram::ui
