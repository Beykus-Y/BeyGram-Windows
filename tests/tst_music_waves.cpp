#include "telegram/music/MusicWaves.h"

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

QVector<MusicWave> wavesFrom(const char* text)
{
    return musicWavesFromJson(jsonObject(text));
}

} // namespace

class MusicWavesTest final : public QObject {
    Q_OBJECT

private slots:
    void readsWavesFromResult();
    void readsTopLevelAndNestedArrays();
    void dropsEmptyIdsAndKeepsFirstDuplicate();
    void trimsTextAndCompletesYandexCoverUri();
    void ignoresMalformedEntriesAndEmptyResponses();
    void usesResponseProviderForWaves();
    void buildsWaveTracksRequest();

    void resolvesYandexDownloaderResponse();
    void prefersHttpsAndKeepsHttpUnplayable();
    void rejectsNonWebAndMalformedUrls();
    void clampsDurations();
    void resolvesNestedLazyResponse();
    void readsEveryUrlFieldAndUrlObject();
};

void MusicWavesTest::readsWavesFromResult()
{
    const QVector<MusicWave> waves = wavesFrom(R"json({
        "result": {
            "waves": [
                {
                    "id": "  my-wave  ",
                    "title": "  My Wave  ",
                    "subtitle": "  Made for you  ",
                    "artworkUrl": "  https://cdn.example/wave.png  ",
                    "provider": "yandex_music"
                },
                {
                    "waveId": 42,
                    "name": "  Genre wave  ",
                    "description": "  Pop  ",
                    "image": {"url": "https://cdn.example/genre.png"}
                }
            ]
        }
    })json");

    QCOMPARE(waves.size(), 2);

    QCOMPARE(waves.at(0).id, QStringLiteral("my-wave"));
    QCOMPARE(waves.at(0).title, QStringLiteral("My Wave"));
    QCOMPARE(waves.at(0).subtitle, QStringLiteral("Made for you"));
    QCOMPARE(waves.at(0).artworkUrl, QStringLiteral("https://cdn.example/wave.png"));
    QCOMPARE(waves.at(0).provider, QStringLiteral("yandex"));

    QCOMPARE(waves.at(1).id, QStringLiteral("42"));
    QCOMPARE(waves.at(1).title, QStringLiteral("Genre wave"));
    QCOMPARE(waves.at(1).subtitle, QStringLiteral("Pop"));
    QCOMPARE(waves.at(1).artworkUrl, QStringLiteral("https://cdn.example/genre.png"));
    QCOMPARE(waves.at(1).provider, QStringLiteral("yandex"));
}

void MusicWavesTest::readsTopLevelAndNestedArrays()
{
    QCOMPARE(wavesFrom(R"json({"waves":[{"id":"a","title":"A"}]})json").size(), 1);
    QCOMPARE(wavesFrom(R"json({"result":{"items":[{"id":"b","title":"B"}]}})json").size(), 1);
    QCOMPARE(wavesFrom(R"json({"result":{"waves":{"items":[{"id":"c","title":"C"}]}}})json").size(), 1);
    QCOMPARE(wavesFrom(R"json({"result":[{"id":"d","title":"D"}]})json").size(), 1);
    QCOMPARE(wavesFrom(R"json({"result":{"data":{"stations":[{"id":"e","title":"E"}]}}})json").size(), 1);
    QCOMPARE(wavesFrom(R"json({"result":{"waves":[{"wave":{"id":"f","title":"F"}}]}})json").size(), 1);
    QCOMPARE(wavesFrom(R"json({"result":{"stations":[{"station":{"id":"g","title":"G"}}]}})json").size(), 1);
    QCOMPARE(wavesFrom(R"json({"data":{"waves":[{"id":"h","title":"H"}]}})json").size(), 1);
    QVERIFY(wavesFrom(R"json({"result":{"waves":[]}})json").isEmpty());
}

void MusicWavesTest::dropsEmptyIdsAndKeepsFirstDuplicate()
{
    const QVector<MusicWave> waves = wavesFrom(R"json({
        "waves": [
            {"id": "  ", "title": "Blank"},
            {"id": "one", "title": " First "},
            {"id": "one", "title": "Second"},
            {"title": "No id"},
            {"id": 7, "title": "Seven"},
            {"id": " one ", "title": "Spaced duplicate"},
            {"id": "one", "title": "Third"}
        ]
    })json");

    QCOMPARE(waves.size(), 2);
    QCOMPARE(waves.at(0).id, QStringLiteral("one"));
    QCOMPARE(waves.at(0).title, QStringLiteral("First"));
    QCOMPARE(waves.at(1).id, QStringLiteral("7"));
    QCOMPARE(waves.at(1).title, QStringLiteral("Seven"));
}

void MusicWavesTest::trimsTextAndCompletesYandexCoverUri()
{
    const QVector<MusicWave> waves = wavesFrom(R"json({
        "waves": [
            {"id": "a", "title": "  Title  ", "subtitle": "  Sub  ", "coverUri": "avatars.example/%%"},
            {"id": "b", "title": "   ", "artwork_url": "  https://x/y  ", "provider": "  Spotify  "}
        ]
    })json");

    QCOMPARE(waves.size(), 2);
    QCOMPARE(waves.at(0).title, QStringLiteral("Title"));
    QCOMPARE(waves.at(0).subtitle, QStringLiteral("Sub"));
    QCOMPARE(waves.at(0).artworkUrl, QStringLiteral("https://avatars.example/200x200"));
    QCOMPARE(waves.at(0).provider, QStringLiteral("yandex"));

    QCOMPARE(waves.at(1).title, QString());
    QCOMPARE(waves.at(1).artworkUrl, QStringLiteral("https://x/y"));
    QCOMPARE(waves.at(1).provider, QStringLiteral("spotify"));
}

void MusicWavesTest::ignoresMalformedEntriesAndEmptyResponses()
{
    const QVector<MusicWave> waves = wavesFrom(R"json({
        "result": {"waves": [null, 3, "x", [], {"id": "ok", "title": "Ok"}]}
    })json");

    QCOMPARE(waves.size(), 1);
    QCOMPARE(waves.at(0).id, QStringLiteral("ok"));

    QVERIFY(musicWavesFromJson(QJsonObject()).isEmpty());
    QVERIFY(wavesFrom(R"json({"result":{"tracks":[]}})json").isEmpty());
    QVERIFY(wavesFrom(R"json({"waves":[{"id":""}]})json").isEmpty());
}

void MusicWavesTest::usesResponseProviderForWaves()
{
    const QVector<MusicWave> waves = wavesFrom(R"json({
        "provider": "spotify",
        "waves": [
            {"id": "a"},
            {"id": "b", "provider": "apple_music"},
            {"id": "c", "provider": 3}
        ]
    })json");

    QCOMPARE(waves.size(), 3);
    QCOMPARE(waves.at(0).provider, QStringLiteral("spotify"));
    QCOMPARE(waves.at(1).provider, QStringLiteral("apple"));
    QCOMPARE(waves.at(2).provider, QStringLiteral("spotify"));
}

void MusicWavesTest::buildsWaveTracksRequest()
{
    const QJsonObject request = musicWaveTracksRequest(QStringLiteral("  wave-1  "), 25);
    QCOMPARE(request.size(), 2);
    QCOMPARE(request.value(QStringLiteral("waveId")).toString(), QStringLiteral("wave-1"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 25);

    QCOMPARE(musicWaveTracksRequest(QStringLiteral("wave"), 0)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(musicWaveTracksRequest(QStringLiteral("wave"), -5)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(musicWaveTracksRequest(QStringLiteral("wave"), 101)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
    QCOMPARE(musicWaveTracksRequest(QStringLiteral("wave"), 1000)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);

    QVERIFY(musicWaveTracksRequest(QStringLiteral("   "), 10).isEmpty());
    QVERIFY(musicWaveTracksRequest(QString(), 10).isEmpty());
}

void MusicWavesTest::resolvesYandexDownloaderResponse()
{
    const ResolvedTrack track = resolveYandexTrack(jsonObject(R"json({
        "result": {
            "trackId": " 12345 ",
            "stream_url": "https://cdn.example/audio.mp3",
            "durationMs": 245500
        }
    })json"));

    QCOMPARE(track.trackId, QStringLiteral("12345"));
    QCOMPARE(track.provider, QStringLiteral("yandex"));
    QCOMPARE(track.streamUrl, QStringLiteral("https://cdn.example/audio.mp3"));
    QCOMPARE(track.durationSeconds, 245);
    QVERIFY(track.playable);
}

void MusicWavesTest::prefersHttpsAndKeepsHttpUnplayable()
{
    const ResolvedTrack secure = resolveYandexTrack(jsonObject(R"json({
        "track_id": "1",
        "url": "http://cdn.example/audio.mp3",
        "direct_link": "https://cdn.example/secure.mp3"
    })json"));

    QCOMPARE(secure.trackId, QStringLiteral("1"));
    QCOMPARE(secure.streamUrl, QStringLiteral("https://cdn.example/secure.mp3"));
    QVERIFY(secure.playable);

    const ResolvedTrack insecure = resolveLazyTrack(jsonObject(R"json({
        "id": "2",
        "file": "http://cdn.example/audio.mp3"
    })json"));

    QCOMPARE(insecure.trackId, QStringLiteral("2"));
    QCOMPARE(insecure.streamUrl, QStringLiteral("http://cdn.example/audio.mp3"));
    QVERIFY(insecure.provider.isEmpty());
    QVERIFY(!insecure.playable);
}

void MusicWavesTest::rejectsNonWebAndMalformedUrls()
{
    const auto resolve = [](const QString& url) {
        QJsonObject response;
        response.insert(QStringLiteral("stream_url"), url);
        return resolveYandexTrack(response);
    };

    for (const char* url : {"ftp://cdn.example/a.mp3", "https://", "not a url", "",
                            "//cdn.example/a.mp3"}) {
        const ResolvedTrack track = resolve(QString::fromLatin1(url));
        QVERIFY2(!track.playable, url);
        QVERIFY2(track.streamUrl.isEmpty(), url);
    }

    QVERIFY(resolve(QStringLiteral("HTTPS://CDN.EXAMPLE/A.MP3")).playable);
}

void MusicWavesTest::clampsDurations()
{
    QJsonObject response;
    response.insert(QStringLiteral("durationSeconds"), -12);
    QCOMPARE(resolveYandexTrack(response).durationSeconds, 0);

    response.insert(QStringLiteral("durationSeconds"), 999999999999.0);
    QCOMPARE(resolveYandexTrack(response).durationSeconds, std::numeric_limits<int>::max());

    response.insert(QStringLiteral("durationSeconds"), QStringLiteral(" 42.9 "));
    QCOMPARE(resolveYandexTrack(response).durationSeconds, 42);

    QJsonObject milliseconds;
    milliseconds.insert(QStringLiteral("duration_ms"), 1500);
    QCOMPARE(resolveYandexTrack(milliseconds).durationSeconds, 1);

    milliseconds.insert(QStringLiteral("duration_ms"), 1.0e15);
    QCOMPARE(resolveYandexTrack(milliseconds).durationSeconds, std::numeric_limits<int>::max());

    milliseconds.insert(QStringLiteral("duration_ms"), -1500);
    QCOMPARE(resolveYandexTrack(milliseconds).durationSeconds, 0);
}

void MusicWavesTest::resolvesNestedLazyResponse()
{
    const ResolvedTrack track = resolveLazyTrack(jsonObject(R"json({
        "result": {
            "track": {"id": "track-9", "provider": "soundcloud"},
            "downloader": {
                "download_url": "https://cdn.example/9.mp3",
                "duration": 180
            }
        }
    })json"));

    QCOMPARE(track.trackId, QStringLiteral("track-9"));
    QCOMPARE(track.provider, QStringLiteral("soundcloud"));
    QCOMPARE(track.streamUrl, QStringLiteral("https://cdn.example/9.mp3"));
    QCOMPARE(track.durationSeconds, 180);
    QVERIFY(track.playable);

    const ResolvedTrack empty = resolveLazyTrack(QJsonObject());
    QVERIFY(empty.trackId.isEmpty());
    QVERIFY(empty.provider.isEmpty());
    QVERIFY(empty.streamUrl.isEmpty());
    QCOMPARE(empty.durationSeconds, 0);
    QVERIFY(!empty.playable);
}

void MusicWavesTest::readsEveryUrlFieldAndUrlObject()
{
    struct UrlCase {
        const char* field;
        const char* url;
    };

    const UrlCase cases[] = {
        {"stream_url", "https://cdn.example/1.mp3"},
        {"url", "https://cdn.example/2.mp3"},
        {"download_url", "https://cdn.example/3.mp3"},
        {"file", "https://cdn.example/4.mp3"},
        {"direct_link", "https://cdn.example/5.mp3"},
    };

    for (const UrlCase& testCase : cases) {
        QJsonObject response;
        response.insert(QString::fromLatin1(testCase.field), QString::fromLatin1(testCase.url));
        const ResolvedTrack track = resolveYandexTrack(response);
        QCOMPARE(track.streamUrl, QString::fromLatin1(testCase.url));
        QVERIFY(track.playable);
    }

    const ResolvedTrack objectUrl = resolveYandexTrack(jsonObject(R"json({
        "trackId": "x",
        "file": {"url": "https://cdn.example/object.mp3"}
    })json"));

    QCOMPARE(objectUrl.streamUrl, QStringLiteral("https://cdn.example/object.mp3"));
    QVERIFY(objectUrl.playable);

    const ResolvedTrack mirrors = resolveYandexTrack(jsonObject(R"json({
        "trackId": "y",
        "download_url": ["http://cdn.example/old.mp3", "https://cdn.example/new.mp3"]
    })json"));

    QCOMPARE(mirrors.streamUrl, QStringLiteral("https://cdn.example/new.mp3"));
    QVERIFY(mirrors.playable);
}

QTEST_GUILESS_MAIN(MusicWavesTest)
#include "tst_music_waves.moc"
