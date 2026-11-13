#include "MusicSearchFacade.h"

#include "telegram/music/MusicSearch.h"

#include <QSet>
#include <QtGlobal>

#include <algorithm>
#include <limits>
#include <utility>

namespace beygram::ui {
namespace {

/// The page keeps the newest 20 queries; older ones fall off the end.
constexpr int kMaxHistoryEntries = 20;

/// A search page shows at most this many rows; the engine clamps 1..200.
constexpr int kSearchLimit = 50;

/// Whole seconds as a string, clamped to 0..INT_MAX. Anything that is not a
/// number, a negative value and an empty string all mean "unknown".
int durationSeconds(const QString& value)
{
    bool ok = false;
    const qlonglong parsed = value.trimmed().toLongLong(&ok);
    if (!ok || parsed <= 0)
        return 0;
    return static_cast<int>(std::min<qlonglong>(parsed, std::numeric_limits<int>::max()));
}

/// "title — artist · m:ss", dropping the halves that are unknown.
QString resultLabel(const telegram::MusicTrackModel& track)
{
    QString label = track.title.isEmpty() ? track.id : track.title;
    if (!track.artistId.isEmpty())
        label += QStringLiteral(" \u2014 ") + track.artistId;
    if (track.durationSeconds > 0)
        label += QStringLiteral(" \u00B7 ") + telegram::musicDurationText(track.durationSeconds);
    return label;
}

/// "track" -> "Track" for the provider-link labels.
QString kindLabel(const QString& kind)
{
    if (kind.isEmpty())
        return kind;
    return kind.left(1).toUpper() + kind.mid(1);
}

} // namespace

MusicSearchFacade::MusicSearchFacade(QObject* parent)
    : QObject(parent)
{
}

QString MusicSearchFacade::query() const
{
    return m_query;
}

QStringList MusicSearchFacade::results() const
{
    return m_results;
}

QStringList MusicSearchFacade::history() const
{
    return m_history.entries();
}

bool MusicSearchFacade::loading() const
{
    return m_loading;
}

void MusicSearchFacade::setLibrary(const QStringList& ids, const QStringList& titles,
                                   const QStringList& artistIds, const QStringList& albumIds,
                                   const QStringList& durations)
{
    const Snapshot before = snapshot();

    const qsizetype count = std::max({ids.size(), titles.size(), artistIds.size(),
                                      albumIds.size(), durations.size()});

    QVector<telegram::MusicTrackModel> tracks;
    tracks.reserve(count);
    QSet<QString> seenIds;

    for (qsizetype i = 0; i < count; ++i) {
        const QString id = ids.value(i).trimmed();
        // A track without an id cannot be played or addressed, and a repeated
        // id would make resultIdAt() ambiguous; keep the first one.
        if (id.isEmpty() || seenIds.contains(id))
            continue;
        seenIds.insert(id);

        telegram::MusicTrackModel track;
        track.id = id;
        track.title = titles.value(i).trimmed();
        track.artistId = artistIds.value(i).trimmed();
        track.albumId = albumIds.value(i).trimmed();
        track.durationSeconds = durationSeconds(durations.value(i));
        tracks.append(track);
    }

    m_tracks = std::move(tracks);

    // A search on screen must never describe the previous library.
    if (!m_query.isEmpty())
        rebuildResults();

    commit(before);
}

void MusicSearchFacade::search(const QString& query)
{
    const Snapshot before = snapshot();

    const QString applied = query.trimmed();
    m_query = applied;

    if (applied.isEmpty()) {
        m_results.clear();
        m_resultIds.clear();
        m_loading = false;
    } else {
        // The history trims, dedupes ignoring case and caps itself.
        m_history.add(applied, kMaxHistoryEntries);
        rebuildResults();
    }

    commit(before);
}

void MusicSearchFacade::clear()
{
    const Snapshot before = snapshot();

    m_query.clear();
    m_results.clear();
    m_resultIds.clear();
    m_loading = false;

    commit(before);
}

void MusicSearchFacade::addProviderLink(const QString& url)
{
    const Snapshot before = snapshot();

    const telegram::ProviderLink link = telegram::parseMusicProviderLink(url);
    if (link.provider.isEmpty() || link.id.isEmpty())
        return;

    if (std::find(m_providerLinks.cbegin(), m_providerLinks.cend(), link)
        != m_providerLinks.cend()) {
        return;
    }

    m_providerLinks.append(link);
    commit(before);
}

QStringList MusicSearchFacade::waves() const
{
    return waveLabels();
}

void MusicSearchFacade::setWaves(const QStringList& ids, const QStringList& titles)
{
    const Snapshot before = snapshot();

    const qsizetype count = std::max(ids.size(), titles.size());
    QStringList waveIds;
    QStringList waveTitles;
    waveIds.reserve(count);
    waveTitles.reserve(count);

    QSet<QString> seenIds;
    for (qsizetype i = 0; i < count; ++i) {
        const QString id = ids.value(i).trimmed();
        if (id.isEmpty() || seenIds.contains(id))
            continue;
        seenIds.insert(id);
        waveIds.append(id);
        waveTitles.append(titles.value(i).trimmed());
    }

    m_waveIds = std::move(waveIds);
    m_waveTitles = std::move(waveTitles);

    commit(before);
}

QString MusicSearchFacade::resultIdAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_resultIds.size()))
        return {};
    return m_resultIds.at(index);
}

void MusicSearchFacade::playResult(int index)
{
    const QString trackId = resultIdAt(index);
    if (trackId.isEmpty())
        return;
    emit playRequested(trackId);
}

QStringList MusicSearchFacade::providerLinks() const
{
    return providerLinkLabels();
}

void MusicSearchFacade::setLoading(bool loading)
{
    if (m_loading == loading)
        return;

    const Snapshot before = snapshot();
    m_loading = loading;
    commit(before);
}

MusicSearchFacade::Snapshot MusicSearchFacade::snapshot() const
{
    return Snapshot{m_query, m_results, m_resultIds, m_history.entries(), m_loading,
                    waveLabels(), providerLinkLabels()};
}

void MusicSearchFacade::rebuildResults()
{
    m_results.clear();
    m_resultIds.clear();

    if (m_query.isEmpty())
        return;

    telegram::MusicSearchQuery query;
    query.text = m_query;
    query.limit = kSearchLimit;

    const QVector<telegram::MusicTrackModel> matches =
        telegram::MusicSearchEngine::search(m_tracks, {}, query);

    m_results.reserve(matches.size());
    m_resultIds.reserve(matches.size());
    for (const telegram::MusicTrackModel& track : matches) {
        m_resultIds.append(track.id);
        m_results.append(resultLabel(track));
    }
}

QStringList MusicSearchFacade::waveLabels() const
{
    QStringList labels;
    labels.reserve(m_waveIds.size());
    for (qsizetype i = 0; i < m_waveIds.size(); ++i) {
        const QString title = m_waveTitles.value(i);
        labels.append(title.isEmpty() ? m_waveIds.at(i) : title);
    }
    return labels;
}

QStringList MusicSearchFacade::providerLinkLabels() const
{
    QStringList labels;
    labels.reserve(m_providerLinks.size());
    for (const telegram::ProviderLink& link : m_providerLinks) {
        const QString name = telegram::providerDisplayName(link.provider);
        QString label = name.isEmpty() ? link.provider : name;
        if (!link.kind.isEmpty())
            label += QStringLiteral(" \u00B7 ") + kindLabel(link.kind);
        if (!link.id.isEmpty())
            label += QStringLiteral(" \u00B7 ") + link.id;
        labels.append(label);
    }
    return labels;
}

void MusicSearchFacade::commit(const Snapshot& before)
{
    if (snapshot() == before)
        return;
    emit changed();
}

} // namespace beygram::ui
