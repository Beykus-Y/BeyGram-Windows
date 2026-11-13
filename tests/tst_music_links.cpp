#include "telegram/music/MusicLinks.h"

#include <QString>
#include <QtTest>

using namespace beygram::telegram;

class MusicLinksTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsYandexLinks();
    void buildsSpotifyLinks();
    void buildsAppleLinks();
    void buildsSoundCloudLinks();
    void buildsYoutubeLinks();
    void returnsEmptyForUnknownProvider();
    void returnsEmptyForBlankInputs();
    void trimsAndEncodesIds();
    void trimsAndEncodesQueries();
    void acceptsAliasesAndCase();
    void encodesSoundCloudPathsSafely();
};

void MusicLinksTest::buildsYandexLinks()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("yandex"), QStringLiteral("123")),
             QStringLiteral("https://music.yandex.ru/track/123"));
    QCOMPARE(providerAlbumUrl(QStringLiteral("yandex"), QStringLiteral("456")),
             QStringLiteral("https://music.yandex.ru/album/456"));
    QCOMPARE(musicSearchUrl(QStringLiteral("yandex"), QStringLiteral("daft punk")),
             QStringLiteral("https://music.yandex.ru/search?text=daft%20punk"));
}

void MusicLinksTest::buildsSpotifyLinks()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("spotify"),
                              QStringLiteral("4uLU6hMCjMI75M1A2tKUQC")),
             QStringLiteral("https://open.spotify.com/track/4uLU6hMCjMI75M1A2tKUQC"));
    QCOMPARE(providerAlbumUrl(QStringLiteral("spotify"),
                              QStringLiteral("4uLU6hMCjMI75M1A2tKUQC")),
             QStringLiteral("https://open.spotify.com/album/4uLU6hMCjMI75M1A2tKUQC"));
    QCOMPARE(musicSearchUrl(QStringLiteral("spotify"), QStringLiteral("daft punk")),
             QStringLiteral("https://open.spotify.com/search/daft%20punk"));
}

void MusicLinksTest::buildsAppleLinks()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("apple"), QStringLiteral("1440833100")),
             QStringLiteral("https://music.apple.com/song/1440833100"));
    QCOMPARE(providerAlbumUrl(QStringLiteral("apple"), QStringLiteral("1440833098")),
             QStringLiteral("https://music.apple.com/album/1440833098"));
    QCOMPARE(musicSearchUrl(QStringLiteral("apple"), QStringLiteral("daft punk")),
             QStringLiteral("https://music.apple.com/search?term=daft%20punk"));
}

void MusicLinksTest::buildsSoundCloudLinks()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"),
                              QStringLiteral("daftpunk/get-lucky")),
             QStringLiteral("https://soundcloud.com/daftpunk/get-lucky"));
    QCOMPARE(providerAlbumUrl(QStringLiteral("soundcloud"),
                              QStringLiteral("daftpunk/sets/random-access-memories")),
             QStringLiteral("https://soundcloud.com/daftpunk/sets/random-access-memories"));
    QCOMPARE(musicSearchUrl(QStringLiteral("soundcloud"), QStringLiteral("daft punk")),
             QStringLiteral("https://soundcloud.com/search?q=daft%20punk"));
}

void MusicLinksTest::buildsYoutubeLinks()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("youtube"), QStringLiteral("dQw4w9WgXcQ")),
             QStringLiteral("https://music.youtube.com/watch?v=dQw4w9WgXcQ"));
    QCOMPARE(providerAlbumUrl(QStringLiteral("youtube"), QStringLiteral("OLAK5uy_abc-123")),
             QStringLiteral("https://music.youtube.com/playlist?list=OLAK5uy_abc-123"));
    QCOMPARE(musicSearchUrl(QStringLiteral("youtube"), QStringLiteral("daft punk")),
             QStringLiteral("https://music.youtube.com/search?q=daft%20punk"));
}

void MusicLinksTest::returnsEmptyForUnknownProvider()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("tidal"), QStringLiteral("1")), QString());
    QCOMPARE(providerAlbumUrl(QStringLiteral("tidal"), QStringLiteral("1")), QString());
    QCOMPARE(musicSearchUrl(QStringLiteral("tidal"), QStringLiteral("song")), QString());
    QCOMPARE(providerTrackUrl(QString(), QStringLiteral("1")), QString());
    QCOMPARE(providerAlbumUrl(QString(), QStringLiteral("1")), QString());
    QCOMPARE(musicSearchUrl(QString(), QStringLiteral("song")), QString());
}

void MusicLinksTest::returnsEmptyForBlankInputs()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("spotify"), QString()), QString());
    QCOMPARE(providerTrackUrl(QStringLiteral("spotify"), QStringLiteral("   ")), QString());
    QCOMPARE(providerAlbumUrl(QStringLiteral("apple"), QStringLiteral("  ")), QString());
    QCOMPARE(musicSearchUrl(QStringLiteral("youtube"), QString()), QString());
    QCOMPARE(musicSearchUrl(QStringLiteral("youtube"), QStringLiteral("   ")), QString());
}

void MusicLinksTest::trimsAndEncodesIds()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("spotify"), QStringLiteral("  a b  ")),
             QStringLiteral("https://open.spotify.com/track/a%20b"));
    QCOMPARE(providerTrackUrl(QStringLiteral("yandex"), QStringLiteral("a/b")),
             QStringLiteral("https://music.yandex.ru/track/a%2Fb"));
    QCOMPARE(providerTrackUrl(QStringLiteral("apple"), QStringLiteral("100%")),
             QStringLiteral("https://music.apple.com/song/100%25"));
    QCOMPARE(providerTrackUrl(QStringLiteral("youtube"), QStringLiteral("a+b")),
             QStringLiteral("https://music.youtube.com/watch?v=a%2Bb"));
    QCOMPARE(providerTrackUrl(QStringLiteral("youtube"), QStringLiteral(" Björk ")),
             QStringLiteral("https://music.youtube.com/watch?v=Bj%C3%B6rk"));
}

void MusicLinksTest::trimsAndEncodesQueries()
{
    QCOMPARE(musicSearchUrl(QStringLiteral("spotify"), QStringLiteral("AC/DC & friends")),
             QStringLiteral("https://open.spotify.com/search/AC%2FDC%20%26%20friends"));
    QCOMPARE(musicSearchUrl(QStringLiteral("apple"), QStringLiteral("Björk")),
             QStringLiteral("https://music.apple.com/search?term=Bj%C3%B6rk"));
    QCOMPARE(musicSearchUrl(QStringLiteral("soundcloud"), QStringLiteral("100%")),
             QStringLiteral("https://soundcloud.com/search?q=100%25"));
    QCOMPARE(musicSearchUrl(QStringLiteral("youtube"), QStringLiteral("a+b")),
             QStringLiteral("https://music.youtube.com/search?q=a%2Bb"));
}

void MusicLinksTest::acceptsAliasesAndCase()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("  YANDEX_MUSIC "), QStringLiteral("1")),
             QStringLiteral("https://music.yandex.ru/track/1"));
    QCOMPARE(providerAlbumUrl(QStringLiteral("Apple_Music"), QStringLiteral("2")),
             QStringLiteral("https://music.apple.com/album/2"));
    QCOMPARE(providerTrackUrl(QStringLiteral("itunes"), QStringLiteral("3")),
             QStringLiteral("https://music.apple.com/song/3"));
    QCOMPARE(musicSearchUrl(QStringLiteral("YouTube_Music"), QStringLiteral("x")),
             QStringLiteral("https://music.youtube.com/search?q=x"));
    QCOMPARE(providerTrackUrl(QStringLiteral("ytmusic"), QStringLiteral("4")),
             QStringLiteral("https://music.youtube.com/watch?v=4"));
}

void MusicLinksTest::encodesSoundCloudPathsSafely()
{
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"),
                              QStringLiteral("artist/track name")),
             QStringLiteral("https://soundcloud.com/artist/track%20name"));
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"),
                              QStringLiteral("artist/AC~DC")),
             QStringLiteral("https://soundcloud.com/artist/AC~DC"));
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"), QStringLiteral("artist")),
             QStringLiteral("https://soundcloud.com/artist"));
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"), QStringLiteral("artist/track/")),
             QStringLiteral("https://soundcloud.com/artist/track"));
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"), QStringLiteral("artist/track?x")),
             QStringLiteral("https://soundcloud.com/artist/track%3Fx"));
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"), QStringLiteral("artist/track#x")),
             QStringLiteral("https://soundcloud.com/artist/track%23x"));
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"), QStringLiteral("artist//track")),
             QString());
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"), QStringLiteral("/artist/track")),
             QString());
    QCOMPARE(providerTrackUrl(QStringLiteral("soundcloud"), QStringLiteral("artist/../track")),
             QString());
}

QTEST_GUILESS_MAIN(MusicLinksTest)
#include "tst_music_links.moc"
