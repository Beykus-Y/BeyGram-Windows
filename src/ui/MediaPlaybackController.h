#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QAudioOutput;
class QMediaPlayer;

namespace beygram::ui {

/// Playback state for already-downloaded audio and video files.
///
/// The controller only accepts local files that already exist on disk (the
/// cached TDLib paths handed over by the media store). It never downloads,
/// never resolves remote URLs, and never logs file system paths. QML binds to
/// the properties and calls the invokables; the controller owns a
/// QMediaPlayer plus a QAudioOutput and reports failures through `error`.
class MediaPlaybackController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(bool open READ isOpen NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY stateChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QObject* mediaPlayer READ mediaPlayer CONSTANT)

public:
    explicit MediaPlaybackController(QObject* parent = nullptr);
    ~MediaPlaybackController() override;

    [[nodiscard]] QUrl source() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool playing() const;
    [[nodiscard]] qint64 position() const;
    [[nodiscard]] qint64 duration() const;
    [[nodiscard]] qreal volume() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] QObject* mediaPlayer() const;

    Q_INVOKABLE bool open(const QUrl& localSource, const QString& title = {});
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qint64 milliseconds);
    void setVolume(qreal volume);

signals:
    void stateChanged();
    void positionChanged();

private:
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio = nullptr;
    QUrl m_source;
    QString m_title;
    QString m_error;
    qreal m_volume = 1.0;
};

} // namespace beygram::ui
