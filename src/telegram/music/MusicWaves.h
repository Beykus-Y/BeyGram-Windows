#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One wave (a radio station) of an external music service.
///
/// id is the provider's own wave id and is never empty in a parsed list.
/// title and subtitle are display texts; both are trimmed, and either may be
/// empty when the service did not send it. artworkUrl is the cover address as
/// sent (trimmed), or a scheme-less Yandex coverUri completed to https.
/// provider is the canonical provider id: the wave's own provider when present,
/// otherwise the response's provider, otherwise "yandex".
struct MusicWave {
    QString id;
    QString title;
    QString subtitle;
    QString artworkUrl;
    QString provider;
};

/// Reads the waves of a music-service response.
///
/// The response may carry the list at the top level or inside a "result"
/// object/array, under "waves", "items", "results" or "stations". One nested
/// "items"/"waves" wrapper and a top-level or result-level "data"/"payload"
/// wrapper are accepted too. Each entry may be the wave itself or wrap it in
/// "wave" or "station". An entry without a usable id is dropped; a repeated id
/// keeps its first position. Text fields are trimmed. The input order is
/// otherwise preserved and the function never throws or logs.
[[nodiscard]] QVector<MusicWave> musicWavesFromJson(const QJsonObject& response);

/// Builds the JSON body of a request for the tracks of one wave.
///
/// waveId is trimmed; a blank id yields an empty object, which callers must
/// treat as "nothing to request". limit is clamped to 1..100. The body is
/// {"waveId": <id>, "limit": <limit>}.
[[nodiscard]] QJsonObject musicWaveTracksRequest(const QString& waveId, int limit);

/// One track address resolved from a downloader/info response.
///
/// trackId and provider identify the track and its service (provider may be
/// empty when the response does not name one). streamUrl is the best address
/// found: the first https address among the known URL fields, otherwise the
/// first http one. playable is true only when streamUrl is a non-empty
/// absolute https address with a host; a caller must check it before playing.
/// durationSeconds is the whole length in seconds, clamped to 0..INT_MAX.
struct ResolvedTrack {
    QString trackId;
    QString provider;
    QString streamUrl;
    int durationSeconds = 0;
    bool playable = false;
};

/// Reads a Yandex downloader/info response.
///
/// Known URL fields are stream_url, url, download_url, file and direct_link
/// (and their camelCase forms), searched in the response and in the usual
/// wrappers (result, data, track, audio, downloader, download, info). A https
/// address is preferred over an earlier http one. provider defaults to
/// "yandex" when the response does not name it.
[[nodiscard]] ResolvedTrack resolveYandexTrack(const QJsonObject& response);

/// Reads a generic lazy-source response with the same field rules as
/// resolveYandexTrack(). provider is left empty when the response does not
/// name one, because the source is not known here.
[[nodiscard]] ResolvedTrack resolveLazyTrack(const QJsonObject& response);

} // namespace beygram::telegram
