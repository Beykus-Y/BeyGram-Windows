#include "SessionLock.h"

#include <QDir>
#include <QFileInfo>

namespace beygram::telegram {

SessionLock::SessionLock(QString lockFilePath)
    : m_path(std::move(lockFilePath)), m_lock(m_path)
{
    m_lock.setStaleLockTime(30000);
}

Status SessionLock::acquire()
{
    if (m_path.isEmpty())
        return Status::failure(ErrorCode::InvalidArgument,
                               QStringLiteral("Session lock path is empty"));

    QString directoryError;
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return Status::failure(ErrorCode::Io,
                               QStringLiteral("Could not create the session directory"));

    if (m_lock.tryLock(0))
        return Status::success();

    QString holder = QStringLiteral("another process");
    qint64 pid = 0;
    QString hostname;
    QString appname;
    if (m_lock.getLockInfo(&pid, &hostname, &appname)) {
        holder = QStringLiteral("PID %1 (%2)").arg(pid).arg(hostname);
    }

    return Status::failure(
        ErrorCode::Unavailable,
        QStringLiteral("This Telegram account is already open in another BeyGram process (%1). "
                       "Close the other window and retry.")
            .arg(holder));
}

void SessionLock::release()
{
    if (m_lock.isLocked())
        m_lock.unlock();
}

bool SessionLock::isLocked() const
{
    return m_lock.isLocked();
}

QString SessionLock::path() const
{
    return m_path;
}

} // namespace beygram::telegram
