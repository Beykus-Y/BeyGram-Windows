#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One member of an outgoing media album as selected by the composer. localPath
/// points at a local file; video selects inputMessageVideo instead of
/// inputMessagePhoto. width/height may stay 0, in which case TDLib fills them
/// in from the uploaded file. durationSeconds is only meaningful for videos,
/// but every item is validated against the same 0..600 range.
struct AlbumSendItem {
    QString localPath;
    QString caption;
    bool video = false;
    int width = 0;
    int height = 0;
    int durationSeconds = 0;
};

/// An album the user is about to send. replyTo is a message id; zero or any
/// negative value means "not a reply".
struct AlbumSendPlan {
    qint64 chatId = 0;
    QVector<AlbumSendItem> items;
    qint64 replyTo = 0;
};

/// Returns an empty string when the plan can be sent, otherwise one stable
/// reason code:
///   "no-chat"          chatId is 0
///   "too-few"          fewer than 2 items
///   "too-many"         more than 10 items
///   "empty-path"       an item's localPath is empty or only whitespace
///   "caption-too-long" an item's caption exceeds 1024 UTF-16 units
///   "bad-size"         an item's width or height is negative
///   "bad-duration"     an item's durationSeconds is outside 0..600
/// Chat is checked before the item count; the remaining checks run per item in
/// plan order.
[[nodiscard]] QString validateAlbumPlan(const AlbumSendPlan& plan);

/// Builds the TDLib request for a valid plan: one sendMessageAlbum with an
/// inputMessagePhoto or inputMessageVideo per item, in plan order, and the
/// caption on the first item only. The result holds that single request; it is
/// a vector so album sends follow the same batch shape as the other request
/// builders. Returns an empty vector when validateAlbumPlan() rejects the plan.
///
/// The shared album builder has no duration parameter, so the planner writes
/// each video's durationSeconds into inputVideo.duration after building the
/// request; everything else (paths, dimensions, reply target, caption
/// placement) comes from sendAlbumRequest(). No file access, no logging and no
/// @extra; the transport adds correlation.
[[nodiscard]] QVector<QJsonObject> albumSendRequests(const AlbumSendPlan& plan);

} // namespace beygram::telegram
