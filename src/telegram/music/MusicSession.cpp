#include "MusicSession.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>

#include <utility>

namespace beygram::telegram {
namespace {

constexpr int kMaxRepeatMode = 2;
constexpr int kMaxCrossfadeMs = 12000;

QString stringField(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

int intField(const QJsonObject& object, const QString& key, int fallback = 0)
{
    return object.value(key).toInt(fallback);
}

qint64 integerField(const QJsonObject& object, const QString& key, qint64 fallback = 0)
{
    return object.value(key).toInteger(fallback);
}

QVector<QString> queueFromJson(const QJsonValue& value)
{
    QVector<QString> queue;
    if (!value.isArray())
        return queue;

    const QJsonArray array = value.toArray();
    queue.reserve(array.size());
    for (const QJsonValue& entry : array) {
        if (entry.isString())
            queue.append(entry.toString());
    }
    return queue;
}

/// Applies every storage invariant. Both directions go through this function:
/// decoding normalizes what it read, and encoding normalizes what it is about
/// to write, so a stored document always loads back unchanged.
MusicSessionState normalized(const MusicSessionState& input)
{
    MusicSessionState result = input;
    result.trackId = result.trackId.trimmed();
    result.durationMs = qMax(0, result.durationMs);

    QVector<QString> queue;
    QSet<QString> seen;
    queue.reserve(input.queue.size());
    seen.reserve(input.queue.size());
    for (const QString& rawId : input.queue) {
        const QString id = rawId.trimmed();
        if (id.isEmpty() || seen.contains(id))
            continue;
        seen.insert(id);
        queue.append(id);
    }
    result.queue = std::move(queue);

    if (result.queue.isEmpty()) {
        // Nothing is current without a queue, so a stale track or playhead
        // would only make the restored state lie.
        result.trackId.clear();
        result.index = -1;
        result.positionMs = 0;
        result.playing = false;
    } else {
        result.index = qBound(0, result.index, static_cast<int>(result.queue.size()) - 1);
        result.positionMs = qBound(0, result.positionMs, result.durationMs);
    }

    result.repeatMode = qBound(0, result.repeatMode, kMaxRepeatMode);
    result.crossfadeMs = qBound(0, result.crossfadeMs, kMaxCrossfadeMs);
    result.savedAt = qMax<qint64>(0, result.savedAt);
    return result;
}

MusicSessionState decode(const QJsonObject& root)
{
    MusicSessionState state;
    state.trackId = stringField(root, QStringLiteral("trackId"));
    state.queue = queueFromJson(root.value(QStringLiteral("queue")));
    state.index = intField(root, QStringLiteral("index"), -1);
    state.positionMs = intField(root, QStringLiteral("positionMs"));
    state.durationMs = intField(root, QStringLiteral("durationMs"));
    state.playing = root.value(QStringLiteral("playing")).toBool(false);
    state.shuffle = root.value(QStringLiteral("shuffle")).toBool(false);
    state.repeatMode = intField(root, QStringLiteral("repeatMode"));
    state.crossfadeMs = intField(root, QStringLiteral("crossfadeMs"), state.crossfadeMs);
    state.savedAt = integerField(root, QStringLiteral("savedAt"));
    return normalized(state);
}

QJsonObject encode(const MusicSessionState& state)
{
    const MusicSessionState clean = normalized(state);

    QJsonArray queue;
    for (const QString& trackId : clean.queue)
        queue.append(trackId);

    return QJsonObject{
        {QStringLiteral("version"), MusicSessionStore::kSchemaVersion},
        {QStringLiteral("trackId"), clean.trackId},
        {QStringLiteral("queue"), queue},
        {QStringLiteral("index"), clean.index},
        {QStringLiteral("positionMs"), clean.positionMs},
        {QStringLiteral("durationMs"), clean.durationMs},
        {QStringLiteral("playing"), clean.playing},
        {QStringLiteral("shuffle"), clean.shuffle},
        {QStringLiteral("repeatMode"), clean.repeatMode},
        {QStringLiteral("crossfadeMs"), clean.crossfadeMs},
        {QStringLiteral("savedAt"), static_cast<double>(clean.savedAt)},
    };
}

} // namespace

MusicSessionStore::MusicSessionStore(QString path) : m_path(std::move(path))
{
}

MusicSessionState MusicSessionStore::load() const
{
    if (m_path.isEmpty())
        return {};

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return {};

    const QJsonObject root = document.object();
    // A document written by another schema is left untouched and is not
    // guessed at; the caller can still overwrite it with save().
    if (root.value(QStringLiteral("version")).toInt() != kSchemaVersion)
        return {};

    return decode(root);
}

bool MusicSessionStore::save(const MusicSessionState& state) const
{
    if (m_path.isEmpty())
        return false;

    const QDir parent = QFileInfo(m_path).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return false;

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    const QByteArray payload = QJsonDocument(encode(state)).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

QString MusicSessionStore::path() const
{
    return m_path;
}

MusicSessionState restoreMusicSession(const MusicSessionState& saved, qint64 nowUnix,
                                      int maxAgeSeconds)
{
    const MusicSessionState session = normalized(saved);
    if (session.queue.isEmpty() || session.savedAt <= 0)
        return {};

    // savedAt is non-negative here, so the subtraction cannot overflow once
    // the future case is handled first.
    if (nowUnix < session.savedAt)
        return {};
    if (nowUnix - session.savedAt > qMax(0, maxAgeSeconds))
        return {};

    MusicSessionState result = session;
    if (result.trackId.isEmpty() || !result.queue.contains(result.trackId))
        result.playing = false;
    return result;
}

} // namespace beygram::telegram
