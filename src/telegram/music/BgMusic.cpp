#include "BgMusic.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSet>
#include <QStringList>

#include <cmath>
#include <initializer_list>
#include <limits>

namespace beygram::telegram {
namespace {

constexpr int kJsonVersion = 1;

QJsonValue firstValue(const QJsonObject& object, std::initializer_list<QString> keys)
{
    for (const QString& key : keys) {
        const auto it = object.constFind(key);
        if (it != object.constEnd())
            return it.value();
    }
    return {};
}

QString stringValue(const QJsonValue& value)
{
    return value.isString() ? value.toString() : QString();
}

// JSON stores every number as a double. Values outside the qint64 range are
// saturated instead of cast, because casting an out-of-range double is UB.
qint64 integerValue(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (!std::isfinite(number))
            return 0;
        if (number >= static_cast<double>(std::numeric_limits<qint64>::max()))
            return std::numeric_limits<qint64>::max();
        if (number <= static_cast<double>(std::numeric_limits<qint64>::min()))
            return std::numeric_limits<qint64>::min();
        return static_cast<qint64>(number);
    }
    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().trimmed().toLongLong(&ok);
        return ok ? parsed : 0;
    }
    return 0;
}

int nonNegativeInt(qint64 value)
{
    if (value <= 0)
        return 0;
    if (value >= std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(value);
}

bool entryFromJson(const QJsonValue& value, BgMusicEntry& entry)
{
    if (!value.isObject())
        return false;

    const QJsonObject object = value.toObject();
    const QString trackId = stringValue(firstValue(object,
        { QStringLiteral("trackId"), QStringLiteral("track_id"), QStringLiteral("id") }));
    if (trackId.trimmed().isEmpty())
        return false;

    entry.trackId = trackId;
    entry.title = stringValue(firstValue(object,
        { QStringLiteral("title"), QStringLiteral("name") }));
    entry.artist = stringValue(firstValue(object,
        { QStringLiteral("artist"), QStringLiteral("performer") }));
    entry.durationSeconds = nonNegativeInt(integerValue(firstValue(object,
        { QStringLiteral("durationSeconds"), QStringLiteral("duration_sec"),
          QStringLiteral("duration_seconds"), QStringLiteral("duration") })));
    entry.startMs = nonNegativeInt(integerValue(firstValue(object,
        { QStringLiteral("startMs"), QStringLiteral("start_ms"), QStringLiteral("start") })));
    return true;
}

QString singleLine(QString text)
{
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return text;
}

int m3uValue(const QString& text)
{
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    return ok ? nonNegativeInt(value) : 0;
}

} // namespace

QString bgMusicToJson(const BgMusicPlaylist& playlist)
{
    QJsonArray entries;
    QSet<QString> seen;
    for (const BgMusicEntry& entry : playlist.entries) {
        if (entry.trackId.trimmed().isEmpty())
            continue;
        if (seen.contains(entry.trackId))
            continue;
        seen.insert(entry.trackId);

        entries.append(QJsonObject{
            { QStringLiteral("trackId"), entry.trackId },
            { QStringLiteral("title"), entry.title },
            { QStringLiteral("artist"), entry.artist },
            { QStringLiteral("durationSeconds"), nonNegativeInt(entry.durationSeconds) },
            { QStringLiteral("startMs"), nonNegativeInt(entry.startMs) },
        });
    }

    const QJsonObject root{
        { QStringLiteral("version"), kJsonVersion },
        { QStringLiteral("id"), playlist.id },
        { QStringLiteral("title"), playlist.title },
        { QStringLiteral("updatedAt"), playlist.updatedAt },
        { QStringLiteral("entries"), entries },
    };
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

BgMusicPlaylist bgMusicFromJson(const QString& json)
{
    BgMusicPlaylist playlist;

    QString text = json;
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF))
        text.remove(0, 1);

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return playlist;

    const QJsonObject root = document.object();

    // A missing version is treated as the current one, so hand-written files
    // that carry only the fields stay readable. A future version is refused
    // rather than guessed at.
    const QJsonValue versionValue = firstValue(root,
        { QStringLiteral("version"), QStringLiteral("formatVersion"),
          QStringLiteral("format_version"), QStringLiteral("schemaVersion"),
          QStringLiteral("schema_version") });
    if (!versionValue.isUndefined() && !versionValue.isNull()) {
        if (integerValue(versionValue) != kJsonVersion)
            return playlist;
    }

    playlist.id = stringValue(firstValue(root,
        { QStringLiteral("id"), QStringLiteral("playlistId"), QStringLiteral("playlist_id") }));
    playlist.title = stringValue(firstValue(root,
        { QStringLiteral("title"), QStringLiteral("name") }));
    playlist.updatedAt = integerValue(firstValue(root,
        { QStringLiteral("updatedAt"), QStringLiteral("updated_at") }));

    QJsonValue entriesValue = root.value(QStringLiteral("entries"));
    if (!entriesValue.isArray())
        entriesValue = root.value(QStringLiteral("tracks"));
    if (!entriesValue.isArray())
        return playlist;

    const QJsonArray entries = entriesValue.toArray();
    QSet<QString> seen;
    for (const QJsonValue& value : entries) {
        BgMusicEntry entry;
        if (!entryFromJson(value, entry))
            continue;
        if (seen.contains(entry.trackId))
            continue;
        seen.insert(entry.trackId);
        playlist.entries.append(entry);
    }
    return playlist;
}

QString bgMusicToM3u(const BgMusicPlaylist& playlist)
{
    QStringList lines;
    lines.append(QStringLiteral("#EXTM3U"));

    QSet<QString> seen;
    for (const BgMusicEntry& entry : playlist.entries) {
        if (entry.trackId.trimmed().isEmpty())
            continue;
        if (seen.contains(entry.trackId))
            continue;
        seen.insert(entry.trackId);

        QString display = singleLine(entry.title);
        const QString artist = singleLine(entry.artist);
        if (!artist.isEmpty())
            display += QStringLiteral(" - ") + artist;

        lines.append(QStringLiteral("#EXTINF:%1,%2")
                         .arg(nonNegativeInt(entry.durationSeconds))
                         .arg(display));
        if (entry.startMs > 0)
            lines.append(QStringLiteral("#EXTBGSTART:%1").arg(entry.startMs));
        lines.append(entry.trackId);
    }

    return lines.join(QLatin1Char('\n'));
}

BgMusicPlaylist bgMusicFromM3u(const QString& m3u)
{
    BgMusicPlaylist playlist;

    QString text = m3u;
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF))
        text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    int pendingDuration = 0;
    int pendingStart = 0;
    QString pendingTitle;
    QString pendingArtist;
    bool hasExtinf = false;
    qsizetype lastEntryIndex = -1;
    QSet<QString> seen;

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;

        if (trimmed.startsWith(QLatin1Char('#'))) {
            const QString upper = trimmed.toUpper();
            if (upper.startsWith(QStringLiteral("#EXTINF:"))) {
                const QString payload =
                    trimmed.mid(QStringLiteral("#EXTINF:").size());
                const qsizetype comma = payload.indexOf(QLatin1Char(','));
                const QString durationText =
                    (comma >= 0 ? payload.left(comma) : payload).trimmed();
                const QString display =
                    comma >= 0 ? payload.mid(comma + 1) : QString();

                pendingDuration = m3uValue(durationText);
                const qsizetype separator =
                    display.lastIndexOf(QStringLiteral(" - "));
                if (separator >= 0) {
                    pendingTitle = display.left(separator).trimmed();
                    pendingArtist = display.mid(separator + 3).trimmed();
                } else {
                    pendingTitle = display.trimmed();
                    pendingArtist.clear();
                }
                // A #EXTBGSTART may precede the #EXTINF line as well, so an
                // already pending start offset is kept.
                hasExtinf = true;
                lastEntryIndex = -1;
                continue;
            }

            if (upper.startsWith(QStringLiteral("#EXTBGSTART:"))) {
                const QString payload =
                    trimmed.mid(QStringLiteral("#EXTBGSTART:").size());
                bool ok = false;
                const qint64 rawStart = payload.trimmed().toLongLong(&ok);
                if (!ok)
                    continue;
                const int start = nonNegativeInt(rawStart);
                if (!hasExtinf && lastEntryIndex >= 0
                    && lastEntryIndex < playlist.entries.size()) {
                    // The tag may follow its track id instead of preceding it.
                    playlist.entries[lastEntryIndex].startMs = start;
                    pendingStart = 0;
                } else {
                    pendingStart = start;
                }
                continue;
            }

            // #EXTM3U and every unknown directive are ignored.
            continue;
        }

        const QString trackId = trimmed;
        if (!trackId.isEmpty() && !seen.contains(trackId)) {
            BgMusicEntry entry;
            entry.trackId = trackId;
            entry.title = pendingTitle;
            entry.artist = pendingArtist;
            entry.durationSeconds = pendingDuration;
            entry.startMs = pendingStart;
            seen.insert(trackId);
            playlist.entries.append(entry);
            lastEntryIndex = playlist.entries.size() - 1;
        } else {
            lastEntryIndex = -1;
        }

        pendingDuration = 0;
        pendingStart = 0;
        pendingTitle.clear();
        pendingArtist.clear();
        hasExtinf = false;
    }

    return playlist;
}

} // namespace beygram::telegram
