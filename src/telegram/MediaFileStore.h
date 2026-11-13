#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QReadWriteLock>
#include <QSet>
#include <QString>

namespace beygram::telegram {

class TelegramSession;

/// Downloads and remembers the local paths of TDLib files used by the UI
/// (avatars and message thumbnails). Only small preview files are requested;
/// full-size media downloads belong to the media viewer slice.
class MediaFileStore final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit MediaFileStore(QObject* parent = nullptr);

    /// Switches the session that owns the cached file ids. TDLib file ids are
    /// only unique per account, so a real session change drops every cached
    /// path, pending request and late response callback. Passing the current
    /// session leaves the cache untouched.
    void setSession(TelegramSession* session, bool reconnect = true);

    /// Registers a TDLib file object and requests its download when needed.
    void trackFileObject(const QJsonObject& file, int priority = 16);
    void trackFileId(int fileId, int priority = 16);

    [[nodiscard]] QString path(int fileId) const;
    [[nodiscard]] bool isReady(int fileId) const;
    /// True when the last download request for this file failed. The UI uses it
    /// to replace the endless spinner with a retry label.
    [[nodiscard]] bool isFailed(int fileId) const;
    [[nodiscard]] int revision() const { return m_revision; }

signals:
    void fileReady(int fileId);
    void fileFailed(int fileId);
    void revisionChanged();

private slots:
    /// Applies the `file` object returned by a downloadFile request. Responses
    /// issued for an earlier session are ignored: their generation is stale.
    void handleDownloadResponse(int sessionGeneration, const QJsonObject& response);

private:
    void handleUpdate(const QJsonObject& update);
    void handleFile(const QJsonObject& file);
    void markFailed(int fileId);
    void requestDownload(int fileId, int priority);
    void clearSessionState();

    TelegramSession* m_session = nullptr;
    int m_sessionGeneration = 0;
    mutable QReadWriteLock m_pathsLock;
    QHash<int, QString> m_paths;
    QSet<int> m_requested;
    QSet<int> m_failed;
    int m_revision = 0;
};

} // namespace beygram::telegram
