#include "MusicWaves.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>

namespace beygram::telegram {
namespace {

constexpr int kWaveTracksMin = 1;
constexpr int kWaveTracksMax = 100;
constexpr qsizetype kMaxResolutionObjects = 24;

// A JSON string, or a number written as text. Booleans, nulls, objects and
// arrays are not identifiers or titles, so they read as empty.
QString scalarText(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (!value.isDouble())
        return {};

    const double number = value.toDouble();
    if (!std::isfinite(number))
        return {};
    if (std::floor(number) == number && std::abs(number) <= 9007199254740992.0)
        return QString::number(static_cast<qint64>(number));
    return QString::number(number, 'g', 15);
}

// The first non-empty value among \a keys, trimmed. An empty field is skipped
// so a later alias can still supply the value.
QString firstText(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QString text = scalarText(object.value(QLatin1String(key))).trimmed();
        if (!text.isEmpty())
            return text;
    }
    return {};
}

// A provider id is always a string; a number or an object in that field is
// malformed and must not become an id.
QString firstString(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (!value.isString())
            continue;
        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            return text;
    }
    return {};
}

// A cover field may be a plain address, an object with a url/uri, or an array
// of addresses. Only strings are read: a numeric field is an id, not an
// address, and must not become a fake artwork URL.
QString textValue(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().trimmed();
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (const char* key : {"url", "uri", "href", "src"}) {
            const QJsonValue nested = object.value(QLatin1String(key));
            if (!nested.isString())
                continue;
            const QString text = nested.toString().trimmed();
            if (!text.isEmpty())
                return text;
        }
        return {};
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array) {
            if (!item.isString())
                continue;
            const QString text = item.toString().trimmed();
            if (!text.isEmpty())
                return text;
        }
    }
    return {};
}

QString artworkUrlFrom(const QJsonObject& object)
{
    for (const char* key : {"artworkUrl", "artwork_url", "imageUrl", "image_url",
                            "coverUrl", "cover_url", "picture", "icon", "image",
                            "cover", "artwork"}) {
        const QString text = textValue(object.value(QLatin1String(key)));
        if (!text.isEmpty())
            return text;
    }

    // Yandex coverUri has no scheme and uses "%%" where a size belongs.
    QString coverUri = textValue(object.value(QStringLiteral("coverUri")));
    if (coverUri.isEmpty())
        coverUri = textValue(object.value(QStringLiteral("cover_uri")));
    if (coverUri.isEmpty())
        return {};
    QString url = coverUri;
    if (url.startsWith(QLatin1String("//")))
        url.prepend(QStringLiteral("https:"));
    else if (!url.contains(QLatin1String("://")))
        url.prepend(QStringLiteral("https://"));
    url.replace(QStringLiteral("%%"), QStringLiteral("200x200"));
    return url;
}

QString canonicalProvider(const QString& provider, const QString& fallback)
{
    const QString key = provider.trimmed().toLower();
    if (key.isEmpty())
        return fallback;
    if (key == QLatin1String("yandex") || key == QLatin1String("yandex_music")
        || key == QLatin1String("yandexmusic") || key == QLatin1String("ym")) {
        return QStringLiteral("yandex");
    }
    if (key == QLatin1String("spotify"))
        return QStringLiteral("spotify");
    if (key == QLatin1String("apple") || key == QLatin1String("apple_music")
        || key == QLatin1String("applemusic") || key == QLatin1String("itunes")) {
        return QStringLiteral("apple");
    }
    if (key == QLatin1String("soundcloud") || key == QLatin1String("sound_cloud"))
        return QStringLiteral("soundcloud");
    if (key == QLatin1String("youtube") || key == QLatin1String("youtube_music")
        || key == QLatin1String("youtubemusic") || key == QLatin1String("ytmusic")
        || key == QLatin1String("ytm")) {
        return QStringLiteral("youtube");
    }
    return key;
}

bool findWaveArray(const QJsonObject& holder, QJsonArray& out)
{
    for (const char* key : {"waves", "items", "results", "stations"}) {
        const QJsonValue value = holder.value(QLatin1String(key));
        if (value.isArray()) {
            out = value.toArray();
            return true;
        }
        if (!value.isObject())
            continue;
        const QJsonObject nested = value.toObject();
        for (const char* inner : {"items", "waves", "results", "stations"}) {
            const QJsonValue innerValue = nested.value(QLatin1String(inner));
            if (innerValue.isArray()) {
                out = innerValue.toArray();
                return true;
            }
        }
    }
    return false;
}

QJsonArray waveArray(const QJsonObject& response)
{
    QJsonArray array;
    const QJsonValue result = response.value(QStringLiteral("result"));
    if (result.isArray())
        return result.toArray();
    if (result.isObject()) {
        const QJsonObject resultObject = result.toObject();
        if (findWaveArray(resultObject, array))
            return array;
        for (const char* key : {"data", "payload"}) {
            const QJsonValue nested = resultObject.value(QLatin1String(key));
            if (nested.isObject() && findWaveArray(nested.toObject(), array))
                return array;
        }
    }
    if (findWaveArray(response, array))
        return array;
    for (const char* key : {"data", "payload"}) {
        const QJsonValue nested = response.value(QLatin1String(key));
        if (nested.isObject() && findWaveArray(nested.toObject(), array))
            return array;
    }
    return {};
}

// A list entry may wrap the wave in an object named after it, as other music
// responses do with "track", "album" and "playlist".
QJsonObject unwrappedWave(const QJsonObject& entry)
{
    for (const char* key : {"wave", "station"}) {
        const QJsonValue value = entry.value(QLatin1String(key));
        if (value.isObject())
            return value.toObject();
    }
    return entry;
}

void appendResolutionObject(QVector<QJsonObject>& queue, const QJsonValue& value)
{
    if (value.isObject()) {
        queue.append(value.toObject());
        return;
    }
    if (!value.isArray())
        return;
    const QJsonArray array = value.toArray();
    for (const QJsonValue& item : array) {
        if (item.isObject()) {
            queue.append(item.toObject());
            break;
        }
    }
}

// The response and the objects that commonly carry the track and its audio.
// Breadth-first, so a field on the response itself wins over the same field in
// a nested wrapper, and bounded, so a hostile response cannot grow the walk.
QVector<QJsonObject> resolutionObjects(const QJsonObject& response)
{
    static constexpr const char* kWrappers[] = {
        "result", "data", "payload", "response", "track", "audio", "downloader",
        "download", "downloadInfo", "download_info", "info", "file_info", "fileInfo",
        "stream", "source", "item",
    };

    QVector<QJsonObject> objects;
    QVector<QJsonObject> queue;
    queue.append(response);
    for (qsizetype index = 0; index < queue.size() && objects.size() < kMaxResolutionObjects; ++index) {
        const QJsonObject current = queue.at(index);
        objects.append(current);
        for (const char* key : kWrappers)
            appendResolutionObject(queue, current.value(QLatin1String(key)));
    }
    return objects;
}

QString firstResolutionText(const QVector<QJsonObject>& objects,
                            std::initializer_list<const char*> keys)
{
    for (const QJsonObject& object : objects) {
        const QString text = firstText(object, keys);
        if (!text.isEmpty())
            return text;
    }
    return {};
}

QString firstResolutionString(const QVector<QJsonObject>& objects,
                              std::initializer_list<const char*> keys)
{
    for (const QJsonObject& object : objects) {
        const QString text = firstString(object, keys);
        if (!text.isEmpty())
            return text;
    }
    return {};
}

// A URL field may itself be an object, such as "file": {"url": "..."}, or a
// list of mirrors. Every string is collected so an https mirror can still win
// over an earlier http one.
void addUrlCandidate(QVector<QString>& candidates, QSet<QString>& seen, const QString& value)
{
    const QString text = value.trimmed();
    if (text.isEmpty() || seen.contains(text))
        return;
    seen.insert(text);
    candidates.append(text);
}

void appendUrlTexts(const QJsonValue& value, QVector<QString>& candidates, QSet<QString>& seen)
{
    if (value.isString()) {
        addUrlCandidate(candidates, seen, value.toString());
        return;
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array) {
            if (item.isString())
                addUrlCandidate(candidates, seen, item.toString());
        }
        return;
    }
    if (!value.isObject())
        return;

    const QJsonObject object = value.toObject();
    for (const char* key : {"url", "uri", "href", "src", "download_url", "stream_url", "direct_link"}) {
        const QJsonValue nested = object.value(QLatin1String(key));
        if (nested.isString())
            addUrlCandidate(candidates, seen, nested.toString());
    }
}

QVector<QString> urlCandidates(const QVector<QJsonObject>& objects)
{
    static constexpr const char* kUrlKeys[] = {
        "stream_url", "url", "download_url", "file", "direct_link",
        "streamUrl", "downloadUrl", "fileUrl", "file_url", "directLink", "src",
    };

    QVector<QString> candidates;
    QSet<QString> seen;
    for (const char* key : kUrlKeys) {
        for (const QJsonObject& object : objects)
            appendUrlTexts(object.value(QLatin1String(key)), candidates, seen);
    }
    return candidates;
}

bool isWebUrl(const QString& value, bool httpsOnly)
{
    const QUrl url(value);
    if (!url.isValid() || url.host().isEmpty())
        return false;
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("https"))
        return true;
    return !httpsOnly && scheme == QLatin1String("http");
}

// https anywhere in the candidate list beats an earlier http field. When only
// http is available it is still reported, so a caller can show why it is not
// playable, but playable stays false.
QString selectStreamUrl(const QVector<QString>& candidates, bool& playable)
{
    playable = false;
    for (const QString& candidate : candidates) {
        if (isWebUrl(candidate, true)) {
            playable = true;
            return candidate;
        }
    }
    for (const QString& candidate : candidates) {
        if (isWebUrl(candidate, false))
            return candidate;
    }
    return {};
}

std::optional<double> numericValue(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (!std::isfinite(number))
            return std::nullopt;
        return number;
    }
    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().trimmed().toDouble(&ok);
        if (ok && std::isfinite(number))
            return number;
    }
    return std::nullopt;
}

std::optional<double> firstNumber(const QVector<QJsonObject>& objects,
                                  std::initializer_list<const char*> keys)
{
    for (const QJsonObject& object : objects) {
        for (const char* key : keys) {
            const std::optional<double> number = numericValue(object.value(QLatin1String(key)));
            if (number)
                return number;
        }
    }
    return std::nullopt;
}

int clampSeconds(double seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0)
        return 0;
    const double maximum = static_cast<double>(std::numeric_limits<int>::max());
    if (seconds >= maximum)
        return std::numeric_limits<int>::max();
    return static_cast<int>(seconds);
}

int durationSecondsFrom(const QVector<QJsonObject>& objects)
{
    if (const std::optional<double> seconds =
            firstNumber(objects, {"durationSeconds", "duration_seconds"})) {
        return clampSeconds(*seconds);
    }
    if (const std::optional<double> milliseconds =
            firstNumber(objects, {"durationMs", "duration_ms", "durationMilliseconds",
                                  "duration_milliseconds"})) {
        return clampSeconds(*milliseconds / 1000.0);
    }
    // A bare "duration" is read as seconds, the unit of ResolvedTrack.
    if (const std::optional<double> seconds = firstNumber(objects, {"duration"}))
        return clampSeconds(*seconds);
    return 0;
}

ResolvedTrack resolvedTrackFrom(const QJsonObject& response, const QString& fallbackProvider)
{
    const QVector<QJsonObject> objects = resolutionObjects(response);

    ResolvedTrack track;
    track.trackId = firstResolutionText(objects, {"trackId", "track_id"});
    if (track.trackId.isEmpty())
        track.trackId = firstResolutionText(objects, {"id"});
    track.provider = canonicalProvider(firstResolutionString(objects, {"provider", "service"}),
                                       fallbackProvider);

    bool playable = false;
    track.streamUrl = selectStreamUrl(urlCandidates(objects), playable);
    track.playable = playable;
    track.durationSeconds = durationSecondsFrom(objects);
    return track;
}

} // namespace

QVector<MusicWave> musicWavesFromJson(const QJsonObject& response)
{
    const QJsonArray array = waveArray(response);
    if (array.isEmpty())
        return {};

    const QJsonObject resultObject = response.value(QStringLiteral("result")).toObject();
    QString fallbackProvider = firstString(response, {"provider", "service"});
    if (fallbackProvider.isEmpty())
        fallbackProvider = firstString(resultObject, {"provider", "service"});
    fallbackProvider = canonicalProvider(fallbackProvider, QStringLiteral("yandex"));

    QVector<MusicWave> waves;
    waves.reserve(array.size());
    QSet<QString> seenIds;
    for (const QJsonValue& value : array) {
        if (!value.isObject())
            continue;

        const QJsonObject object = unwrappedWave(value.toObject());
        MusicWave wave;
        wave.id = firstText(object, {"id", "waveId", "wave_id", "stationId", "station_id", "slug"});
        if (wave.id.isEmpty() || seenIds.contains(wave.id))
            continue;
        seenIds.insert(wave.id);

        wave.title = firstText(object, {"title", "name", "waveTitle", "wave_title"});
        wave.subtitle = firstText(object, {"subtitle", "subTitle", "sub_title", "description",
                                           "tagline"});
        wave.artworkUrl = artworkUrlFrom(object);
        wave.provider = canonicalProvider(firstString(object, {"provider", "service"}),
                                          fallbackProvider);
        waves.append(wave);
    }
    return waves;
}

QJsonObject musicWaveTracksRequest(const QString& waveId, int limit)
{
    const QString id = waveId.trimmed();
    if (id.isEmpty())
        return {};

    const int bounded = std::clamp(limit, kWaveTracksMin, kWaveTracksMax);
    return QJsonObject{
        {QStringLiteral("waveId"), id},
        {QStringLiteral("limit"), static_cast<double>(bounded)},
    };
}

ResolvedTrack resolveYandexTrack(const QJsonObject& response)
{
    return resolvedTrackFrom(response, QStringLiteral("yandex"));
}

ResolvedTrack resolveLazyTrack(const QJsonObject& response)
{
    return resolvedTrackFrom(response, QString());
}

} // namespace beygram::telegram
