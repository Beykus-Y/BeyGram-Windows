#include "MusicPlayer.h"

#include <QByteArray>
#include <QSet>

#include <algorithm>
#include <numeric>

namespace beygram::telegram {

namespace {

// The overlap may be turned off entirely or stretched, but never beyond the
// range the Android player exposes.
constexpr int kMaxCrossfadeMs = 12000;

// FNV-1a over the UTF-8 bytes. It is fixed across runs and platforms, unlike
// qHash(), whose seed may be randomized, and it needs no random number
// generator: the shuffled order is a pure function of the track ids.
quint64 stableHash(const QString& text)
{
    constexpr quint64 kOffsetBasis = 14695981039346656037ULL;
    constexpr quint64 kPrime = 1099511628211ULL;

    quint64 hash = kOffsetBasis;
    for (const char byte : text.toUtf8()) {
        hash ^= static_cast<quint64>(static_cast<unsigned char>(byte));
        hash *= kPrime;
    }
    return hash;
}

// Queue indices sorted by the hash of their id, ties keeping queue order.
QVector<int> shuffledOrder(const QVector<QString>& queue)
{
    QVector<quint64> hashes;
    hashes.reserve(queue.size());
    for (const QString& id : queue)
        hashes.append(stableHash(id));

    QVector<int> order(queue.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&hashes](int lhs, int rhs) {
        return hashes.at(lhs) < hashes.at(rhs);
    });
    return order;
}

QVector<int> identityOrder(int size)
{
    QVector<int> order(size);
    std::iota(order.begin(), order.end(), 0);
    return order;
}

} // namespace

MusicPlayer::MusicPlayer(QObject* parent) : QObject(parent)
{
}

void MusicPlayer::setQueue(const QVector<QString>& trackIds)
{
    QVector<QString> cleaned;
    cleaned.reserve(trackIds.size());
    QSet<QString> seen;
    for (const QString& id : trackIds) {
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);
        cleaned.append(id);
    }

    if (cleaned == m_queue)
        return;

    const QString previousId = currentTrackId();
    m_queue = cleaned;

    // The current track survives a reorder as long as its id is still queued.
    const int focus = previousId.isEmpty() ? -1 : m_queue.indexOf(previousId);
    if (previousId.isEmpty() || focus >= 0) {
        rebuildOrder(focus);
        emit changed();
        return;
    }

    // The queue edit removed the current track, so playback stops.
    m_orderPosition = -1;
    m_positionMs = 0;
    m_durationMs = 0;
    m_playing = false;
    rebuildOrder(-1);
    emit trackChanged(QString());
    emit changed();
}

QVector<QString> MusicPlayer::queue() const
{
    return m_queue;
}

int MusicPlayer::currentIndex() const
{
    if (m_orderPosition < 0 || m_orderPosition >= m_order.size())
        return -1;

    const int index = m_order.at(m_orderPosition);
    if (index < 0 || index >= m_queue.size())
        return -1;
    return index;
}

QString MusicPlayer::currentTrackId() const
{
    const int index = currentIndex();
    return index >= 0 ? m_queue.at(index) : QString();
}

void MusicPlayer::playAt(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;

    const int orderPosition = m_order.indexOf(index);
    if (orderPosition < 0)
        return;

    const bool sameTrack = index == currentIndex();
    m_orderPosition = orderPosition;

    const bool positionRewound = m_positionMs != 0;
    const bool startedPlaying = !m_playing;
    m_positionMs = 0;
    m_playing = true;

    if (!sameTrack) {
        // The backend reports the new duration once it has loaded the track.
        m_durationMs = 0;
        emit trackChanged(m_queue.at(index));
    }
    if (!sameTrack || positionRewound || startedPlaying)
        emit changed();
}

bool MusicPlayer::next()
{
    if (m_queue.isEmpty() || m_orderPosition < 0)
        return false;

    // Repeat-one is a stay, not a rewind: the backend keeps playing the track.
    if (m_repeatMode == RepeatMode::One)
        return true;

    if (m_orderPosition + 1 < m_order.size()) {
        activate(m_orderPosition + 1);
        return true;
    }

    if (m_repeatMode == RepeatMode::All) {
        activate(0);
        return true;
    }

    if (!m_playing)
        return false;

    // The last track stays current; only playback stops.
    m_playing = false;
    emit changed();
    return false;
}

bool MusicPlayer::previous()
{
    if (m_queue.isEmpty() || m_orderPosition < 0)
        return false;

    if (m_orderPosition > 0) {
        activate(m_orderPosition - 1);
        return true;
    }

    // RepeatMode::One deliberately has no effect here.
    if (m_repeatMode == RepeatMode::All) {
        activate(m_order.size() - 1);
        return true;
    }

    return false;
}

void MusicPlayer::setShuffle(bool on)
{
    if (on == m_shuffle)
        return;

    const int focus = currentIndex();
    m_shuffle = on;
    rebuildOrder(focus);
    emit changed();
}

bool MusicPlayer::shuffle() const
{
    return m_shuffle;
}

void MusicPlayer::setRepeatMode(RepeatMode mode)
{
    switch (mode) {
    case RepeatMode::Off:
    case RepeatMode::All:
    case RepeatMode::One:
        break;
    default:
        return;
    }

    if (mode == m_repeatMode)
        return;

    m_repeatMode = mode;
    emit changed();
}

RepeatMode MusicPlayer::repeatMode() const
{
    return m_repeatMode;
}

void MusicPlayer::setCrossfadeMs(int ms)
{
    const int clamped = std::clamp(ms, 0, kMaxCrossfadeMs);
    if (clamped == m_crossfadeMs)
        return;

    m_crossfadeMs = clamped;
    emit changed();
}

int MusicPlayer::crossfadeMs() const
{
    return m_crossfadeMs;
}

void MusicPlayer::setPositionMs(int ms)
{
    const int clamped = std::clamp(ms, 0, m_durationMs);
    if (clamped == m_positionMs)
        return;

    m_positionMs = clamped;
    emit changed();
}

int MusicPlayer::positionMs() const
{
    return m_positionMs;
}

void MusicPlayer::setDurationMs(int ms)
{
    const int clamped = std::max(ms, 0);
    if (clamped == m_durationMs)
        return;

    m_durationMs = clamped;
    m_positionMs = std::min(m_positionMs, m_durationMs);
    emit changed();
}

int MusicPlayer::durationMs() const
{
    return m_durationMs;
}

void MusicPlayer::setPlaying(bool playing)
{
    if (playing == m_playing)
        return;

    m_playing = playing;
    emit changed();
}

bool MusicPlayer::playing() const
{
    return m_playing;
}

void MusicPlayer::clear()
{
    const bool hadTrack = !currentTrackId().isEmpty();
    if (m_queue.isEmpty() && m_positionMs == 0 && m_durationMs == 0 && !m_playing)
        return;

    m_queue.clear();
    m_order.clear();
    m_orderPosition = -1;
    m_positionMs = 0;
    m_durationMs = 0;
    m_playing = false;

    if (hadTrack)
        emit trackChanged(QString());
    emit changed();
}

void MusicPlayer::rebuildOrder(int focusIndex)
{
    m_order = m_shuffle ? shuffledOrder(m_queue) : identityOrder(m_queue.size());
    m_orderPosition = focusIndex >= 0 ? m_order.indexOf(focusIndex) : -1;
}

void MusicPlayer::activate(int orderPosition)
{
    const QString previousId = currentTrackId();
    m_orderPosition = orderPosition;
    const QString trackId = currentTrackId();
    const bool trackChangedNow = trackId != previousId;

    const bool positionRewound = m_positionMs != 0;
    const bool startedPlaying = !m_playing;
    if (trackChangedNow)
        m_durationMs = 0;
    m_positionMs = 0;
    m_playing = true;

    if (trackChangedNow)
        emit trackChanged(trackId);
    if (trackChangedNow || positionRewound || startedPlaying)
        emit changed();
}

} // namespace beygram::telegram
