#pragma once

#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Builds the public web address of a provider track.
///
/// The provider is trimmed and matched ignoring case; the canonical ids are
/// "yandex", "spotify", "apple", "soundcloud" and "youtube", and the aliases
/// "yandex_music", "apple_music", "youtube_music" and "itunes" are accepted.
/// The id is trimmed and percent-encoded. An unknown provider or an empty id
/// yields an empty string.
///
/// SoundCloud addresses tracks by permalink path, so an id may contain slashes
/// ("artist/track"). Each path segment is validated and encoded separately; a
/// path with an empty, "." or ".." segment yields an empty string. Every other
/// provider treats the id as one opaque path or query value.
[[nodiscard]] QString providerTrackUrl(const QString& provider, const QString& id);

/// Builds the public web address of a provider album.
///
/// The rules are the same as providerTrackUrl(). For SoundCloud, whose albums
/// are sets, pass the set path ("artist/sets/name"). YouTube Music album ids
/// are playlist ids and open through the playlist page. An unknown provider or
/// an empty id yields an empty string.
[[nodiscard]] QString providerAlbumUrl(const QString& provider, const QString& id);

/// Builds a search address for a provider.
///
/// The provider is trimmed and matched ignoring case. The query is trimmed and
/// percent-encoded; a query that is empty after trimming, or an unknown
/// provider, yields an empty string. The returned address always starts with
/// https.
[[nodiscard]] QString musicSearchUrl(const QString& provider, const QString& query);

} // namespace beygram::telegram
