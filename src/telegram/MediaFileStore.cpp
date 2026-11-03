#include "MediaFileStore.h"

#include "TelegramSession.h"

#include <QJsonValue>
#include <QLoggingCategory>

namespace beygram::telegram {
namespace {
Q_LOGGING_CATEGORY(lcMedia, "beygram.telegram.media")
constexpr int MaxPriority = 32;

int clampPriority(int priority)
{
    return qBound(1, priority, MaxPriority);
}
} // namespace

MediaFileStore::MediaFileStore(QObject* parent) : QObject(parent)
{
}

void MediaFileStore::setSession(TelegramSession* session, bool reconnect)
{
    if (m_session == session && reconnect)
        return;
    if (m_session)
        disconnect(m_session, &TelegramSession::updateReceived, this,
                   &MediaFileStore::handleUpdate);
    m_session = session;
    if (m_session) {
        connect(m_session, &TelegramSession::updateReceived, this,
                &MediaFileStore::handleUpdate);
    }
}

void MediaFileStore::trackFileObject(const QJsonObject& file, int priority)
{
    const int fileId = file.value(QStringLiteral("id")).toInt();
    if (fileId == 0)
        return;

    const QJsonObject local = file.value(QStringLiteral("local")).toObject();
    const QString path = local.value(QStringLiteral("path")).toString();
    const bool complete = local.value(QStringLiteral("is_downloading_completed")).toBool();
    if (complete && !path.isEmpty()) {
        bool changed = false;
        {
            QWriteLocker locker(&m_pathsLock);
            if (m_paths.value(fileId) != path) {
                m_paths.insert(fileId, path);
                ++m_revision;
                changed = true;
            }
        }
        if (changed) {
            emit fileReady(fileId);
            emit revisionChanged();
        }
        return;
    }

    requestDownload(fileId, priority);
}

void MediaFileStore::trackFileId(int fileId, int priority)
{
    if (fileId == 0)
        return;
    {
        QReadLocker locker(&m_pathsLock);
        if (m_paths.contains(fileId))
            return;
    }
    requestDownload(fileId, priority);
}

QString MediaFileStore::path(int fileId) const
{
    QReadLocker locker(&m_pathsLock);
    return m_paths.value(fileId);
}

bool MediaFileStore::isReady(int fileId) const
{
    QReadLocker locker(&m_pathsLock);
    return m_paths.contains(fileId);
}

void MediaFileStore::handleUpdate(const QJsonObject& update)
{
    if (update.value(QStringLiteral("@type")).toString() == QLatin1String("updateFile"))
        handleFile(update.value(QStringLiteral("file")).toObject());
}

void MediaFileStore::handleFile(const QJsonObject& file)
{
    const int fileId = file.value(QStringLiteral("id")).toInt();
    if (fileId == 0)
        return;

    const QJsonObject local = file.value(QStringLiteral("local")).toObject();
    if (!local.value(QStringLiteral("is_downloading_completed")).toBool())
        return;

    const QString path = local.value(QStringLiteral("path")).toString();
    if (path.isEmpty())
        return;

    m_requested.remove(fileId);
    bool changed = false;
    {
        QWriteLocker locker(&m_pathsLock);
        if (m_paths.value(fileId) == path)
            return;
        m_paths.insert(fileId, path);
        ++m_revision;
        changed = true;
    }
    if (!changed)
        return;

    qCDebug(lcMedia) << "file ready" << fileId;
    emit fileReady(fileId);
    emit revisionChanged();
}

void MediaFileStore::requestDownload(int fileId, int priority)
{
    if (fileId == 0 || !m_session || !m_session->isRunning())
        return;
    {
        QReadLocker locker(&m_pathsLock);
        if (m_paths.contains(fileId))
            return;
    }
    if (m_requested.contains(fileId))
        return;

    m_requested.insert(fileId);
    qCDebug(lcMedia) << "download requested" << fileId;
    m_session->sendRequest(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("downloadFile")},
                    {QStringLiteral("file_id"), fileId},
                    {QStringLiteral("priority"), clampPriority(priority)},
                    {QStringLiteral("offset"), 0},
                    {QStringLiteral("limit"), 0},
                    {QStringLiteral("synchronous"), false}},
        [this, fileId](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString() == QLatin1String("file")) {
                handleFile(response);
            } else {
                m_requested.remove(fileId);
            }
        },
        120000);
}

} // namespace beygram::telegram
