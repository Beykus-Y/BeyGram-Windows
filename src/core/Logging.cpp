#include "Logging.h"

#include "Redaction.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QtGlobal>

#include <cstdio>
#include <deque>

namespace beygram::logging {
namespace {

constexpr qint64 MaxLogBytes = 5 * 1024 * 1024;
constexpr int MaxRecentMessages = 200;
constexpr int MaxRotatedFiles = 3;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return QStringLiteral("DEBUG");
    case QtInfoMsg: return QStringLiteral("INFO ");
    case QtWarningMsg: return QStringLiteral("WARN ");
    case QtCriticalMsg: return QStringLiteral("ERROR");
    case QtFatalMsg: return QStringLiteral("FATAL");
    }
    return QStringLiteral("LOG  ");
}

class Logger {
public:
    void initialize(const QString& directory, bool verbose)
    {
        QMutexLocker lock(&m_mutex);
        m_verbose = verbose;

        if (!directory.isEmpty() && QDir().mkpath(directory)) {
            m_filePath = QDir(directory).filePath(QStringLiteral("beygram.log"));
            openFileLocked();
        } else {
            m_filePath.clear();
        }

        if (!m_installed) {
            m_previous = qInstallMessageHandler(&Logger::handle);
            m_installed = true;
        }
    }

    void shutdown()
    {
        QMutexLocker lock(&m_mutex);
        closeFileLocked();
        if (m_installed) {
            qInstallMessageHandler(m_previous);
            m_installed = false;
        }
        m_filePath.clear();
    }

    void write(QtMsgType type, const QMessageLogContext& context, const QString& raw)
    {
        if (!m_verbose && type == QtDebugMsg)
            return;

        const QString category = context.category ? QString::fromUtf8(context.category)
                                                  : QStringLiteral("default");
        const QString safe = redact(raw);

        const QString line = QStringLiteral("%1 %2 [%3] %4")
                                 .arg(QDateTime::currentDateTime().toString(
                                          QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
                                      levelName(type), category, safe);

        QMutexLocker lock(&m_mutex);

        m_recent.emplace_back(line);
        while (static_cast<int>(m_recent.size()) > MaxRecentMessages)
            m_recent.pop_front();

        if (m_file.isOpen()) {
            rotateIfNeededLocked();
            m_file.write(line.toUtf8());
            m_file.write("\n");
            m_file.flush();
        } else {
            std::fputs(line.toUtf8().constData(), stderr);
            std::fputc('\n', stderr);
        }

        if (type == QtFatalMsg) {
            closeFileLocked();
        }
    }

    QStringList recent() const
    {
        QMutexLocker lock(&m_mutex);
        QStringList result;
        result.reserve(static_cast<int>(m_recent.size()));
        for (const QString& line : m_recent)
            result.append(line);
        return result;
    }

    QString filePath() const
    {
        QMutexLocker lock(&m_mutex);
        return m_filePath;
    }

    bool verbose() const
    {
        QMutexLocker lock(&m_mutex);
        return m_verbose;
    }

    static void handle(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        instance().write(type, context, message);
    }

    static Logger& instance()
    {
        // Intentionally never destroyed: Qt may log during static destruction,
        // and a missing logger would fall back to an unredacted handler.
        static Logger* logger = new Logger();
        return *logger;
    }

private:
    Logger() = default;

    void openFileLocked()
    {
        closeFileLocked();
        m_file.setFileName(m_filePath);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            m_filePath.clear();
            m_file.setFileName(QString());
        }
    }

    void closeFileLocked()
    {
        if (m_file.isOpen()) {
            m_file.flush();
            m_file.close();
        }
    }

    void rotateIfNeededLocked()
    {
        if (m_file.size() < MaxLogBytes)
            return;

        const QString base = m_filePath;
        closeFileLocked();

        for (int index = MaxRotatedFiles - 1; index >= 1; --index) {
            const QString from = QStringLiteral("%1.%2").arg(base).arg(index);
            const QString to = QStringLiteral("%1.%2").arg(base).arg(index + 1);
            if (QFile::exists(from)) {
                QFile::remove(to);
                QFile::rename(from, to);
            }
        }
        QFile::remove(base + QStringLiteral(".1"));
        QFile::rename(base, base + QStringLiteral(".1"));
        openFileLocked();
    }

    mutable QMutex m_mutex;
    QFile m_file;
    QString m_filePath;
    std::deque<QString> m_recent;
    QtMessageHandler m_previous = nullptr;
    bool m_installed = false;
    bool m_verbose = false;
};

} // namespace

void initialize(const QString& logDirectory, bool verbose)
{
    Logger::instance().initialize(logDirectory, verbose);
}

void shutdown()
{
    Logger::instance().shutdown();
}

QStringList recentMessages()
{
    return Logger::instance().recent();
}

QString logFilePath()
{
    return Logger::instance().filePath();
}

bool isVerbose()
{
    return Logger::instance().verbose();
}

} // namespace beygram::logging
