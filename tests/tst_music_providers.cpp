#include "telegram/music/MusicProviders.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

QJsonObject jsonObject(const char* text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}

QVector<ProviderTrack> tracksFrom(const char* provider, const char* text)
{
    return providerTracksFromJson(QString::fromLatin1(provider), jsonObject(text));
}

void verifyEmptyLink(const QString& url)
{
    const ProviderLink link = parseMusicProviderLink(url);
    QVERIFY2(link.provider.isEmpty(), qPrintable(url));
    QVERIFY2(link.kind.isEmpty(), qPrintable(url));
    QVERIFY2(link.id.isEmpty(), qPrintable(url));
}

} // namespace

class MusicProvidersTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesYandexTrackAndAlbumLinks();
    void parsesYandexPlaylistLinks();
    void parsesSpotifyLinks();
    void parsesAppleMusicLinks();
    void parsesSoundCloudLinks();
    void parsesYoutubeMusicLinks();
    void rejectsUnknownAndMalformedLinks();
    void rejectsUnsafeIds();
    void acceptsHttpTrimsInputAndNormalizesHosts();
    void returnsDisplayNames();

    void readsYandexSearchResponse();
    void readsYandexAlbumVolumes();
    void readsSpotifyResponse();
    void readsAppleItunesResponse();
    void readsAppleMusicApiResponse();
    void readsSoundCloudResponse();
    void readsYoutubeResponse();
    void readsGenericResponse();
    void skipsMalformedTracksAndClampsDuration();
    void returnsNothingForUnknownProviderOrEmptyResponse();
};

void MusicProvidersTest::parsesYandexTrackAndAlbumLinks()
{
    ProviderLink link = parseMusicProviderLink(
        QStringLiteral("  https://music.yandex.ru/album/123/track/456?utm_source=share  "));
    QCOMPARE(link.provider, QStringLiteral("yandex"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("456"));

    link = parseMusicProviderLink(QStringLiteral("http://music.yandex.com/track/789"));
    QCOMPARE(link.provider, QStringLiteral("yandex"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("789"));

    link = parseMusicProviderLink(QStringLiteral("https://music.yandex.by/album/42"));
    QCOMPARE(link.provider, QStringLiteral("yandex"));
    QCOMPARE(link.kind, QStringLiteral("album"));
    QCOMPARE(link.id, QStringLiteral("42"));

    link = parseMusicProviderLink(QStringLiteral("music.yandex.ru/album/41286299/"));
    QCOMPARE(link.provider, QStringLiteral("yandex"));
    QCOMPARE(link.kind, QStringLiteral("album"));
    QCOMPARE(link.id, QStringLiteral("41286299"));
}

void MusicProvidersTest::parsesYandexPlaylistLinks()
{
    const QString uuid = QStringLiteral("123e4567-e89b-12d3-a456-426614174000");
    ProviderLink link = parseMusicProviderLink(
        QStringLiteral("https://music.yandex.kz/playlists/") + uuid.toUpper());
    QCOMPARE(link.provider, QStringLiteral("yandex"));
    QCOMPARE(link.kind, QStringLiteral("playlist"));
    QCOMPARE(link.id, uuid);

    link = parseMusicProviderLink(
        QStringLiteral("https://music.yandex.uz/playlists/lk.") + uuid);
    QCOMPARE(link.kind, QStringLiteral("playlist"));
    QCOMPARE(link.id, QStringLiteral("lk.") + uuid);

    // The Android parser leaves Yandex artist pages to the browser.
    verifyEmptyLink(QStringLiteral("https://music.yandex.ru/artist/777"));
    verifyEmptyLink(QStringLiteral("https://music.yandex.ru/playlists/not-a-uuid"));
}

void MusicProvidersTest::parsesSpotifyLinks()
{
    ProviderLink link = parseMusicProviderLink(
        QStringLiteral("https://open.spotify.com/track/4uLU6hMCjMI75M1A2tKUQC?si=abc"));
    QCOMPARE(link.provider, QStringLiteral("spotify"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("4uLU6hMCjMI75M1A2tKUQC"));

    link = parseMusicProviderLink(
        QStringLiteral("https://open.spotify.com/intl-de/album/4uLU6hMCjMI75M1A2tKUQC"));
    QCOMPARE(link.kind, QStringLiteral("album"));
    QCOMPARE(link.id, QStringLiteral("4uLU6hMCjMI75M1A2tKUQC"));

    link = parseMusicProviderLink(
        QStringLiteral("https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M"));
    QCOMPARE(link.kind, QStringLiteral("playlist"));
    QCOMPARE(link.id, QStringLiteral("37i9dQZF1DXcBWIGoYBM5M"));

    link = parseMusicProviderLink(
        QStringLiteral("https://open.spotify.com/artist/0gxyHStUsqpMadRV0Di1Qt"));
    QCOMPARE(link.kind, QStringLiteral("artist"));
    QCOMPARE(link.id, QStringLiteral("0gxyHStUsqpMadRV0Di1Qt"));
}

void MusicProvidersTest::parsesAppleMusicLinks()
{
    ProviderLink link = parseMusicProviderLink(
        QStringLiteral("https://music.apple.com/us/album/random-access-memories/617154241"));
    QCOMPARE(link.provider, QStringLiteral("apple"));
    QCOMPARE(link.kind, QStringLiteral("album"));
    QCOMPARE(link.id, QStringLiteral("617154241"));

    link = parseMusicProviderLink(
        QStringLiteral("https://music.apple.com/us/album/get-lucky/617154241?i=617154246"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("617154246"));

    link = parseMusicProviderLink(
        QStringLiteral("https://music.apple.com/gb/artist/daft-punk/5468295"));
    QCOMPARE(link.kind, QStringLiteral("artist"));
    QCOMPARE(link.id, QStringLiteral("5468295"));

    link = parseMusicProviderLink(
        QStringLiteral("https://music.apple.com/us/playlist/daft-punk-essentials/pl.abc123"));
    QCOMPARE(link.kind, QStringLiteral("playlist"));
    QCOMPARE(link.id, QStringLiteral("pl.abc123"));

    link = parseMusicProviderLink(QStringLiteral("https://music.apple.com/us/song/get-lucky/617154246"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("617154246"));

    verifyEmptyLink(
        QStringLiteral("https://music.apple.com/us/album/random-access-memories/not-a-number"));
}

void MusicProvidersTest::parsesSoundCloudLinks()
{
    ProviderLink link = parseMusicProviderLink(
        QStringLiteral("https://soundcloud.com/daftpunk/get-lucky?si=abc"));
    QCOMPARE(link.provider, QStringLiteral("soundcloud"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("daftpunk/get-lucky"));

    link = parseMusicProviderLink(
        QStringLiteral("https://soundcloud.com/daftpunk/sets/random-access-memories"));
    QCOMPARE(link.kind, QStringLiteral("playlist"));
    QCOMPARE(link.id, QStringLiteral("daftpunk/sets/random-access-memories"));

    link = parseMusicProviderLink(QStringLiteral("https://soundcloud.com/daftpunk"));
    QCOMPARE(link.kind, QStringLiteral("artist"));
    QCOMPARE(link.id, QStringLiteral("daftpunk"));

    verifyEmptyLink(QStringLiteral("https://soundcloud.com/discover"));
    verifyEmptyLink(QStringLiteral("https://soundcloud.com/daftpunk/tracks"));
    verifyEmptyLink(QStringLiteral("https://soundcloud.com/daftpunk/sets"));
}

void MusicProvidersTest::parsesYoutubeMusicLinks()
{
    ProviderLink link = parseMusicProviderLink(
        QStringLiteral("https://music.youtube.com/watch?v=dQw4w9WgXcQ&list=RDdQw4w9WgXcQ"));
    QCOMPARE(link.provider, QStringLiteral("youtube"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("dQw4w9WgXcQ"));

    link = parseMusicProviderLink(
        QStringLiteral("https://music.youtube.com/playlist?list=OLAK5uy_abc-123"));
    QCOMPARE(link.kind, QStringLiteral("playlist"));
    QCOMPARE(link.id, QStringLiteral("OLAK5uy_abc-123"));

    link = parseMusicProviderLink(QStringLiteral("https://music.youtube.com/channel/UCabc_123"));
    QCOMPARE(link.kind, QStringLiteral("artist"));
    QCOMPARE(link.id, QStringLiteral("UCabc_123"));

    verifyEmptyLink(QStringLiteral("https://music.youtube.com/watch"));
    verifyEmptyLink(QStringLiteral("https://music.youtube.com/watch?v=bad%20id"));
    verifyEmptyLink(QStringLiteral("https://youtube.com/watch?v=dQw4w9WgXcQ"));
}

void MusicProvidersTest::rejectsUnknownAndMalformedLinks()
{
    verifyEmptyLink(QString());
    verifyEmptyLink(QStringLiteral("   "));
    verifyEmptyLink(QStringLiteral("ftp://open.spotify.com/track/abc123"));
    verifyEmptyLink(QStringLiteral("javascript:alert(1)"));
    verifyEmptyLink(QStringLiteral("https://example.com/track/abc123"));
    verifyEmptyLink(QStringLiteral("https://music.yandex.ru.evil.com/album/1/track/2"));
    verifyEmptyLink(QStringLiteral("https://user@open.spotify.com/track/abc123"));
    verifyEmptyLink(QStringLiteral("https://open.spotify.com/track/"));
    verifyEmptyLink(QStringLiteral("https://open.spotify.com/track/abc/extra"));
    verifyEmptyLink(QStringLiteral("https://music.yandex.ru/album/abc/track/123"));
    verifyEmptyLink(QStringLiteral("https://music.yandex.ru/album/123/track/abc"));
    verifyEmptyLink(QStringLiteral("https://music.youtube.com/watch?v="));
    verifyEmptyLink(QStringLiteral("https://soundcloud.com/daftpunk/get lucky"));
}

void MusicProvidersTest::rejectsUnsafeIds()
{
    verifyEmptyLink(
        QStringLiteral("https://music.yandex.ru/track/123456789012345678901"));
    verifyEmptyLink(QStringLiteral("https://open.spotify.com/track/abc-def"));
    verifyEmptyLink(QStringLiteral("https://open.spotify.com/track/abc%2Fdef"));
    verifyEmptyLink(QStringLiteral("https://music.apple.com/us/album/name/12%203"));
    verifyEmptyLink(QStringLiteral("https://music.youtube.com/watch?v=abc%2Fdef"));
    verifyEmptyLink(QStringLiteral("https://soundcloud.com/daftpunk/get%20lucky"));
}

void MusicProvidersTest::acceptsHttpTrimsInputAndNormalizesHosts()
{
    ProviderLink link = parseMusicProviderLink(
        QStringLiteral("  HTTP://WWW.OPEN.SPOTIFY.COM/TRACK/abc123  "));
    QCOMPARE(link.provider, QStringLiteral("spotify"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("abc123"));

    link = parseMusicProviderLink(
        QStringLiteral("https://www.music.youtube.com/watch?v=dQw4w9WgXcQ"));
    QCOMPARE(link.provider, QStringLiteral("youtube"));
    QCOMPARE(link.id, QStringLiteral("dQw4w9WgXcQ"));

    link = parseMusicProviderLink(
        QStringLiteral("https://open.spotify.com:443/album/abc123#fragment"));
    QCOMPARE(link.kind, QStringLiteral("album"));
    QCOMPARE(link.id, QStringLiteral("abc123"));

    link = parseMusicProviderLink(QStringLiteral("open.spotify.com/track/abc123"));
    QCOMPARE(link.provider, QStringLiteral("spotify"));
    QCOMPARE(link.kind, QStringLiteral("track"));
    QCOMPARE(link.id, QStringLiteral("abc123"));
}

void MusicProvidersTest::returnsDisplayNames()
{
    QCOMPARE(providerDisplayName(QStringLiteral("yandex")), QStringLiteral("Yandex Music"));
    QCOMPARE(providerDisplayName(QStringLiteral("YANDEX_MUSIC")), QStringLiteral("Yandex Music"));
    QCOMPARE(providerDisplayName(QStringLiteral("spotify")), QStringLiteral("Spotify"));
    QCOMPARE(providerDisplayName(QStringLiteral("  Spotify  ")), QStringLiteral("Spotify"));
    QCOMPARE(providerDisplayName(QStringLiteral("apple")), QStringLiteral("Apple Music"));
    QCOMPARE(providerDisplayName(QStringLiteral("itunes")), QStringLiteral("Apple Music"));
    QCOMPARE(providerDisplayName(QStringLiteral("soundcloud")), QStringLiteral("SoundCloud"));
    QCOMPARE(providerDisplayName(QStringLiteral("youtube")), QStringLiteral("YouTube Music"));
    QCOMPARE(providerDisplayName(QStringLiteral("ytmusic")), QStringLiteral("YouTube Music"));
    QCOMPARE(providerDisplayName(QStringLiteral("tidal")), QString());
    QCOMPARE(providerDisplayName(QString()), QString());
}

void MusicProvidersTest::readsYandexSearchResponse()
{
    const QVector<ProviderTrack> tracks = tracksFrom("yandex", R"json({
        "result": {
            "tracks": {
                "results": [
                    {
                        "id": 1001,
                        "title": "Get Lucky",
                        "artists": [
                            {"id": 1, "name": "Daft Punk"},
                            {"id": 2, "name": "Pharrell Williams"}
                        ],
                        "albums": [{"id": 10, "title": "Random Access Memories"}],
                        "durationMs": 369000,
                        "coverUri": "avatars.yandex.net/get-music-content/abc/%%"
                    },
                    {"id": 1002, "title": "   ", "durationMs": 1000},
                    {"title": "Missing id"},
                    {"id": 1003}
                ]
            }
        }
    })json");

    QCOMPARE(tracks.size(), 1);
    const ProviderTrack& track = tracks.first();
    QCOMPARE(track.provider, QStringLiteral("yandex"));
    QCOMPARE(track.id, QStringLiteral("1001"));
    QCOMPARE(track.title, QStringLiteral("Get Lucky"));
    QCOMPARE(track.artist, QStringLiteral("Daft Punk, Pharrell Williams"));
    QCOMPARE(track.album, QStringLiteral("Random Access Memories"));
    QCOMPARE(track.durationSeconds, 369);
    QCOMPARE(track.artworkUrl,
             QStringLiteral("https://avatars.yandex.net/get-music-content/abc/200x200"));
}

void MusicProvidersTest::readsYandexAlbumVolumes()
{
    const QVector<ProviderTrack> tracks = tracksFrom("yandex", R"json({
        "result": {
            "id": 10,
            "title": "Random Access Memories",
            "artists": [{"name": "Daft Punk"}],
            "volumes": [
                [
                    {"id": 1, "title": "Give Life Back to Music", "durationMs": 274000}
                ],
                [
                    {"id": 2, "title": "The Game of Love", "durationMs": -1000},
                    {"id": 3, "title": "Giorgio by Moroder", "durationMs": 549000}
                ]
            ]
        }
    })json");

    QCOMPARE(tracks.size(), 3);
    QCOMPARE(tracks.at(0).id, QStringLiteral("1"));
    QCOMPARE(tracks.at(0).durationSeconds, 274);
    QCOMPARE(tracks.at(1).durationSeconds, 0);
    QCOMPARE(tracks.at(2).id, QStringLiteral("3"));
    QCOMPARE(tracks.at(2).durationSeconds, 549);
}

void MusicProvidersTest::readsSpotifyResponse()
{
    const QVector<ProviderTrack> tracks = tracksFrom("spotify", R"json({
        "tracks": {
            "items": [
                {
                    "id": "4uLU6hMCjMI75M1A2tKUQC",
                    "name": "Never Gonna Give You Up",
                    "artists": [{"name": "Rick Astley"}],
                    "album": {
                        "name": "Whenever You Need Somebody",
                        "images": [{"url": "https://i.scdn.co/image/abc"}]
                    },
                    "duration_ms": 213573
                },
                {"id": "bad/id", "name": "Bad", "duration_ms": 1000},
                {"id": "emptyTitle", "name": "   "}
            ]
        }
    })json");

    QCOMPARE(tracks.size(), 1);
    const ProviderTrack& track = tracks.first();
    QCOMPARE(track.provider, QStringLiteral("spotify"));
    QCOMPARE(track.id, QStringLiteral("4uLU6hMCjMI75M1A2tKUQC"));
    QCOMPARE(track.title, QStringLiteral("Never Gonna Give You Up"));
    QCOMPARE(track.artist, QStringLiteral("Rick Astley"));
    QCOMPARE(track.album, QStringLiteral("Whenever You Need Somebody"));
    QCOMPARE(track.durationSeconds, 213);
    QCOMPARE(track.artworkUrl, QStringLiteral("https://i.scdn.co/image/abc"));
}

void MusicProvidersTest::readsAppleItunesResponse()
{
    const QVector<ProviderTrack> tracks = tracksFrom("apple", R"json({
        "results": [
            {
                "wrapperType": "track",
                "trackId": 1440833100,
                "trackName": "Get Lucky",
                "artistName": "Daft Punk",
                "collectionName": "Random Access Memories",
                "trackTimeMillis": 369000,
                "artworkUrl100": "https://is1.mzstatic.com/image/thumb/Music/100x100bb.jpg"
            },
            {
                "wrapperType": "collection",
                "collectionId": 1440833098,
                "collectionName": "Random Access Memories",
                "artistName": "Daft Punk"
            },
            {
                "wrapperType": "track",
                "trackId": 1440833101,
                "trackName": "Instant Crush",
                "trackTimeMillis": -5
            }
        ]
    })json");

    QCOMPARE(tracks.size(), 2);
    QCOMPARE(tracks.at(0).provider, QStringLiteral("apple"));
    QCOMPARE(tracks.at(0).id, QStringLiteral("1440833100"));
    QCOMPARE(tracks.at(0).artist, QStringLiteral("Daft Punk"));
    QCOMPARE(tracks.at(0).album, QStringLiteral("Random Access Memories"));
    QCOMPARE(tracks.at(0).durationSeconds, 369);
    QCOMPARE(tracks.at(0).artworkUrl,
             QStringLiteral("https://is1.mzstatic.com/image/thumb/Music/100x100bb.jpg"));
    QCOMPARE(tracks.at(1).durationSeconds, 0);
}

void MusicProvidersTest::readsAppleMusicApiResponse()
{
    const QVector<ProviderTrack> tracks = tracksFrom("apple", R"json({
        "data": [
            {
                "id": "1440833100",
                "type": "songs",
                "attributes": {
                    "name": "Get Lucky",
                    "artistName": "Daft Punk",
                    "albumName": "Random Access Memories",
                    "durationInMillis": 369000,
                    "artwork": {
                        "url": "https://is1.mzstatic.com/image/thumb/Music/{w}x{h}bb.jpg"
                    }
                }
            }
        ]
    })json");

    QCOMPARE(tracks.size(), 1);
    QCOMPARE(tracks.first().id, QStringLiteral("1440833100"));
    QCOMPARE(tracks.first().album, QStringLiteral("Random Access Memories"));
    QCOMPARE(tracks.first().durationSeconds, 369);
    QCOMPARE(tracks.first().artworkUrl,
             QStringLiteral("https://is1.mzstatic.com/image/thumb/Music/{w}x{h}bb.jpg"));
}

void MusicProvidersTest::readsSoundCloudResponse()
{
    const QVector<ProviderTrack> tracks = tracksFrom("soundcloud", R"json({
        "collection": [
            {
                "id": 123456,
                "title": "Get Lucky",
                "user": {"username": "Daft Punk"},
                "duration": 369000,
                "artwork_url": "https://i1.sndcdn.com/artworks-abc-large.jpg"
            },
            {
                "id": 123457,
                "title": "No artwork",
                "user": {"username": "A"},
                "duration": -5
            }
        ]
    })json");

    QCOMPARE(tracks.size(), 2);
    QCOMPARE(tracks.at(0).provider, QStringLiteral("soundcloud"));
    QCOMPARE(tracks.at(0).id, QStringLiteral("123456"));
    QCOMPARE(tracks.at(0).artist, QStringLiteral("Daft Punk"));
    QCOMPARE(tracks.at(0).durationSeconds, 369);
    QCOMPARE(tracks.at(0).artworkUrl,
             QStringLiteral("https://i1.sndcdn.com/artworks-abc-large.jpg"));
    QCOMPARE(tracks.at(1).durationSeconds, 0);
    QVERIFY(tracks.at(1).artworkUrl.isEmpty());
}

void MusicProvidersTest::readsYoutubeResponse()
{
    const QVector<ProviderTrack> tracks = tracksFrom("youtube", R"json({
        "items": [
            {
                "id": {"videoId": "dQw4w9WgXcQ"},
                "snippet": {
                    "title": "Never Gonna Give You Up",
                    "channelTitle": "Rick Astley",
                    "thumbnails": {
                        "high": {"url": "https://i.ytimg.com/vi/dQw4w9WgXcQ/hqdefault.jpg"}
                    }
                },
                "contentDetails": {"duration": "PT3M33S"}
            },
            {
                "id": {"videoId": "bad id"},
                "snippet": {"title": "Bad"}
            },
            {
                "id": {"videoId": "abc"},
                "snippet": {"title": "No duration"}
            }
        ]
    })json");

    QCOMPARE(tracks.size(), 2);
    QCOMPARE(tracks.at(0).provider, QStringLiteral("youtube"));
    QCOMPARE(tracks.at(0).id, QStringLiteral("dQw4w9WgXcQ"));
    QCOMPARE(tracks.at(0).artist, QStringLiteral("Rick Astley"));
    QCOMPARE(tracks.at(0).durationSeconds, 213);
    QCOMPARE(tracks.at(0).artworkUrl,
             QStringLiteral("https://i.ytimg.com/vi/dQw4w9WgXcQ/hqdefault.jpg"));
    QCOMPARE(tracks.at(1).durationSeconds, 0);
}

void MusicProvidersTest::readsGenericResponse()
{
    const QVector<ProviderTrack> tracks = tracksFrom("spotify", R"json({
        "tracks": [
            {
                "id": "t1",
                "title": "Song",
                "artist": "Artist",
                "album": "Album",
                "durationSeconds": 180,
                "artworkUrl": "https://example.com/a.jpg"
            },
            {
                "id": "t2",
                "title": "Clamped",
                "durationSeconds": -10
            }
        ]
    })json");

    QCOMPARE(tracks.size(), 2);
    QCOMPARE(tracks.at(0).id, QStringLiteral("t1"));
    QCOMPARE(tracks.at(0).artist, QStringLiteral("Artist"));
    QCOMPARE(tracks.at(0).album, QStringLiteral("Album"));
    QCOMPARE(tracks.at(0).durationSeconds, 180);
    QCOMPARE(tracks.at(0).artworkUrl, QStringLiteral("https://example.com/a.jpg"));
    QCOMPARE(tracks.at(1).durationSeconds, 0);
}

void MusicProvidersTest::skipsMalformedTracksAndClampsDuration()
{
    const QVector<ProviderTrack> tracks = tracksFrom("spotify", R"json({
        "items": [
            {"id": "missingTitle"},
            {"title": "Missing id"},
            {"id": "bad id", "title": "Unsafe id"},
            {"id": "ok", "title": "Huge", "durationSeconds": 999999999999},
            {"id": "ok2", "title": "Negative", "durationSeconds": -1}
        ]
    })json");

    QCOMPARE(tracks.size(), 2);
    QCOMPARE(tracks.at(0).durationSeconds, std::numeric_limits<int>::max());
    QCOMPARE(tracks.at(1).durationSeconds, 0);
}

void MusicProvidersTest::returnsNothingForUnknownProviderOrEmptyResponse()
{
    QVERIFY(providerTracksFromJson(QStringLiteral("tidal"), jsonObject(R"json({
        "tracks": [{"id": "1", "title": "Song"}]
    })json")).isEmpty());

    QVERIFY(providerTracksFromJson(QStringLiteral("spotify"), QJsonObject()).isEmpty());
    QVERIFY(providerTracksFromJson(QStringLiteral("yandex"), jsonObject(R"json({
        "result": {"tracks": {"results": []}}
    })json")).isEmpty());
}

QTEST_GUILESS_MAIN(MusicProvidersTest)
#include "tst_music_providers.moc"
