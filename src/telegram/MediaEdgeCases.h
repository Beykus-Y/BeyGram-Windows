#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// What one TDLib messageVideoNote actually carries.
///
/// The probe reads `video_note.video` (falling back to `video_note.file` for a
/// stored wrapper that uses that name), `waveform`, `duration`, the file id and
/// the square side from `length`. A `width`/`height` pair is preferred when the
/// wrapper carries it; `length` is the fallback for both.
///
/// hasVideo is true only when the video file object has a positive id, because
/// an id-less file cannot be requested from TDLib. hasWaveform is true when the
/// waveform string is non-empty (or, in a stored wrapper, when the waveform
/// array has entries). durationSeconds, width and height are never negative.
///
/// likelyLossy flags a note that claims a duration but cannot be rendered the
/// way a video note must be: it has no waveform, no video file, or both. A note
/// without a duration is not flagged, because there is nothing to compare the
/// missing parts against.
struct VideoNoteProbe {
    bool hasVideo = false;
    bool hasWaveform = false;
    int durationSeconds = 0;
    qint64 fileId = 0;
    int width = 0;
    int height = 0;
    bool likelyLossy = false;
};

/// Reads a message or a bare messageVideoNote content. Anything that is not a
/// messageVideoNote yields the default probe. Pure: no I/O, no logging.
[[nodiscard]] VideoNoteProbe videoNoteProbe(const QJsonObject& message);

/// The small overlay state of one media message.
///
/// kind is the mediaKindFromContent() value ("photo", "video", "animation",
/// "audio", "voice", "document", "sticker", "location", "contact", "poll" or
/// "none") and label is a short display name for it ("Photo", "GIF", ...).
/// The label never encodes the state below; the flags carry that state.
///
/// spoiler mirrors has_spoiler and is_secret: content TDLib hides until the
/// user opens it must be hidden on the badge too. expired is true for TDLib's
/// messageExpired* placeholders and for an explicit expired/is_expired flag.
/// ttlSeconds is the non-negative `ttl` (or `ttl_seconds`) value, zero when the
/// content carries none. An absent or malformed field keeps the safe default,
/// so a broken payload can never make hidden content look visible.
struct MediaBadge {
    QString kind;
    QString label;
    bool spoiler = false;
    bool expired = false;
    qint64 ttlSeconds = 0;
};

/// Builds the spoiler/expiry badge for a content object (or a message carrying
/// one under "content").
[[nodiscard]] MediaBadge mediaBadgeFromContent(const QJsonObject& content);

/// Maps a content object (or a message that carries one under "content") to one
/// of the fixed kind names above.
///
/// messageVideoNote and messageExpiredVideoNote map to "video", because the
/// fixed set has no separate video-note kind. The messageExpired* placeholders
/// keep the kind of the media they replaced, so the same card can explain what
/// expired. messageAnimatedEmoji is a sticker; venue and live-location content
/// are locations. Text, dice, paid media, invoices, calls, service events and
/// unknown content report "none": they have no downloadable attachment.
[[nodiscard]] QString mediaKindFromContent(const QJsonObject& content);

/// Whether the content's main file still has to be downloaded.
///
/// True for a file-backed kind whose primary file object has a positive id and
/// whose `local` block does not report a completed download with a non-empty
/// path. An active but unfinished download still needs downloading, so it stays
/// true. Location, contact, poll, text and unknown content, expired
/// placeholders and any content without a usable file id report false. Pure:
/// no I/O, no logging.
[[nodiscard]] bool mediaNeedsDownload(const QJsonObject& content);

} // namespace beygram::telegram
