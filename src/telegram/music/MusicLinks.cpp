#include "MusicLinks.h"

#include <QStringList>
#include <QUrl>

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

QString percentEncode(const QString& value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

// SoundCloud track ids are permalink paths such as "artist/track". Each
// segment is encoded separately so the slashes stay path separators. Empty,
// "." and ".." segments would change the path and are rejected; every other
// character is percent-encoded and therefore safe.
QString soundCloudPath(const QString& id)
{
    QString path = id;
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    if (path.isEmpty())
        return {};

    const QStringList segments = path.split(QLatin1Char('/'), Qt::KeepEmptyParts);
    QStringList encoded;
    encoded.reserve(segments.size());
    for (const QString& segment : segments) {
        if (segment.isEmpty() || segment == QLatin1String(".")
            || segment == QLatin1String("..")) {
            return {};
        }
        encoded.append(percentEncode(segment));
    }
    return encoded.join(QLatin1Char('/'));
}

} // namespace

QString providerTrackUrl(const QString& provider, const QString& id)
{
    const QString canonical = canonicalProviderId(provider);
    const QString trimmed = id.trimmed();
    if (canonical.isEmpty() || trimmed.isEmpty())
        return {};

    if (canonical == QLatin1String("yandex")) {
        return QStringLiteral("https://music.yandex.ru/track/") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("spotify")) {
        return QStringLiteral("https://open.spotify.com/track/") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("apple")) {
        return QStringLiteral("https://music.apple.com/song/") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("soundcloud")) {
        const QString path = soundCloudPath(trimmed);
        if (path.isEmpty())
            return {};
        return QStringLiteral("https://soundcloud.com/") + path;
    }
    if (canonical == QLatin1String("youtube")) {
        return QStringLiteral("https://music.youtube.com/watch?v=") + percentEncode(trimmed);
    }
    return {};
}

QString providerAlbumUrl(const QString& provider, const QString& id)
{
    const QString canonical = canonicalProviderId(provider);
    const QString trimmed = id.trimmed();
    if (canonical.isEmpty() || trimmed.isEmpty())
        return {};

    if (canonical == QLatin1String("yandex")) {
        return QStringLiteral("https://music.yandex.ru/album/") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("spotify")) {
        return QStringLiteral("https://open.spotify.com/album/") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("apple")) {
        return QStringLiteral("https://music.apple.com/album/") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("soundcloud")) {
        const QString path = soundCloudPath(trimmed);
        if (path.isEmpty())
            return {};
        return QStringLiteral("https://soundcloud.com/") + path;
    }
    if (canonical == QLatin1String("youtube")) {
        return QStringLiteral("https://music.youtube.com/playlist?list=") + percentEncode(trimmed);
    }
    return {};
}

QString musicSearchUrl(const QString& provider, const QString& query)
{
    const QString canonical = canonicalProviderId(provider);
    const QString trimmed = query.trimmed();
    if (canonical.isEmpty() || trimmed.isEmpty())
        return {};

    if (canonical == QLatin1String("yandex")) {
        return QStringLiteral("https://music.yandex.ru/search?text=") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("spotify")) {
        return QStringLiteral("https://open.spotify.com/search/") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("apple")) {
        return QStringLiteral("https://music.apple.com/search?term=") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("soundcloud")) {
        return QStringLiteral("https://soundcloud.com/search?q=") + percentEncode(trimmed);
    }
    if (canonical == QLatin1String("youtube")) {
        return QStringLiteral("https://music.youtube.com/search?q=") + percentEncode(trimmed);
    }
    return {};
}

} // namespace beygram::telegram
