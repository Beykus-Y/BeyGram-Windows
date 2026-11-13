#include "MusicProviders.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <optional>

namespace beygram::telegram {
namespace {

QString canonicalProviderId(const QString& provider)
{
    const QString key = provider.trimmed().toLower();
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
    return {};
}

ProviderLink makeLink(const QString& provider, const QString& kind, const QString& id)
{
    return ProviderLink{provider, kind, id};
}

// ---------------------------------------------------------------------------
// Link parsing
// ---------------------------------------------------------------------------

bool isAsciiLetterOrDigit(QChar character)
{
    const ushort code = character.unicode();
    return (code >= 'a' && code <= 'z')
        || (code >= 'A' && code <= 'Z')
        || (code >= '0' && code <= '9');
}

bool isHexDigit(QChar character)
{
    const ushort code = character.unicode();
    return (code >= '0' && code <= '9')
        || (code >= 'a' && code <= 'f')
        || (code >= 'A' && code <= 'F');
}

bool isNumericId(const QString& value)
{
    if (value.isEmpty() || value.size() > 20)
        return false;
    for (const QChar character : value) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    return true;
}

bool isSpotifyId(const QString& value)
{
    if (value.isEmpty() || value.size() > 64)
        return false;
    for (const QChar character : value) {
        if (!isAsciiLetterOrDigit(character))
            return false;
    }
    return true;
}

bool isYoutubeId(const QString& value)
{
    if (value.isEmpty() || value.size() > 64)
        return false;
    for (const QChar character : value) {
        if (!isAsciiLetterOrDigit(character) && character != QLatin1Char('_')
            && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

bool isApplePlaylistId(const QString& value)
{
    if (value.isEmpty() || value.size() > 128)
        return false;
    for (const QChar character : value) {
        if (!isAsciiLetterOrDigit(character) && character != QLatin1Char('.')
            && character != QLatin1Char('_') && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

bool isSoundCloudSegment(const QString& value)
{
    if (value.isEmpty() || value.size() > 128)
        return false;
    if (value == QLatin1String(".") || value == QLatin1String(".."))
        return false;
    for (const QChar character : value) {
        if (!isAsciiLetterOrDigit(character) && character != QLatin1Char('.')
            && character != QLatin1Char('_') && character != QLatin1Char('~')
            && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

// Yandex Music playlist ids are uuids, optionally prefixed with "lk.".
bool isYandexPlaylistId(const QString& value)
{
    QString uuid = value;
    if (uuid.startsWith(QStringLiteral("lk."), Qt::CaseInsensitive))
        uuid = uuid.mid(3);
    if (uuid.size() != 36)
        return false;
    for (qsizetype index = 0; index < uuid.size(); ++index) {
        const QChar character = uuid.at(index);
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (character != QLatin1Char('-'))
                return false;
        } else if (!isHexDigit(character)) {
            return false;
        }
    }
    return true;
}

QString normalizedHost(const QUrl& url)
{
    QString host = url.host().toLower();
    while (host.endsWith(QLatin1Char('.')))
        host.chop(1);
    if (host.startsWith(QLatin1String("www.")))
        host.remove(0, 4);
    return host;
}

QStringList decodedPathSegments(const QUrl& url)
{
    QStringList segments;
    const QString path = url.path(QUrl::FullyEncoded);
    if (path.isEmpty())
        return segments;

    const QStringList encoded = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    segments.reserve(encoded.size());
    for (const QString& part : encoded)
        segments.append(QUrl::fromPercentEncoding(part.toUtf8()));
    return segments;
}

bool isYandexMusicHost(const QString& host)
{
    return host == QLatin1String("music.yandex.ru")
        || host == QLatin1String("music.yandex.com")
        || host == QLatin1String("music.yandex.by")
        || host == QLatin1String("music.yandex.kz")
        || host == QLatin1String("music.yandex.uz");
}

// Whether the address starts with a scheme and "://". A colon inside a query
// (for example a "next" parameter) does not count, so a scheme-less address is
// still completed instead of being rejected.
bool hasUrlScheme(const QString& value)
{
    if (value.isEmpty())
        return false;
    const qsizetype colon = value.indexOf(QLatin1Char(':'));
    if (colon <= 0 || !value.at(0).isLetter())
        return false;
    for (qsizetype index = 0; index < colon; ++index) {
        const QChar character = value.at(index);
        if (!isAsciiLetterOrDigit(character) && character != QLatin1Char('+')
            && character != QLatin1Char('-') && character != QLatin1Char('.')) {
            return false;
        }
    }
    return value.mid(colon, 3) == QLatin1String("://");
}

ProviderLink parseYandexMusicLink(const QStringList& segments)
{
    if (segments.size() == 2) {
        const QString& first = segments.at(0);
        const QString& second = segments.at(1);
        if (first == QLatin1String("album") && isNumericId(second))
            return makeLink(QStringLiteral("yandex"), QStringLiteral("album"), second);
        if (first == QLatin1String("track") && isNumericId(second))
            return makeLink(QStringLiteral("yandex"), QStringLiteral("track"), second);
        if (first == QLatin1String("playlists") && isYandexPlaylistId(second))
            return makeLink(QStringLiteral("yandex"), QStringLiteral("playlist"), second.toLower());
        return {};
    }

    if (segments.size() == 4 && segments.at(0) == QLatin1String("album")
        && segments.at(2) == QLatin1String("track")
        && isNumericId(segments.at(1)) && isNumericId(segments.at(3))) {
        return makeLink(QStringLiteral("yandex"), QStringLiteral("track"), segments.at(3));
    }
    return {};
}

ProviderLink parseSpotifyLink(const QStringList& segments)
{
    qsizetype start = 0;
    if (!segments.isEmpty()
        && segments.first().startsWith(QLatin1String("intl-"), Qt::CaseInsensitive)) {
        start = 1;
    }
    if (segments.size() - start != 2)
        return {};

    const QString kind = segments.at(start).toLower();
    if (kind != QLatin1String("track") && kind != QLatin1String("album")
        && kind != QLatin1String("playlist") && kind != QLatin1String("artist")) {
        return {};
    }

    const QString id = segments.at(start + 1);
    if (!isSpotifyId(id))
        return {};
    return makeLink(QStringLiteral("spotify"), kind, id);
}

ProviderLink parseAppleMusicLink(const QStringList& segments, const QUrlQuery& query)
{
    for (qsizetype index = 0; index < segments.size(); ++index) {
        const QString kind = segments.at(index).toLower();
        if (kind != QLatin1String("album") && kind != QLatin1String("song")
            && kind != QLatin1String("artist") && kind != QLatin1String("playlist")) {
            continue;
        }

        const QStringList tail = segments.mid(index + 1);
        if (tail.isEmpty())
            return {};

        QString id = tail.last();
        if (id.startsWith(QLatin1String("id"), Qt::CaseInsensitive)
            && isNumericId(id.mid(2))) {
            id = id.mid(2);
        }

        if (kind == QLatin1String("album")) {
            // Apple shares an individual song as an album page plus ?i=<song id>.
            const QString songId =
                query.queryItemValue(QStringLiteral("i"), QUrl::FullyDecoded).trimmed();
            if (isNumericId(songId))
                return makeLink(QStringLiteral("apple"), QStringLiteral("track"), songId);
            if (isNumericId(id))
                return makeLink(QStringLiteral("apple"), QStringLiteral("album"), id);
            return {};
        }
        if (kind == QLatin1String("song")) {
            if (isNumericId(id))
                return makeLink(QStringLiteral("apple"), QStringLiteral("track"), id);
            return {};
        }
        if (kind == QLatin1String("artist")) {
            if (isNumericId(id))
                return makeLink(QStringLiteral("apple"), QStringLiteral("artist"), id);
            return {};
        }
        if (kind == QLatin1String("playlist")) {
            if (isApplePlaylistId(id))
                return makeLink(QStringLiteral("apple"), QStringLiteral("playlist"), id);
            return {};
        }
    }
    return {};
}

bool isSoundCloudRootPage(const QString& segment)
{
    static const QStringList reserved = {
        QStringLiteral("discover"), QStringLiteral("search"), QStringLiteral("stream"),
        QStringLiteral("you"), QStringLiteral("upload"), QStringLiteral("settings"),
        QStringLiteral("notifications"), QStringLiteral("messages"), QStringLiteral("charts"),
        QStringLiteral("tags"), QStringLiteral("people"), QStringLiteral("feed"),
        QStringLiteral("stations"), QStringLiteral("jobs"), QStringLiteral("pro"),
        QStringLiteral("creators"), QStringLiteral("imprint"), QStringLiteral("pages"),
    };
    return reserved.contains(segment.toLower());
}

bool isSoundCloudUserPage(const QString& segment)
{
    static const QStringList reserved = {
        QStringLiteral("tracks"), QStringLiteral("albums"), QStringLiteral("likes"),
        QStringLiteral("reposts"), QStringLiteral("sets"), QStringLiteral("playlists"),
        QStringLiteral("followers"), QStringLiteral("following"),
        QStringLiteral("comments"), QStringLiteral("popular-tracks"),
    };
    return reserved.contains(segment.toLower());
}

ProviderLink parseSoundCloudLink(const QStringList& segments)
{
    if (segments.isEmpty())
        return {};

    const QString& artist = segments.first();
    if (!isSoundCloudSegment(artist) || isSoundCloudRootPage(artist))
        return {};

    if (segments.size() == 1)
        return makeLink(QStringLiteral("soundcloud"), QStringLiteral("artist"), artist);

    if (segments.size() == 2) {
        const QString& track = segments.at(1);
        if (isSoundCloudUserPage(track) || !isSoundCloudSegment(track))
            return {};
        return makeLink(QStringLiteral("soundcloud"), QStringLiteral("track"),
                        artist + QLatin1Char('/') + track);
    }

    if (segments.size() == 3
        && segments.at(1).compare(QLatin1String("sets"), Qt::CaseInsensitive) == 0) {
        const QString& name = segments.at(2);
        if (!isSoundCloudSegment(name))
            return {};
        return makeLink(QStringLiteral("soundcloud"), QStringLiteral("playlist"),
                        artist + QStringLiteral("/sets/") + name);
    }
    return {};
}

ProviderLink parseYoutubeMusicLink(const QStringList& segments, const QUrlQuery& query)
{
    if (segments.size() == 1 && segments.first() == QLatin1String("watch")) {
        const QString id =
            query.queryItemValue(QStringLiteral("v"), QUrl::FullyDecoded).trimmed();
        if (!isYoutubeId(id))
            return {};
        return makeLink(QStringLiteral("youtube"), QStringLiteral("track"), id);
    }

    if (segments.size() == 1 && segments.first() == QLatin1String("playlist")) {
        const QString id =
            query.queryItemValue(QStringLiteral("list"), QUrl::FullyDecoded).trimmed();
        if (!isYoutubeId(id))
            return {};
        return makeLink(QStringLiteral("youtube"), QStringLiteral("playlist"), id);
    }

    if (segments.size() == 2 && segments.first() == QLatin1String("channel")) {
        const QString id = segments.at(1);
        if (!isYoutubeId(id))
            return {};
        return makeLink(QStringLiteral("youtube"), QStringLiteral("artist"), id);
    }
    return {};
}

// ---------------------------------------------------------------------------
// Provider JSON
// ---------------------------------------------------------------------------

QString firstString(const QJsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (!value.isString())
            continue;
        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            return text;
    }
    return {};
}

QString nestedString(const QJsonObject& object, const char* objectKey,
                     std::initializer_list<const char*> keys)
{
    return firstString(object.value(QString::fromLatin1(objectKey)).toObject(), keys);
}

std::optional<qint64> jsonInteger(const QJsonValue& value)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (!std::isfinite(number))
            return std::nullopt;
        if (number >= static_cast<double>(std::numeric_limits<qint64>::max())
            || number < static_cast<double>(std::numeric_limits<qint64>::min())) {
            return std::nullopt;
        }
        return static_cast<qint64>(number);
    }
    if (value.isString()) {
        bool ok = false;
        const qint64 number = value.toString().trimmed().toLongLong(&ok);
        if (ok)
            return number;
    }
    return std::nullopt;
}

QString jsonIdText(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().trimmed();
    if (value.isDouble()) {
        const auto number = jsonInteger(value);
        if (number && *number > 0)
            return QString::number(*number);
    }
    return {};
}

QString idFromValue(const QJsonValue& value)
{
    if (!value.isObject())
        return jsonIdText(value);

    const QJsonObject object = value.toObject();
    for (const char* key : {"videoId", "video_id", "id", "trackId", "track_id"}) {
        const QString id = jsonIdText(object.value(QString::fromLatin1(key)));
        if (!id.isEmpty())
            return id;
    }
    return {};
}

QString trackIdOf(const QJsonObject& object)
{
    for (const char* key : {"id", "trackId", "track_id", "videoId", "video_id"}) {
        const QString id = idFromValue(object.value(QString::fromLatin1(key)));
        if (!id.isEmpty())
            return id;
    }

    // A YouTube playlist item keeps the video id in snippet.resourceId.
    const QJsonObject snippet = object.value(QStringLiteral("snippet")).toObject();
    const QJsonObject resourceId = snippet.value(QStringLiteral("resourceId")).toObject();
    for (const char* key : {"videoId", "video_id"}) {
        const QString id = jsonIdText(resourceId.value(QString::fromLatin1(key)));
        if (!id.isEmpty())
            return id;
    }
    return {};
}

bool isSafeTrackId(const QString& id)
{
    if (id.isEmpty() || id.size() > 128)
        return false;
    for (const QChar character : id) {
        if (isAsciiLetterOrDigit(character))
            continue;
        switch (character.unicode()) {
        case '.':
        case '_':
        case '-':
        case ':':
        case '~':
            continue;
        default:
            return false;
        }
    }
    return true;
}

void appendArtistNames(const QJsonValue& value, QStringList& names)
{
    if (value.isString()) {
        const QString name = value.toString().trimmed();
        if (!name.isEmpty())
            names.append(name);
        return;
    }
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue item : array)
            appendArtistNames(item, names);
        return;
    }
    if (value.isObject()) {
        const QString name = firstString(value.toObject(),
                                         {"name", "title", "username", "channelTitle"});
        if (!name.isEmpty())
            names.append(name);
    }
}

QString artistsOf(const QJsonObject& object)
{
    QStringList names;
    appendArtistNames(object.value(QStringLiteral("artists")), names);
    if (names.isEmpty())
        appendArtistNames(object.value(QStringLiteral("artist")), names);

    if (names.isEmpty()) {
        const QString direct = firstString(
            object, {"artistName", "artist_name", "performer", "channelTitle"});
        if (!direct.isEmpty())
            names.append(direct);
    }

    if (names.isEmpty()) {
        const QString user = nestedString(object, "user", {"username", "name"});
        if (!user.isEmpty())
            names.append(user);
    }

    if (names.isEmpty()) {
        const QString channel = nestedString(object, "snippet", {"channelTitle", "channel_title"});
        if (!channel.isEmpty())
            names.append(channel);
    }

    QStringList unique;
    QSet<QString> seen;
    for (const QString& raw : names) {
        const QString name = raw.trimmed();
        if (name.isEmpty())
            continue;
        const QString key = name.toCaseFolded();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        unique.append(name);
    }
    return unique.join(QStringLiteral(", "));
}

QString albumOf(const QJsonObject& object)
{
    QString album = firstString(object,
                                {"albumTitle", "album_title", "albumName", "album_name",
                                 "collectionName", "collection_name"});
    if (!album.isEmpty())
        return album;

    const QJsonValue albumValue = object.value(QStringLiteral("album"));
    if (albumValue.isString()) {
        album = albumValue.toString().trimmed();
    } else if (albumValue.isObject()) {
        album = firstString(albumValue.toObject(), {"title", "name"});
    }
    if (!album.isEmpty())
        return album;

    const QJsonArray albums = object.value(QStringLiteral("albums")).toArray();
    if (!albums.isEmpty()) {
        const QJsonObject first = albums.first().toObject();
        album = firstString(first, {"title", "name"});
    }
    return album;
}

std::optional<qint64> isoDurationSeconds(const QString& text)
{
    static const QRegularExpression pattern(
        QString::fromLatin1(R"(^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch())
        return std::nullopt;

    constexpr qint64 kMaxComponent = 1000000000LL;
    qint64 seconds = 0;
    bool any = false;
    for (int group = 1; group <= 4; ++group) {
        const QString captured = match.captured(group);
        if (captured.isEmpty())
            continue;
        any = true;
        const qint64 value = std::min<qint64>(captured.toLongLong(), kMaxComponent);
        switch (group) {
        case 1:
            seconds += value * 86400;
            break;
        case 2:
            seconds += value * 3600;
            break;
        case 3:
            seconds += value * 60;
            break;
        default:
            seconds += value;
            break;
        }
    }
    if (!any)
        return std::nullopt;
    return seconds;
}

int clampDuration(qint64 seconds)
{
    if (seconds <= 0)
        return 0;
    if (seconds > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(seconds);
}

int durationSecondsOf(const QJsonObject& object, const QString& provider)
{
    for (const char* key : {"durationMs", "duration_ms", "durationMillis", "duration_millis",
                            "trackTimeMillis", "track_time_millis", "durationInMillis",
                            "duration_in_millis"}) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        if (const auto milliseconds = jsonInteger(value)) {
            const qint64 seconds = *milliseconds > 0 ? *milliseconds / 1000 : 0;
            return clampDuration(seconds);
        }
        return 0;
    }

    const QJsonObject contentDetails = object.value(QStringLiteral("contentDetails")).toObject();
    if (const auto seconds = isoDurationSeconds(firstString(contentDetails, {"duration"})))
        return clampDuration(*seconds);

    const QJsonValue durationValue = object.value(QStringLiteral("duration"));
    if (durationValue.isString()) {
        if (const auto seconds = isoDurationSeconds(durationValue.toString()))
            return clampDuration(*seconds);
    }

    for (const char* key : {"durationSeconds", "duration_seconds", "durationSec",
                            "duration_sec", "lengthSeconds", "length_seconds"}) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        if (const auto seconds = jsonInteger(value))
            return clampDuration(*seconds);
        return 0;
    }

    if (!durationValue.isUndefined() && !durationValue.isNull()) {
        if (const auto value = jsonInteger(durationValue)) {
            if (provider == QLatin1String("soundcloud")) {
                // SoundCloud reports milliseconds. A value below one second is
                // treated as seconds so a generic response that uses `duration`
                // still reads naturally; real SoundCloud tracks are longer.
                const qint64 seconds = (*value > 0 && *value < 1000)
                    ? *value
                    : (*value / 1000);
                return clampDuration(seconds);
            }
            return clampDuration(*value);
        }
        return 0;
    }

    const QJsonValue length = object.value(QStringLiteral("length"));
    if (!length.isUndefined() && !length.isNull()) {
        if (const auto value = jsonInteger(length))
            return clampDuration(*value);
    }
    return 0;
}

QString stringFromArtworkValue(const QJsonValue& value)
{
    if (value.isString())
        return value.toString().trimmed();
    if (value.isObject())
        return firstString(value.toObject(), {"url", "src", "href", "uri"});
    return {};
}

QString normalizedArtworkUrl(QString url, const QString& provider)
{
    if (url.isEmpty())
        return {};

    url.replace(QStringLiteral("%%"), QStringLiteral("200x200"));
    if (url.startsWith(QLatin1String("//")))
        url.prepend(QStringLiteral("https:"));
    else if (provider == QLatin1String("yandex") && !url.contains(QLatin1String("://")))
        url.prepend(QStringLiteral("https://"));

    const QUrl parsed(url, QUrl::TolerantMode);
    const QString scheme = parsed.scheme().toLower();
    if (parsed.host().isEmpty()
        || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        return {};
    }
    return url;
}

QString artworkOf(const QJsonObject& object, const QString& provider)
{
    for (const char* key : {"artworkUrl", "artwork_url", "coverUrl", "cover_url",
                            "coverUri", "cover_uri", "cover", "ogImage", "og_image",
                            "imageUrl", "image_url", "artwork", "image",
                            "thumbnailUrl", "thumbnail_url", "thumbnail",
                            "artworkUrl100", "artworkUrl60", "artwork_url_100"}) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (value.isUndefined() || value.isNull())
            continue;
        const QString url = normalizedArtworkUrl(stringFromArtworkValue(value), provider);
        if (!url.isEmpty())
            return url;
    }

    const QJsonObject album = object.value(QStringLiteral("album")).toObject();
    const QJsonArray images = album.value(QStringLiteral("images")).toArray();
    for (const QJsonValue image : images) {
        const QString url = normalizedArtworkUrl(stringFromArtworkValue(image), provider);
        if (!url.isEmpty())
            return url;
    }

    const QJsonObject snippet = object.value(QStringLiteral("snippet")).toObject();
    const QJsonObject thumbnails = snippet.value(QStringLiteral("thumbnails")).toObject();
    for (const char* key : {"maxres", "high", "medium", "default"}) {
        const QString url = normalizedArtworkUrl(
            stringFromArtworkValue(thumbnails.value(QString::fromLatin1(key))), provider);
        if (!url.isEmpty())
            return url;
    }

    const QString avatar = normalizedArtworkUrl(
        nestedString(object, "user", {"avatar_url", "avatarUrl"}), provider);
    if (!avatar.isEmpty())
        return avatar;

    return {};
}

QString titleOf(const QJsonObject& object)
{
    const QString title = firstString(object, {"title", "name", "trackName", "track_name"});
    if (!title.isEmpty())
        return title;
    return nestedString(object, "snippet", {"title"});
}

std::optional<ProviderTrack> trackFromObject(const QString& provider, QJsonObject object)
{
    // Common envelopes: a Spotify playlist item carries `track`, an Apple Music
    // API item carries `attributes`, and some responses wrap a single item.
    // Only unwrap when the nested object looks like the item; otherwise a
    // wrapper with its own title would be replaced by an unrelated object.
    for (const char* key : {"track", "song", "item", "attributes", "result"}) {
        QJsonObject nested = object.value(QString::fromLatin1(key)).toObject();
        if (nested.isEmpty())
            continue;
        if (titleOf(nested).isEmpty() && !titleOf(object).isEmpty())
            continue;

        if (trackIdOf(nested).isEmpty()) {
            const QString outerId = trackIdOf(object);
            if (!outerId.isEmpty())
                nested.insert(QStringLiteral("id"), outerId);
        }
        object = nested;
        break;
    }

    const QString id = trackIdOf(object);
    if (!isSafeTrackId(id))
        return std::nullopt;

    const QString title = titleOf(object);
    if (title.isEmpty())
        return std::nullopt;

    ProviderTrack track;
    track.provider = provider;
    track.id = id;
    track.title = title;
    track.artist = artistsOf(object);
    track.album = albumOf(object);
    track.durationSeconds = durationSecondsOf(object, provider);
    track.artworkUrl = artworkOf(object, provider);
    return track;
}

void appendTrackObjects(const QString& provider, const QJsonValue& value,
                        QVector<ProviderTrack>& tracks);

void appendWrappedLists(const QString& provider, const QJsonObject& object,
                        QVector<ProviderTrack>& tracks)
{
    for (const char* key : {"items", "results", "tracks", "collection", "data",
                            "entries", "songs"}) {
        const QJsonValue value = object.value(QString::fromLatin1(key));
        if (value.isArray())
            appendTrackObjects(provider, value, tracks);
        else if (value.isObject())
            appendWrappedLists(provider, value.toObject(), tracks);
    }
}

void appendTrackObjects(const QString& provider, const QJsonValue& value,
                        QVector<ProviderTrack>& tracks)
{
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue item : array) {
            if (item.isArray()) {
                appendTrackObjects(provider, item, tracks);
            } else if (item.isObject()) {
                if (auto track = trackFromObject(provider, item.toObject()))
                    tracks.append(*track);
            }
        }
        return;
    }

    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        if (auto track = trackFromObject(provider, object)) {
            tracks.append(*track);
            return;
        }
        appendWrappedLists(provider, object, tracks);
    }
}

QVector<ProviderTrack> firstTracks(const QString& provider,
                                   std::initializer_list<QJsonValue> candidates)
{
    for (const QJsonValue& candidate : candidates) {
        QVector<ProviderTrack> tracks;
        appendTrackObjects(provider, candidate, tracks);
        if (!tracks.isEmpty())
            return tracks;
    }
    return {};
}

} // namespace

ProviderLink parseMusicProviderLink(const QString& url)
{
    const QString text = url.trimmed();
    if (text.isEmpty())
        return {};

    // Like the Android parser, a scheme-less address is completed with https.
    QString normalized = text;
    if (!hasUrlScheme(normalized))
        normalized.prepend(QStringLiteral("https://"));

    const QUrl parsed(normalized, QUrl::StrictMode);
    if (!parsed.isValid())
        return {};

    const QString scheme = parsed.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return {};
    if (!parsed.userInfo().isEmpty())
        return {};

    const QString host = normalizedHost(parsed);
    if (host.isEmpty())
        return {};

    const QStringList segments = decodedPathSegments(parsed);
    const QUrlQuery query(parsed);

    if (isYandexMusicHost(host))
        return parseYandexMusicLink(segments);
    if (host == QLatin1String("open.spotify.com"))
        return parseSpotifyLink(segments);
    if (host == QLatin1String("music.apple.com"))
        return parseAppleMusicLink(segments, query);
    if (host == QLatin1String("soundcloud.com"))
        return parseSoundCloudLink(segments);
    if (host == QLatin1String("music.youtube.com"))
        return parseYoutubeMusicLink(segments, query);
    return {};
}

QString providerDisplayName(const QString& provider)
{
    const QString id = canonicalProviderId(provider);
    if (id == QLatin1String("yandex"))
        return QStringLiteral("Yandex Music");
    if (id == QLatin1String("spotify"))
        return QStringLiteral("Spotify");
    if (id == QLatin1String("apple"))
        return QStringLiteral("Apple Music");
    if (id == QLatin1String("soundcloud"))
        return QStringLiteral("SoundCloud");
    if (id == QLatin1String("youtube"))
        return QStringLiteral("YouTube Music");
    return {};
}

QVector<ProviderTrack> providerTracksFromJson(const QString& provider,
                                              const QJsonObject& response)
{
    const QString id = canonicalProviderId(provider);
    if (id.isEmpty())
        return {};

    const QJsonValue resultValue = response.value(QStringLiteral("result"));
    const QJsonObject resultObject = resultValue.toObject();
    const QJsonObject root = resultObject.isEmpty() ? response : resultObject;

    if (id == QLatin1String("yandex")) {
        const QJsonObject tracksObject = root.value(QStringLiteral("tracks")).toObject();
        const QJsonObject libraryObject = root.value(QStringLiteral("library")).toObject();
        const QVector<ProviderTrack> tracks = firstTracks(id, {
            tracksObject.value(QStringLiteral("results")),
            root.value(QStringLiteral("volumes")),
            libraryObject.value(QStringLiteral("tracks")),
            root.value(QStringLiteral("tracks")),
            root.value(QStringLiteral("sequence")),
            resultValue.isArray() ? resultValue : QJsonValue(),
            response.value(QStringLiteral("tracks")).toObject()
                .value(QStringLiteral("results")),
        });
        if (!tracks.isEmpty())
            return tracks;
    }

    if (id == QLatin1String("spotify")) {
        const QJsonObject tracksObject = root.value(QStringLiteral("tracks")).toObject();
        const QJsonObject albumObject = root.value(QStringLiteral("album")).toObject();
        const QJsonObject playlistObject = root.value(QStringLiteral("playlist")).toObject();
        const QVector<ProviderTrack> tracks = firstTracks(id, {
            tracksObject.value(QStringLiteral("items")),
            root.value(QStringLiteral("items")),
            albumObject.value(QStringLiteral("tracks")).toObject()
                .value(QStringLiteral("items")),
            playlistObject.value(QStringLiteral("tracks")).toObject()
                .value(QStringLiteral("items")),
            root.value(QStringLiteral("tracks")),
            resultValue.isArray() ? resultValue : QJsonValue(),
        });
        if (!tracks.isEmpty())
            return tracks;
    }

    if (id == QLatin1String("apple")) {
        const QVector<ProviderTrack> tracks = firstTracks(id, {
            root.value(QStringLiteral("results")),
            root.value(QStringLiteral("data")),
            root.value(QStringLiteral("items")),
            root.value(QStringLiteral("tracks")),
            resultValue.isArray() ? resultValue : QJsonValue(),
        });
        if (!tracks.isEmpty())
            return tracks;
    }

    if (id == QLatin1String("soundcloud")) {
        const QVector<ProviderTrack> tracks = firstTracks(id, {
            root.value(QStringLiteral("collection")),
            root.value(QStringLiteral("tracks")),
            root.value(QStringLiteral("items")),
            root.value(QStringLiteral("data")),
            resultValue.isArray() ? resultValue : QJsonValue(),
        });
        if (!tracks.isEmpty())
            return tracks;
    }

    if (id == QLatin1String("youtube")) {
        const QVector<ProviderTrack> tracks = firstTracks(id, {
            root.value(QStringLiteral("items")),
            root.value(QStringLiteral("contents")),
            resultValue.isArray() ? resultValue : QJsonValue(),
        });
        if (!tracks.isEmpty())
            return tracks;
    }

    const QJsonObject tracksObject = root.value(QStringLiteral("tracks")).toObject();
    const QVector<ProviderTrack> tracks = firstTracks(id, {
        tracksObject.value(QStringLiteral("items")),
        tracksObject.value(QStringLiteral("results")),
        root.value(QStringLiteral("tracks")),
        root.value(QStringLiteral("items")),
        root.value(QStringLiteral("results")),
        root.value(QStringLiteral("collection")),
        root.value(QStringLiteral("data")),
        root.value(QStringLiteral("entries")),
        root.value(QStringLiteral("songs")),
        resultValue.isArray() ? resultValue : QJsonValue(),
    });
    if (!tracks.isEmpty())
        return tracks;

    // A response for one track is accepted, but an album or playlist object is
    // not turned into a track just because it also has an id and a title.
    const bool looksLikeCollection = root.contains(QStringLiteral("volumes"))
        || root.contains(QStringLiteral("trackCount"))
        || root.contains(QStringLiteral("track_count"))
        || root.contains(QStringLiteral("tracks"));
    if (!looksLikeCollection) {
        if (auto single = trackFromObject(id, root))
            return {*single};
    }
    return {};
}

} // namespace beygram::telegram
