#include "MediaViewerController.h"

#include "telegram/MediaFileStore.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace beygram::ui {
namespace {
constexpr qint64 CopyChunkSize = 256 * 1024;

QString mediaUrl(int fileId, int revision)
{
    return QStringLiteral("image://beygrammedia/%1?v=%2").arg(fileId).arg(revision);
}
} // namespace

MediaViewerController::MediaViewerController(telegram::MediaFileStore* store, QObject* parent)
    : QObject(parent), m_store(store)
{
    if (!m_store)
        return;

    connect(m_store, &telegram::MediaFileStore::fileReady, this,
            &MediaViewerController::handleFileReady);
    connect(m_store, &telegram::MediaFileStore::revisionChanged, this, [this] {
        const QString previousSource = m_source;
        const bool wasLoading = loading();
        updateSource();
        if (m_source != previousSource || loading() != wasLoading)
            emit stateChanged();
    });
}

bool MediaViewerController::isOpen() const
{
    return m_open;
}

int MediaViewerController::fileId() const
{
    return m_fileId;
}

QString MediaViewerController::title() const
{
    return m_title;
}

QString MediaViewerController::source() const
{
    return m_source;
}

bool MediaViewerController::loading() const
{
    return m_open && m_fileId > 0 && m_store && !m_store->isReady(m_fileId);
}

QString MediaViewerController::error() const
{
    return m_error;
}

void MediaViewerController::open(int fileId, const QString& title)
{
    if (!m_store) {
        m_open = false;
        m_fileId = 0;
        m_title.clear();
        m_source.clear();
        m_error = QStringLiteral("Media store is unavailable");
        emit stateChanged();
        return;
    }

    if (fileId <= 0) {
        m_open = false;
        m_fileId = 0;
        m_title.clear();
        m_source.clear();
        m_error = QStringLiteral("No media file");
        emit stateChanged();
        return;
    }

    m_open = true;
    m_fileId = fileId;
    m_title = title;
    m_error.clear();

    // Ask the store to prioritise this file; it is a no-op when already cached
    // or when no session is running.
    m_store->trackFileId(fileId, 32);

    m_revision = m_store->revision();
    m_source = m_store->isReady(fileId) ? mediaUrl(fileId, m_revision) : QString();
    emit stateChanged();
}

void MediaViewerController::close()
{
    m_open = false;
    m_fileId = 0;
    m_title.clear();
    m_source.clear();
    m_error.clear();
    emit stateChanged();
}

bool MediaViewerController::saveAs(const QString& targetPath)
{
    if (!m_open || m_fileId <= 0 || !m_store) {
        m_error = QStringLiteral("No media file is open");
        emit stateChanged();
        return false;
    }

    if (!m_store->isReady(m_fileId)) {
        m_error = QStringLiteral("The media file is still downloading");
        emit stateChanged();
        return false;
    }

    const QString sourcePath = m_store->path(m_fileId);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        m_error = QStringLiteral("The media file is unavailable");
        emit stateChanged();
        return false;
    }

    if (targetPath.trimmed().isEmpty()) {
        m_error = QStringLiteral("Choose a destination file");
        emit stateChanged();
        return false;
    }

    // Never write over the cached TDLib file while reading from it.
    const QFileInfo sourceInfo(sourcePath);
    const QFileInfo targetInfo(targetPath);
    const QString sourceCanonical = sourceInfo.canonicalFilePath();
    const QString targetCanonical = targetInfo.canonicalFilePath();
    if (targetInfo.absoluteFilePath() == sourceInfo.absoluteFilePath()
        || (!targetCanonical.isEmpty() && targetCanonical == sourceCanonical)) {
        m_error = QStringLiteral("Choose a different destination file");
        emit stateChanged();
        return false;
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Could not read the media file");
        emit stateChanged();
        return false;
    }

    QSaveFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly)) {
        m_error = QStringLiteral("Could not create the destination file");
        emit stateChanged();
        return false;
    }

    bool copied = true;
    while (copied && !source.atEnd()) {
        const QByteArray chunk = source.read(CopyChunkSize);
        if (chunk.isEmpty()) {
            copied = source.error() == QFileDevice::NoError;
            break;
        }
        if (target.write(chunk) != chunk.size())
            copied = false;
    }
    if (copied && source.error() != QFileDevice::NoError)
        copied = false;
    if (copied && !target.commit())
        copied = false;

    if (!copied) {
        target.cancelWriting();
        m_error = QStringLiteral("Could not save the media file");
        emit stateChanged();
        return false;
    }

    m_error.clear();
    emit stateChanged();
    return true;
}

void MediaViewerController::updateSource()
{
    if (!m_store || !m_open || m_fileId <= 0 || !m_store->isReady(m_fileId)) {
        m_source.clear();
        return;
    }

    // The revision only cache-busts the URL when the open file becomes
    // available. Revisions bumped by other files must not reload the viewer.
    if (!m_source.isEmpty())
        return;

    m_revision = m_store->revision();
    m_source = mediaUrl(m_fileId, m_revision);
}

void MediaViewerController::handleFileReady(int fileId)
{
    if (!m_open || fileId != m_fileId)
        return;

    const QString previousSource = m_source;
    const bool wasLoading = loading();
    updateSource();
    if (m_source != previousSource || loading() != wasLoading)
        emit stateChanged();
}

} // namespace beygram::ui
