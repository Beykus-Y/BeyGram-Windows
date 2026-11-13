#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One track returned by an external music service.
///
/// provider is the canonical provider id ("yandex", "spotify", "apple",
/// "soundcloud" or "youtube"), id is the provider's own track id, and title,
/// artist, album, durationSeconds and artworkUrl are already-parsed metadata.
/// A text field may be empty; durationSeconds is never negative. Nothing here
/// touches TDLib or the network.
struct ProviderTrack {
    QString provider;
    QString id;
    QString title;
    QString artist;
    QString album;
    int durationSeconds = 0;
    QString artworkUrl;

    bool operator==(const ProviderTrack&) const = default;
};

/// A link to one item on an external music service.
///
/// provider is the canonical provider id, kind is "track", "album", "playlist"
/// or "artist", and id is the provider id of that item. A SoundCloud id is the
/// permalink path (for example "artist/track") because SoundCloud addresses
/// tracks by path rather than by a numeric id. An unrecognized or malformed
/// address yields an empty ProviderLink, i.e. every field is empty.
struct ProviderLink {
    QString provider;
    QString kind;
    QString id;

    bool operator==(const ProviderLink&) const = default;
};

/// Parses a web link of a supported music service.
///
/// The input is trimmed. An address without a scheme is completed with https,
/// as the Android parser does; after that only http and https are accepted.
/// Recognized forms are:
///
///   https://music.yandex.ru/album/<id>/track/<id>   -> track
///   https://music.yandex.ru/track/<id>              -> track
///   https://music.yandex.ru/album/<id>              -> album
///   https://music.yandex.ru/playlists/<uuid>        -> playlist
///   https://open.spotify.com/<kind>/<id>            -> track/album/playlist/artist
///   https://music.apple.com/<storefront>/album/<name>/<id> -> album
///   https://music.apple.com/.../album/.../<id>?i=<id>      -> track
///   https://music.apple.com/.../song|artist|playlist/.../<id> -> track/artist/playlist
///   https://soundcloud.com/<artist>/<track>         -> track
///   https://soundcloud.com/<artist>/sets/<name>     -> playlist
///   https://soundcloud.com/<artist>                 -> artist
///   https://music.youtube.com/watch?v=<id>          -> track
///   https://music.youtube.com/playlist?list=<id>    -> playlist
///   https://music.youtube.com/channel/<id>          -> artist
///
/// Query parameters (utm tags, Spotify's si, Apple's i and so on) never change
/// a route, except Apple's i, which turns an album page into its track. Hosts
/// are matched ignoring case and a leading "www."; Yandex Music is accepted on
/// its .ru, .com, .by, .kz and .uz hosts. Every id is validated to a safe
/// charset before it is returned, so a malformed or foreign link can never be
/// turned into a lookup or a share link. The function never logs.
[[nodiscard]] ProviderLink parseMusicProviderLink(const QString& url);

/// Returns the display name of a provider: "Yandex Music", "Spotify",
/// "Apple Music", "SoundCloud" or "YouTube Music". The argument is trimmed and
/// matched ignoring case; the canonical ids above and the aliases
/// "yandex_music", "apple_music", "youtube_music" and "itunes" are accepted.
/// An unknown provider yields an empty string.
[[nodiscard]] QString providerDisplayName(const QString& provider);

/// Reads the tracks of a provider response.
///
/// The known response shapes are accepted: Yandex Music (search results,
/// library tracks, album volumes and radio sequence), Spotify (tracks/items
/// pages and playlist item wrappers), Apple Music (iTunes results and Music API
/// data with attributes), SoundCloud (collection/tracks) and YouTube Music
/// (items with snippet/contentDetails). A generic tracks/items/results/
/// collection array is understood as a fallback.
///
/// Objects without a usable id or title are skipped, as are objects whose id
/// contains characters that are not safe in a URL. Durations written in
/// milliseconds are converted to whole seconds; negative or malformed
/// durations become 0 and values beyond the int range are clamped. Artwork
/// addresses are kept only when they are http(s); a Yandex coverUri without a
/// scheme is completed and its "%%" size placeholder becomes "200x200".
///
/// An unknown provider or a response without tracks yields an empty vector.
/// The input order is preserved and the function never throws or logs.
[[nodiscard]] QVector<ProviderTrack> providerTracksFromJson(const QString& provider,
                                                            const QJsonObject& response);

} // namespace beygram::telegram
