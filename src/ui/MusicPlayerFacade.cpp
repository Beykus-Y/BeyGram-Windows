#include "MusicPlayerFacade.h"

#include "core/AppPaths.h"

#include <QAudioOutput>
#include <QDateTime>
#include <QDir>
#include <QMediaPlayer>

#include <utility>

namespace beygram::ui {

namespace {

/// The default location of the saved music session. Tests point the facade at
/// a temporary directory through BEYGRAM_MUSIC_SESSION_PATH instead of writing
/// into the user's config directory.
QString defaultSessionPath()
{
    const QString override = qEnvironmentVariable("BEYGRAM_MUSIC_SESSION_PATH");
    if (!override.isEmpty())
        return override;

    return QDir(AppPaths::dataDirectory()).filePath(QStringLiteral("music/session.json"));
}

QString repeatModeName(telegram::RepeatMode mode)
{
    switch (mode) {
    case telegram::RepeatMode::All:
        return QStringLiteral("all");
    case telegram::RepeatMode::One:
        return QStringLiteral("one");
    case telegram::RepeatMode::Off:
        break;
    }
    return QStringLiteral("off");
}

} // namespace

MusicPlayerFacade::MusicPlayerFacade(QObject* parent)
    : QObject(parent), m_sessionStore(defaultSessionPath())
{
    // trackChanged() is forwarded as-is. changed() is not forwarded: every
    // mutation goes through this facade, which compares snapshots and emits
    // changed() once per call instead of once per internal model.
    connect(&m_player, &telegram::MusicPlayer::trackChanged,
            this, &MusicPlayerFacade::trackChanged);
    connect(&m_player, &telegram::MusicPlayer::changed, this,
            [this] { syncAudio(); });
}

void MusicPlayerFacade::ensureAudio()
{
    if (m_audio)
        return;

    m_output = new QAudioOutput(this);
    m_output->setVolume(1.0f);
    m_audio = new QMediaPlayer(this);
    m_audio->setAudioOutput(m_output);
    m_audio->setPlaybackRate(static_cast<float>(m_settings.speed()));

    connect(m_audio, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
        if (m_loadedTrackId.isEmpty() || m_loadedTrackId != m_player.currentTrackId())
            return;
        const Snapshot before = snapshot();
        m_player.setPositionMs(static_cast<int>(position));
        commit(before);
    });
    connect(m_audio, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
        if (m_loadedTrackId.isEmpty() || m_loadedTrackId != m_player.currentTrackId())
            return;
        const Snapshot before = snapshot();
        m_player.setDurationMs(static_cast<int>(duration));
        commit(before);
    });
    connect(m_audio, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status != QMediaPlayer::EndOfMedia)
                    return;
                const QString previous = m_player.currentTrackId();
                next();
                if (m_player.currentTrackId() == previous && m_player.playing()) {
                    // Repeat-one stays on the same track and emits no
                    // trackChanged(); rewind explicitly so it can play again.
                    m_audio->setPosition(0);
                    m_audio->play();
                }
            });
    connect(m_audio, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString&) {
                const Snapshot before = snapshot();
                m_player.setPlaying(false);
                commit(before);
            });
}

void MusicPlayerFacade::syncAudio()
{
    const QString id = m_player.currentTrackId();
    const QUrl source = m_sources.value(id);
    if (id.isEmpty() || source.isEmpty()) {
        if (m_audio && m_audio->playbackState() != QMediaPlayer::StoppedState)
            m_audio->stop();
        m_loadedTrackId.clear();
        return;
    }

    ensureAudio();
    if (m_loadedTrackId != id) {
        m_loadedTrackId = id;
        m_audio->setSource(source);
        m_audio->setPlaybackRate(static_cast<float>(m_settings.speed()));
    }

    if (m_player.playing()) {
        if (m_audio->playbackState() != QMediaPlayer::PlayingState)
            m_audio->play();
    } else if (m_audio->playbackState() == QMediaPlayer::PlayingState) {
        m_audio->pause();
    }
}

QString MusicPlayerFacade::title() const
{
    return m_titles.value(m_player.currentTrackId());
}

bool MusicPlayerFacade::playing() const
{
    return m_player.playing();
}

int MusicPlayerFacade::positionMs() const
{
    return m_player.positionMs();
}

int MusicPlayerFacade::durationMs() const
{
    return m_player.durationMs();
}

double MusicPlayerFacade::speed() const
{
    return m_settings.speed();
}

void MusicPlayerFacade::setSpeed(double speed)
{
    const Snapshot before = snapshot();
    m_settings.setSpeed(speed);
    if (m_audio)
        m_audio->setPlaybackRate(static_cast<float>(m_settings.speed()));
    commit(before);
}

bool MusicPlayerFacade::shuffle() const
{
    return m_player.shuffle();
}

void MusicPlayerFacade::setShuffle(bool on)
{
    const Snapshot before = snapshot();
    m_player.setShuffle(on);
    commit(before);
}

QString MusicPlayerFacade::repeatMode() const
{
    return repeatModeName(m_player.repeatMode());
}

bool MusicPlayerFacade::hasNext() const
{
    const QVector<QString> queue = m_player.queue();
    if (queue.isEmpty())
        return false;

    // Nothing is current yet, so next() starts the first entry.
    if (m_player.currentTrackId().isEmpty())
        return true;

    // Repeat One keeps the current track and Repeat All wraps, so either way
    // next() leaves a track current even with a single-entry queue.
    if (m_player.repeatMode() != telegram::RepeatMode::Off)
        return true;

    // MusicPlayer owns the shuffled order, so its exact last entry is not
    // visible here. Any second entry is a safe answer; the last step may still
    // stop playback when the current track happens to be last in that order.
    if (m_player.shuffle())
        return queue.size() > 1;

    return m_player.currentIndex() + 1 < queue.size();
}

void MusicPlayerFacade::setQueue(const QStringList& trackIds, const QStringList& titles)
{
    const Snapshot before = snapshot();

    QVector<QString> ids;
    ids.reserve(trackIds.size());
    QHash<QString, QString> nextTitles;
    nextTitles.reserve(trackIds.size());

    for (qsizetype index = 0; index < trackIds.size(); ++index) {
        const QString& id = trackIds.at(index);
        if (id.isEmpty())
            continue;

        ids.append(id);
        const QString title = titles.value(index);
        // MusicPlayer keeps the first occurrence of a duplicate id, so the
        // first non-empty title wins for that id as well.
        if (!title.isEmpty() && !nextTitles.contains(id))
            nextTitles.insert(id, title);
    }

    m_titles = std::move(nextTitles);
    m_player.setQueue(ids);
    commit(before);
}

void MusicPlayerFacade::playPause()
{
    const Snapshot before = snapshot();

    if (m_player.currentTrackId().isEmpty()) {
        if (m_player.queue().isEmpty())
            return;
        m_player.playAt(0);
    } else {
        m_player.setPlaying(!m_player.playing());
    }

    commit(before);
}

void MusicPlayerFacade::next()
{
    const Snapshot before = snapshot();

    if (m_player.currentTrackId().isEmpty()) {
        if (m_player.queue().isEmpty())
            return;
        m_player.playAt(0);
    } else {
        m_player.next();
    }

    commit(before);
}

void MusicPlayerFacade::previous()
{
    const Snapshot before = snapshot();

    if (m_player.currentTrackId().isEmpty()) {
        const int last = static_cast<int>(m_player.queue().size()) - 1;
        if (last < 0)
            return;
        m_player.playAt(last);
    } else {
        m_player.previous();
    }

    commit(before);
}

void MusicPlayerFacade::seek(int ms)
{
    const Snapshot before = snapshot();
    m_player.setPositionMs(ms);
    if (m_audio && m_loadedTrackId == m_player.currentTrackId())
        m_audio->setPosition(m_player.positionMs());
    commit(before);
}

void MusicPlayerFacade::setDurationMs(int ms)
{
    const Snapshot before = snapshot();
    m_player.setDurationMs(ms);
    commit(before);
}

void MusicPlayerFacade::cycleRepeat()
{
    const Snapshot before = snapshot();

    switch (m_player.repeatMode()) {
    case telegram::RepeatMode::Off:
        m_player.setRepeatMode(telegram::RepeatMode::All);
        break;
    case telegram::RepeatMode::All:
        m_player.setRepeatMode(telegram::RepeatMode::One);
        break;
    case telegram::RepeatMode::One:
        m_player.setRepeatMode(telegram::RepeatMode::Off);
        break;
    }

    commit(before);
}

void MusicPlayerFacade::saveSession()
{
    telegram::MusicSessionState state;
    state.trackId = m_player.currentTrackId();
    state.queue = m_player.queue();
    state.index = m_player.currentIndex();
    state.positionMs = m_player.positionMs();
    state.durationMs = m_player.durationMs();
    state.playing = m_player.playing();
    state.shuffle = m_player.shuffle();
    state.repeatMode = static_cast<int>(m_player.repeatMode());
    // PlaybackSettings is the user-preference model; MusicPlayer only mirrors
    // the value it was given. Saving the settings value keeps a later restore
    // from picking up a stale player-side copy.
    state.crossfadeMs = m_settings.crossfadeMs();
    state.savedAt = QDateTime::currentSecsSinceEpoch();

    // A failed write leaves the previous session intact (MusicSessionStore uses
    // QSaveFile). There is nothing observable to report here.
    m_sessionStore.save(state);
}

void MusicPlayerFacade::restoreSession()
{
    const telegram::MusicSessionState saved = m_sessionStore.load();
    if (saved.queue.isEmpty())
        return;

    const telegram::MusicSessionState session =
        telegram::restoreMusicSession(saved, QDateTime::currentSecsSinceEpoch());
    if (session.queue.isEmpty())
        return;

    const Snapshot before = snapshot();

    m_player.setQueue(session.queue);
    m_player.setShuffle(session.shuffle);
    m_player.setRepeatMode(static_cast<telegram::RepeatMode>(session.repeatMode));
    m_player.setCrossfadeMs(session.crossfadeMs);
    m_settings.setCrossfadeMs(session.crossfadeMs);

    if (session.index >= 0 && session.index < m_player.queue().size()) {
        // playAt() rewinds the track and starts it, so duration, position and
        // the play flag are applied afterwards to reach the saved state.
        m_player.playAt(session.index);
        m_player.setDurationMs(session.durationMs);
        m_player.setPositionMs(session.positionMs);
        m_player.setPlaying(session.playing);
    }

    // Titles are caller-supplied metadata, not part of the session document.
    // Keep the ones that still name a queued track and drop the rest.
    const QVector<QString> queue = m_player.queue();
    for (auto it = m_titles.begin(); it != m_titles.end();) {
        if (!queue.contains(it.key()))
            it = m_titles.erase(it);
        else
            ++it;
    }

    commit(before);
}

void MusicPlayerFacade::setTitleFor(const QString& id, const QString& title)
{
    if (id.isEmpty())
        return;

    const Snapshot before = snapshot();

    if (title.isEmpty()) {
        if (m_titles.remove(id) == 0)
            return;
    } else {
        if (m_titles.value(id) == title)
            return;
        m_titles.insert(id, title);
    }

    commit(before);
}

void MusicPlayerFacade::setSourceFor(const QString& id, const QUrl& source)
{
    const QString cleanId = id.trimmed();
    if (cleanId.isEmpty())
        return;

    if (!source.isValid() || source.isEmpty()) {
        m_sources.remove(cleanId);
    } else {
        if (m_sources.value(cleanId) == source)
            return;
        m_sources.insert(cleanId, source);
    }

    if (cleanId == m_player.currentTrackId())
        syncAudio();
}

bool MusicPlayerFacade::hasSource(const QString& id) const
{
    return m_sources.contains(id.trimmed());
}

void MusicPlayerFacade::playTrack(const QString& id)
{
    const QString cleanId = id.trimmed();
    if (cleanId.isEmpty())
        return;

    const QVector<QString> queue = m_player.queue();
    const int index = static_cast<int>(queue.indexOf(cleanId));
    if (index < 0)
        return;

    const Snapshot before = snapshot();
    m_player.playAt(index);
    commit(before);
}

MusicPlayerFacade::Snapshot MusicPlayerFacade::snapshot() const
{
    Snapshot state;
    state.title = title();
    state.playing = playing();
    state.positionMs = positionMs();
    state.durationMs = durationMs();
    state.speed = speed();
    state.shuffle = shuffle();
    state.repeatMode = repeatMode();
    state.hasNext = hasNext();
    return state;
}

void MusicPlayerFacade::commit(const Snapshot& before)
{
    if (before == snapshot())
        return;

    emit changed();
}

} // namespace beygram::ui
