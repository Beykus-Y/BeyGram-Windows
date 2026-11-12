#include "PlaybackFacade.h"

#include <QVector>

#include <utility>

namespace beygram::ui {

namespace {

/// MediaPlaylistModel addresses an entry by message id *and* chat id, but the
/// facade queue carries no chat context, so every entry gets this placeholder.
/// The facade only ever reads the message id and the title back out of the
/// queue, so the value never reaches the UI.
constexpr qint64 kQueueChatId = 1;

/// Parses a QML-provided id. Anything that is not a positive integer yields 0,
/// which every model here treats as "ignore".
qint64 parseId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

/// Whole seconds for the AudioTrackModel, which stores seconds while the player
/// state stores milliseconds. Positive input never rounds down to zero.
int secondsFromMs(int durationMs)
{
    if (durationMs <= 0)
        return 0;
    return qMax(1, (durationMs + 500) / 1000);
}

} // namespace

PlaybackFacade::PlaybackFacade(QObject* parent) : QObject(parent)
{
}

bool PlaybackFacade::active() const
{
    return m_state.active();
}

QString PlaybackFacade::title() const
{
    return m_title;
}

bool PlaybackFacade::playing() const
{
    return m_state.playing();
}

int PlaybackFacade::positionMs() const
{
    return m_state.positionMs();
}

int PlaybackFacade::durationMs() const
{
    return m_state.durationMs();
}

int PlaybackFacade::volume() const
{
    return m_state.volume();
}

double PlaybackFacade::rate() const
{
    return m_state.rate();
}

QStringList PlaybackFacade::waveform() const
{
    QStringList bars;
    const QVector<double> normalized = m_waveform.normalized();
    bars.reserve(normalized.size());
    for (const double value : normalized)
        bars.append(QString::number(value, 'f', 3));
    return bars;
}

bool PlaybackFacade::hasNext() const
{
    const int total = m_playlist.count();
    if (total == 0)
        return false;

    const int index = m_playlist.currentIndex();
    // With no current entry the queue has not started, so next() would still
    // find the first one.
    if (index < 0)
        return true;
    if (index + 1 < total)
        return true;
    return m_playlist.repeatAll();
}

PlaybackFacade::Snapshot PlaybackFacade::snapshot() const
{
    Snapshot state;
    state.active = active();
    state.title = m_title;
    state.playing = playing();
    state.positionMs = positionMs();
    state.durationMs = durationMs();
    state.volume = volume();
    state.rate = rate();
    state.waveform = waveform();
    state.hasNext = hasNext();
    return state;
}

void PlaybackFacade::commit(const Snapshot& before)
{
    if (before == snapshot())
        return;

    emit changed();
}

void PlaybackFacade::syncQueueCursor()
{
    const qint64 current = m_state.messageId();
    if (current <= 0)
        return;

    const QVector<telegram::PlaylistEntry> entries = m_playlist.entries();
    for (qsizetype index = 0; index < entries.size(); ++index) {
        if (entries.at(index).messageId != current)
            continue;

        m_playlist.playAt(static_cast<int>(index));
        return;
    }
}

void PlaybackFacade::openVideo(const QString& messageId, const QString& title, int durationMs)
{
    const qint64 id = parseId(messageId);
    if (id <= 0 || durationMs <= 0)
        return;

    const Snapshot before = snapshot();

    m_title = title;
    // A video has no envelope and no track tags, so dropping both keeps a
    // previous voice message or audio track from leaking into the new clip.
    m_audioTracks.clear();
    m_waveform.clear();
    m_state.open(id, durationMs);
    syncQueueCursor();

    commit(before);
}

void PlaybackFacade::openAudio(const QString& messageId, const QString& title,
                               const QString& performer, int durationMs)
{
    const qint64 id = parseId(messageId);
    if (id <= 0 || durationMs <= 0)
        return;

    const Snapshot before = snapshot();

    telegram::AudioTrack track;
    track.messageId = id;
    track.title = title;
    track.performer = performer;
    track.durationSeconds = secondsFromMs(durationMs);
    m_audioTracks.setTracks({track});
    // "Performer — Title", or whichever half is present.
    m_title = m_audioTracks.displayTitle(0);

    // The envelope belongs to the clip that is open; the caller feeds the new
    // one through setWaveform() right after this call.
    m_waveform.clear();
    m_state.open(id, durationMs);
    syncQueueCursor();

    commit(before);
}

void PlaybackFacade::playPause()
{
    if (!m_state.active())
        return;

    const Snapshot before = snapshot();
    const QString id = QString::number(m_state.messageId());
    const bool wasPlaying = m_state.playing();

    m_state.setPlaying(!wasPlaying);
    commit(before);

    // The facade holds no transport, so the owner starts or stops the player.
    if (wasPlaying)
        emit pauseRequested(id);
    else
        emit playRequested(id);
}

void PlaybackFacade::stop()
{
    if (!m_state.active())
        return;

    const Snapshot before = snapshot();
    const QString id = QString::number(m_state.messageId());

    m_state.close();
    m_title.clear();
    m_waveform.clear();
    m_audioTracks.clear();
    // The queue survives, so next() can resume it.

    commit(before);
    emit pauseRequested(id);
}

void PlaybackFacade::seek(int ms)
{
    if (!m_state.active())
        return;

    const Snapshot before = snapshot();
    m_state.setPosition(ms);
    // The played part of the envelope follows the playhead.
    m_waveform.setProgress(m_state.progress());
    commit(before);
}

void PlaybackFacade::setVolume(int percent)
{
    if (!m_state.active())
        return;

    const Snapshot before = snapshot();
    m_state.setVolume(percent);
    commit(before);
}

void PlaybackFacade::setRate(double rate)
{
    if (!m_state.active())
        return;

    const Snapshot before = snapshot();
    m_state.setRate(rate);
    commit(before);
}

void PlaybackFacade::setWaveform(const QVariantList& samples)
{
    QVector<int> amplitudes;
    amplitudes.reserve(samples.size());
    for (const QVariant& sample : samples) {
        bool ok = false;
        const int value = sample.toInt(&ok);
        // One bar per incoming element keeps the envelope aligned with the
        // source even when a value cannot be read as a number.
        amplitudes.append(ok ? value : 0);
    }

    const Snapshot before = snapshot();
    m_waveform.setSamples(std::move(amplitudes));
    commit(before);
}

void PlaybackFacade::setQueue(const QStringList& messageIds, const QStringList& titles)
{
    QVector<telegram::PlaylistEntry> entries;
    entries.reserve(messageIds.size());

    for (qsizetype index = 0; index < messageIds.size(); ++index) {
        const qint64 id = parseId(messageIds.at(index));
        if (id <= 0)
            continue;

        telegram::PlaylistEntry entry;
        entry.messageId = id;
        entry.chatId = kQueueChatId;
        entry.title = titles.value(index);
        entries.append(entry);
    }

    const Snapshot before = snapshot();
    m_playlist.setEntries(std::move(entries));
    // Replacing the queue resets the cursor, so point it back at the open clip
    // when the clip is part of the new queue.
    syncQueueCursor();
    commit(before);
}

void PlaybackFacade::advance(bool forward)
{
    if (m_playlist.count() == 0)
        return;

    const Snapshot before = snapshot();
    const bool moved = forward ? m_playlist.next() : m_playlist.previous();
    if (!moved)
        return;

    const telegram::PlaylistEntry entry = m_playlist.current();
    if (entry.messageId <= 0)
        return;

    commit(before);
    // The queue knows an id and a label but not how long the media is, so the
    // facade cannot open the entry on its own: it announces the new entry and
    // the owner opens it with openVideo()/openAudio(), which re-syncs the
    // cursor to the same entry.
    emit playRequested(QString::number(entry.messageId));
}

void PlaybackFacade::next()
{
    advance(true);
}

void PlaybackFacade::previous()
{
    advance(false);
}

} // namespace beygram::ui
