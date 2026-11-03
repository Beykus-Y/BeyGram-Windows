#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
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

    void setSession(TelegramSession* session, bool reconnect = true);

    /// Registers a TDLib file object and requests its download when needed.
    void trackFileObject(const QJsonObject& file, int priority = 16);
    void trackFileId(int fileId, int priority = 16);

    [[nodiscard]] QString path(int fileId) const;
    [[nodiscard]] bool isReady(int fileId) const;
    [[nodiscard]] int revision() const { return m_revision; }

signals:
    void fileReady(int fileId);
    void revisionChanged();

private:
    void handleUpdate(const QJsonObject& update);
    void handleFile(const QJsonObject& file);
    void requestDownload(int fileId, int priority);

    TelegramSession* m_session = nullptr;
    QHash<int, QString> m_paths;
    QSet<int> m_requested;
    int m_revision = 0;
};

} // namespace beygram::telegram
