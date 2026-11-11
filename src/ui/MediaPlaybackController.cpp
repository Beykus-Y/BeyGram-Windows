#include "MediaPlaybackController.h"

#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaPlayer>

#include <cmath>

namespace beygram::ui {
namespace {

/// Resolves a QML-provided source into a local file path.
///
/// Accepts `file:` URLs, plain local paths and Windows drive letters (which
/// QUrl parses as a one-letter scheme). Everything else — http(s), rtsp,
/// data, qrc — is rejected so playback can never reach the network.
QString localPath(const QUrl& url)
{
    if (url.isEmpty())
        return {};

    if (url.isLocalFile())
        return url.toLocalFile();

    if (url.scheme().isEmpty())
        return url.path(QUrl::FullyDecoded);

    const QString scheme = url.scheme();
    if (scheme.size() == 1 && scheme.at(0).isLetter() && url.path().startsWith(QLatin1Char('/'))) {
        QString rest = url.path(QUrl::FullyDecoded);
        rest.replace(QLatin1Char('\\'), QLatin1Char('/'));
        return scheme + QLatin1Char(':') + rest;
    }

    return {};
}

/// Stops playback and drops the loaded media without touching the volume.
void resetPlayer(QMediaPlayer* player)
{
    if (!player)
        return;
    player->stop();
    player->setSource(QUrl());
}

/// Player error strings may quote the file path; never hand that to QML.
QString playbackErrorText(const QString& message, const QUrl& source)
{
    QString text = message.trimmed();
    const QString path = source.toLocalFile();
    if (!path.isEmpty() && text.contains(path, Qt::CaseInsensitive))
        text.clear();
    if (text.isEmpty())
        text = QStringLiteral("Playback failed");
    return text;
}

} // namespace

MediaPlaybackController::MediaPlaybackController(QObject* parent)
    : QObject(parent), m_player(new QMediaPlayer(this)), m_audio(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(m_volume);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState) {
                emit stateChanged();
                emit positionChanged();
            });

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::InvalidMedia && m_error.isEmpty())
                    m_error = QStringLiteral("Unsupported media");
                emit stateChanged();
            });

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& message) {
                m_error = playbackErrorText(message, m_source);
                emit stateChanged();
            });

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64) {
        emit positionChanged();
    });

    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64) {
        emit stateChanged();
    });
}

MediaPlaybackController::~MediaPlaybackController()
{
    // Detach before the children are destroyed so a backend callback cannot
    // touch a half-destroyed controller.
    if (m_player) {
        m_player->disconnect(this);
        m_player->stop();
        m_player->setSource(QUrl());
        m_player->setAudioOutput(nullptr);
    }
    if (m_audio)
        m_audio->disconnect(this);
}

QUrl MediaPlaybackController::source() const
{
    return m_source;
}

QString MediaPlaybackController::title() const
{
    return m_title;
}

bool MediaPlaybackController::isOpen() const
{
    return !m_source.isEmpty();
}

bool MediaPlaybackController::playing() const
{
    return m_player && isOpen() && m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 MediaPlaybackController::position() const
{
    if (!m_player || !isOpen())
        return 0;
    return m_player->position();
}

qint64 MediaPlaybackController::duration() const
{
    if (!m_player || !isOpen())
        return 0;
    return m_player->duration();
}

qreal MediaPlaybackController::volume() const
{
    return m_volume;
}

QString MediaPlaybackController::error() const
{
    return m_error;
}

QObject* MediaPlaybackController::mediaPlayer() const
{
    return m_player;
}

bool MediaPlaybackController::open(const QUrl& localSource, const QString& title)
{
    m_error.clear();

    const QString path = localPath(localSource);
    if (path.isEmpty()) {
        if (localSource.isEmpty())
            m_error = QStringLiteral("No media file");
        else
            m_error = QStringLiteral("Only downloaded media can be played");

        resetPlayer(m_player);
        m_source.clear();
        m_title.clear();
        emit stateChanged();
        emit positionChanged();
        return false;
    }

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        m_error = QStringLiteral("The media file is unavailable");

        resetPlayer(m_player);
        m_source.clear();
        m_title.clear();
        emit stateChanged();
        emit positionChanged();
        return false;
    }

    resetPlayer(m_player);

    m_source = QUrl::fromLocalFile(info.absoluteFilePath());
    m_title = title.isEmpty() ? info.fileName() : title;

    if (m_player)
        m_player->setSource(m_source);

    emit stateChanged();
    emit positionChanged();
    return true;
}

void MediaPlaybackController::play()
{
    if (!m_player || !isOpen())
        return;
    m_player->play();
}

void MediaPlaybackController::pause()
{
    if (!m_player || !isOpen())
        return;
    m_player->pause();
}

void MediaPlaybackController::toggle()
{
    if (!m_player || !isOpen())
        return;
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void MediaPlaybackController::stop()
{
    if (!m_player || !isOpen())
        return;

    m_player->stop();
    if (m_player->position() != 0) {
        m_player->setPosition(0);
        emit positionChanged();
    }
    emit stateChanged();
}

void MediaPlaybackController::seek(qint64 milliseconds)
{
    if (!m_player || !isOpen())
        return;

    qint64 target = qMax<qint64>(0, milliseconds);
    const qint64 total = m_player->duration();
    if (total > 0 && target > total)
        target = total;
    m_player->setPosition(target);
}

void MediaPlaybackController::setVolume(qreal volume)
{
    if (!std::isfinite(static_cast<double>(volume)))
        return;

    const qreal clamped = qBound<qreal>(0.0, volume, 1.0);
    if (qFuzzyCompare(clamped, m_volume))
        return;

    m_volume = clamped;
    if (m_audio)
        m_audio->setVolume(clamped);
    emit stateChanged();
}

} // namespace beygram::ui
