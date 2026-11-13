#include "BgMusicFacade.h"

#include "telegram/music/BgMusicImport.h"

#include <QFileInfo>
#include <QSet>

#include <utility>

namespace beygram::ui {
namespace {

// planBgMusicImport() validates the text and reports the playlist identity, but
// it deliberately returns no entries. The facade therefore parses the accepted
// text with the same extension rule, so the decision and the payload can never
// disagree about which parser runs.
telegram::BgMusicPlaylist parsePlaylist(const QString& fileName, const QString& content)
{
    const QString suffix = QFileInfo(fileName.trimmed()).suffix().toLower();
    if (suffix == QStringLiteral("json"))
        return telegram::bgMusicFromJson(content);
    if (suffix == QStringLiteral("m3u") || suffix == QStringLiteral("m3u8"))
        return telegram::bgMusicFromM3u(content);
    return {};
}

// An M3U carries no title, and a hand-written JSON may leave it empty. The file
// name is the only human-readable label the import has left, so it keeps the row
// from rendering blank. The generated id is the last resort for a name that has
// no base either, e.g. ".m3u".
QString titleFromFileName(const QString& fileName, const QString& fallback)
{
    const QString base = QFileInfo(fileName.trimmed()).completeBaseName().trimmed();
    return base.isEmpty() ? fallback : base;
}

} // namespace

BgMusicFacade::BgMusicFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList BgMusicFacade::playlistTitles() const
{
    QStringList titles;
    titles.reserve(m_playlists.size());
    for (const telegram::BgMusicPlaylist& playlist : m_playlists)
        titles.append(playlist.title);
    return titles;
}

QStringList BgMusicFacade::playlistIds() const
{
    QStringList ids;
    ids.reserve(m_playlists.size());
    for (const telegram::BgMusicPlaylist& playlist : m_playlists)
        ids.append(playlist.id);
    return ids;
}

int BgMusicFacade::entryCount() const
{
    const qsizetype index = selectedIndex();
    if (index < 0)
        return 0;
    return static_cast<int>(m_playlists.at(index).entries.size());
}

QString BgMusicFacade::selectedId() const
{
    return m_selectedId;
}

void BgMusicFacade::setPlaylists(const QStringList& ids, const QStringList& titles)
{
    const State before = state();

    // The lists are parallel, so a title with no id at its offset belongs to no
    // playlist. The shorter list defines the rows, exactly like the other
    // facades that accept a TDLib-shaped strip.
    const qsizetype count = qMin(ids.size(), titles.size());
    QVector<telegram::BgMusicPlaylist> next;
    next.reserve(count);

    QSet<QString> seen;
    for (qsizetype i = 0; i < count; ++i) {
        const QString& id = ids.at(i);
        // Nothing can select an id-less playlist, so it would be an invisible
        // row. A repeated id keeps its first row, because the caller's order is
        // the screen's order and a later duplicate must not move it.
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);

        telegram::BgMusicPlaylist playlist;
        playlist.id = id;
        playlist.title = titles.at(i);

        // A metadata refresh is not a content replacement: keep the entries the
        // facade already imported for this id. A playlist that is gone from the
        // new strip is dropped with its entries below.
        const qsizetype existing = indexOf(id);
        if (existing >= 0) {
            playlist.entries = m_playlists.at(existing).entries;
            playlist.updatedAt = m_playlists.at(existing).updatedAt;
        }

        next.append(std::move(playlist));
    }

    m_playlists = std::move(next);

    // The selection must name a visible row; otherwise the panel would show an
    // empty entry count for a playlist it cannot highlight.
    if (!m_selectedId.isEmpty() && indexOf(m_selectedId) < 0)
        m_selectedId.clear();

    emitIfChanged(before);
}

void BgMusicFacade::select(const QString& id)
{
    // An empty id is "no selection", not a playlist called "": clearing is the
    // one case a caller can use without knowing whether a row exists.
    if (id.isEmpty()) {
        if (m_selectedId.isEmpty())
            return;
        m_selectedId.clear();
        emit changed();
        return;
    }

    // A stale tap must not select an invisible playlist; the current selection
    // stays as it was.
    if (indexOf(id) < 0 || m_selectedId == id)
        return;

    m_selectedId = id;
    emit changed();
}

bool BgMusicFacade::importFromText(const QString& fileName, const QString& content)
{
    const telegram::ImportDecision decision =
        telegram::planBgMusicImport(fileName, content.toUtf8().size(), content);
    if (!decision.accepted) {
        // A rejected paste is not a state change: the panel keeps showing what
        // it showed and only the dialog's error line moves.
        emit importFailed(decision.reason);
        return false;
    }

    telegram::BgMusicPlaylist imported = parsePlaylist(fileName, content);
    if (imported.entries.isEmpty()) {
        // Defensive: the decision above already proved a parser found entries.
        // If the two ever disagree, fail closed instead of adding an empty row.
        emit importFailed(QStringLiteral("no-entries"));
        return false;
    }

    const State before = state();

    // M3U has no playlist identity, and a hand-written JSON may omit the id. A
    // deterministic local id keeps select() and the exported document usable.
    if (imported.id.trimmed().isEmpty())
        imported.id = nextGeneratedId();
    if (imported.title.trimmed().isEmpty())
        imported.title = titleFromFileName(fileName, imported.id);

    // The id is the playlist's identity: importing the same id again is an
    // update, not a second row. A new id is appended in import order.
    const qsizetype existing = indexOf(imported.id);
    if (existing >= 0)
        m_playlists[existing] = imported;
    else
        m_playlists.append(imported);

    // The user just pasted this playlist, so show it. If it replaced the
    // selected one, the selection already names it and does not move.
    m_selectedId = imported.id;

    emitIfChanged(before);
    return true;
}

QString BgMusicFacade::exportJson() const
{
    const qsizetype index = selectedIndex();
    if (index < 0)
        return QString();
    return telegram::bgMusicToJson(m_playlists.at(index));
}

QString BgMusicFacade::exportM3u() const
{
    const qsizetype index = selectedIndex();
    if (index < 0)
        return QString();
    return telegram::bgMusicToM3u(m_playlists.at(index));
}

void BgMusicFacade::clear()
{
    if (m_playlists.isEmpty() && m_selectedId.isEmpty())
        return;

    m_playlists.clear();
    m_selectedId.clear();
    emit changed();
}

BgMusicFacade::State BgMusicFacade::state() const
{
    State s;
    s.ids = playlistIds();
    s.titles = playlistTitles();
    s.entryCount = entryCount();
    s.selectedId = m_selectedId;
    return s;
}

void BgMusicFacade::emitIfChanged(const State& before)
{
    if (state() == before)
        return;
    emit changed();
}

qsizetype BgMusicFacade::indexOf(const QString& id) const
{
    for (qsizetype i = 0; i < m_playlists.size(); ++i) {
        if (m_playlists.at(i).id == id)
            return i;
    }
    return -1;
}

qsizetype BgMusicFacade::selectedIndex() const
{
    if (m_selectedId.isEmpty())
        return -1;
    return indexOf(m_selectedId);
}

QString BgMusicFacade::nextGeneratedId() const
{
    for (int suffix = 1; ; ++suffix) {
        const QString candidate = QStringLiteral("import-%1").arg(suffix);
        if (indexOf(candidate) < 0)
            return candidate;
    }
}

} // namespace beygram::ui
