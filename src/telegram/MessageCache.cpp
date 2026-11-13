#include "MessageCache.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace beygram::telegram {
namespace {

constexpr int CacheVersion = 1;

} // namespace

QString messageCacheFilePath(const QString& directory, qint64 chatId)
{
    if (directory.isEmpty() || chatId == 0)
        return {};
    return QDir(directory).filePath(QStringLiteral("chat-%1.json").arg(chatId));
}

CachedMessages loadCachedMessages(const QString& directory, qint64 chatId)
{
    CachedMessages cached;
    const QString path = messageCacheFilePath(directory, chatId);
    if (path.isEmpty())
        return cached;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return cached;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return cached;

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != CacheVersion)
        return cached;

    cached.savedAt = static_cast<qint64>(
        root.value(QStringLiteral("savedAt")).toDouble());
    cached.messages = root.value(QStringLiteral("messages")).toArray();
    return cached;
}

bool saveCachedMessages(const QString& directory, qint64 chatId,
                        const QJsonArray& messages, int maxMessages)
{
    const QString path = messageCacheFilePath(directory, chatId);
    if (path.isEmpty())
        return false;

    if (!QDir().mkpath(directory))
        return false;

    QJsonArray kept;
    const int limit = qBound(0, maxMessages, messages.size());
    for (int index = 0; index < limit; ++index)
        kept.append(messages.at(index));

    QJsonObject root{
        {QStringLiteral("version"), CacheVersion},
        {QStringLiteral("savedAt"),
         static_cast<double>(QDateTime::currentSecsSinceEpoch())},
        {QStringLiteral("messages"), kept},
    };

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

void removeCachedMessages(const QString& directory, qint64 chatId)
{
    const QString path = messageCacheFilePath(directory, chatId);
    if (!path.isEmpty())
        QFile::remove(path);
}

} // namespace beygram::telegram
