#include "MusicLibrary.h"

#include <QSet>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool sameTrack(const LibraryTrack& left, const LibraryTrack& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.performer == right.performer
        && left.album == right.album
        && left.durationSeconds == right.durationSeconds
        && left.fileId == right.fileId;
}

// LibraryTrack carries no operator==, so the change check compares every field
// explicitly. Keeps setTracks() from notifying on a no-op assignment.
bool sameTracks(const QVector<LibraryTrack>& lhs, const QVector<LibraryTrack>& rhs)
{
    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), sameTrack);
}

bool caseInsensitiveLess(const QString& left, const QString& right)
{
    const int folded = QString::compare(left, right, Qt::CaseInsensitive);
    if (folded != 0)
        return folded < 0;
    // Same letters in a different case: fall back to the exact order so the
    // result stays stable.
    return QString::compare(left, right) < 0;
}

// Distinct non-empty values of one field, sorted ignoring case. Values that
// differ only in case are the same value; the first stored spelling is kept.
QStringList sortedUniqueValues(const QVector<LibraryTrack>& tracks,
                               QString LibraryTrack::*field)
{
    QStringList values;
    values.reserve(tracks.size());
    QSet<QString> seen;

    for (const LibraryTrack& track : tracks) {
        const QString value = track.*field;
        if (value.isEmpty())
            continue;
        const QString key = value.toCaseFolded();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        values.append(value);
    }

    std::sort(values.begin(), values.end(), caseInsensitiveLess);
    return values;
}

} // namespace

MusicLibrary::MusicLibrary(QObject* parent)
    : QObject(parent)
{
}

void MusicLibrary::setTracks(QVector<LibraryTrack> tracks)
{
    QVector<LibraryTrack> next;
    next.reserve(tracks.size());
    QSet<qint64> seen;

    for (LibraryTrack& track : tracks) {
        // A track without an id cannot be addressed or played.
        if (track.id <= 0)
            continue;
        // A negative length is meaningless; show it as unknown rather than
        // letting it skew the library.
        if (track.durationSeconds < 0)
            track.durationSeconds = 0;
        // One id names one track; the first track and its tags win over later
        // repeats.
        if (seen.contains(track.id))
            continue;
        seen.insert(track.id);
        next.append(std::move(track));
    }

    if (sameTracks(next, m_tracks))
        return;

    m_tracks = std::move(next);
    emit changed();
}

QVector<LibraryTrack> MusicLibrary::tracks() const
{
    return m_tracks;
}

int MusicLibrary::count() const
{
    return static_cast<int>(m_tracks.size());
}

QVector<LibraryTrack> MusicLibrary::search(const QString& query) const
{
    // A blank query is no filter at all; searching for the spaces themselves
    // would only hide tracks.
    const QString needle = query.trimmed();
    if (needle.isEmpty())
        return m_tracks;

    QVector<LibraryTrack> matches;
    matches.reserve(m_tracks.size());

    for (const LibraryTrack& track : m_tracks) {
        if (track.title.contains(needle, Qt::CaseInsensitive)
            || track.performer.contains(needle, Qt::CaseInsensitive)
            || track.album.contains(needle, Qt::CaseInsensitive)) {
            matches.append(track);
        }
    }

    return matches;
}

QStringList MusicLibrary::performers() const
{
    return sortedUniqueValues(m_tracks, &LibraryTrack::performer);
}

QStringList MusicLibrary::albums() const
{
    return sortedUniqueValues(m_tracks, &LibraryTrack::album);
}

QVector<LibraryTrack> MusicLibrary::byPerformer(const QString& performer) const
{
    QVector<LibraryTrack> matches;

    // An empty performer would otherwise match every untagged track.
    if (performer.isEmpty())
        return matches;

    matches.reserve(m_tracks.size());

    for (const LibraryTrack& track : m_tracks) {
        if (QString::compare(track.performer, performer, Qt::CaseInsensitive) == 0)
            matches.append(track);
    }

    return matches;
}

void MusicLibrary::clear()
{
    if (m_tracks.isEmpty())
        return;

    m_tracks.clear();
    emit changed();
}

} // namespace beygram::telegram
