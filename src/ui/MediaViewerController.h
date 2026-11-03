#pragma once

#include <QObject>
#include <QString>

namespace beygram::telegram {
class MediaFileStore;
}

namespace beygram::ui {

/// Full-screen image viewer state for already-downloaded TDLib media.
///
/// The controller only exposes local, cached files: it asks the media store to
/// prioritise a download and hands QML an `image://beygrammedia/<fileId>` URL
/// once the file exists. It never performs network requests itself, never
/// exposes file system paths and never logs paths or file contents.
class MediaViewerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY stateChanged)
    Q_PROPERTY(int fileId READ fileId NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(QString source READ source NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)

public:
    explicit MediaViewerController(telegram::MediaFileStore* store, QObject* parent = nullptr);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] int fileId() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QString source() const;
    [[nodiscard]] bool loading() const;
    [[nodiscard]] QString error() const;

    Q_INVOKABLE void open(int fileId, const QString& title);
    Q_INVOKABLE void close();
    Q_INVOKABLE bool saveAs(const QString& targetPath);

signals:
    void stateChanged();

private:
    void updateSource();
    void handleFileReady(int fileId);

    telegram::MediaFileStore* m_store = nullptr;
    bool m_open = false;
    int m_fileId = 0;
    QString m_title;
    QString m_source;
    QString m_error;
    int m_revision = 0;
};

} // namespace beygram::ui
