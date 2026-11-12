#include "AudioTrackModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool storesTrack(const QVector<AudioTrack>& tracks, qint64 messageId)
{
    return std::any_of(tracks.cbegin(), tracks.cend(),
                       [messageId](const AudioTrack& track) {
                           return track.messageId == messageId;
                       });
}

// AudioTrack carries no operator==, so the change check compares the five
// fields explicitly. Keeps setTracks() from notifying on a no-op assignment.
bool sameTracks(const QVector<AudioTrack>& lhs, const QVector<AudioTrack>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                      [](const AudioTrack& left, const AudioTrack& right) {
                          return left.messageId == right.messageId
                              && left.title == right.title
                              && left.performer == right.performer
                              && left.durationSeconds == right.durationSeconds
                              && left.fileName == right.fileName;
                      });
}

// Builds "Performer — Title" with a real em dash. The source stays pure ASCII
// so the literal does not depend on the compiler's source charset.
QString joinedLabel(const QString& performer, const QString& title)
{
    return performer + QLatin1Char(' ') + QChar(0x2014) + QLatin1Char(' ')
        + title;
}

} // namespace

AudioTrackModel::AudioTrackModel(QObject* parent)
    : QObject(parent)
{
}

void AudioTrackModel::setTracks(QVector<AudioTrack> tracks)
{
    QVector<AudioTrack> next;
    next.reserve(tracks.size());

    for (AudioTrack& track : tracks) {
        // A track without a message id cannot be played or addressed.
        if (track.messageId <= 0)
            continue;
        // A negative length is meaningless; show it as unknown rather than
        // letting it eat into totalDuration().
        if (track.durationSeconds < 0)
            track.durationSeconds = 0;
        // One message holds one audio file; the first track and its tags win
        // over later repeats.
        if (storesTrack(next, track.messageId))
            continue;
        next.append(std::move(track));
    }

    if (sameTracks(next, m_tracks))
        return;

    m_tracks = std::move(next);
    emit changed();
}

QVector<AudioTrack> AudioTrackModel::tracks() const
{
    return m_tracks;
}

int AudioTrackModel::count() const
{
    return static_cast<int>(m_tracks.size());
}

QVector<AudioTrack> AudioTrackModel::byPerformer(const QString& performer) const
{
    QVector<AudioTrack> matches;

    // An empty performer would otherwise match every untagged track.
    if (performer.isEmpty())
        return matches;

    matches.reserve(m_tracks.size());

    for (const AudioTrack& track : m_tracks) {
        if (QString::compare(track.performer, performer, Qt::CaseInsensitive) == 0)
            matches.append(track);
    }

    return matches;
}

QString AudioTrackModel::displayTitle(int index) const
{
    if (index < 0 || index >= m_tracks.size())
        return QString();

    const AudioTrack& track = m_tracks.at(index);

    if (track.title.isEmpty() && track.performer.isEmpty())
        return QStringLiteral("Unknown title");

    // Untagged performer or title leaves a lone name instead of a dangling
    // separator.
    if (track.performer.isEmpty())
        return track.title;
    if (track.title.isEmpty())
        return track.performer;

    return joinedLabel(track.performer, track.title);
}

int AudioTrackModel::totalDuration() const
{
    int seconds = 0;

    for (const AudioTrack& track : m_tracks)
        seconds += track.durationSeconds;

    return seconds;
}

void AudioTrackModel::clear()
{
    if (m_tracks.isEmpty())
        return;

    m_tracks.clear();
    emit changed();
}

} // namespace beygram::telegram
