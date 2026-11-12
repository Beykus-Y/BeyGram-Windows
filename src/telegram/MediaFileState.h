#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Where a TDLib file transfer stands, as far as a progress indicator cares.
/// Idle means nothing is known to be downloading, Pending a transfer that was
/// started but is not running right now, Downloading an active transfer, Ready
/// a completed file, and Failed a transfer TDLib reported an error for.
enum class DownloadState { Idle, Pending, Downloading, Ready, Failed };

/// Snapshot of one TDLib file object (`updateFile` payload). fileId is the
/// TDLib int32 id and stays 0 when the update carries no usable id; size is the
/// expected size in bytes and stays 0 while TDLib has not reported it;
/// downloaded is never negative and never exceeds a known size; path is only
/// filled for a Ready file, because the path of an incomplete download points
/// at partial data.
struct MediaFileState {
    int fileId = 0;
    DownloadState state = DownloadState::Idle;
    qint64 downloaded = 0;
    qint64 size = 0;
    QString path;
};

/// Reads a TDLib file object: `expected_size` (falling back to the deprecated
/// `size`), and the `local` block with `is_downloading_active`,
/// `is_downloading_completed`, `downloaded_size` and `path`. An `error` object
/// or a non-empty `error` string on the file or on its `local` block marks the
/// transfer Failed, which wins over every other flag. A completed transfer is
/// Ready and counts as fully downloaded. An update without a positive id
/// yields a default (Idle, fileId 0) state. The function is pure: no I/O, no
/// logging, no side effects.
[[nodiscard]] MediaFileState mediaFileStateFromUpdate(const QJsonObject& updateFile);

/// Progress of a transfer as a whole percentage. Ready is 100 and Failed is 0.
/// The other states report downloaded/size truncated to 0..99, so a running
/// transfer never looks finished; an unknown size (or no bytes yet) reports 0.
[[nodiscard]] int progressPercent(const MediaFileState& state);

} // namespace beygram::telegram
