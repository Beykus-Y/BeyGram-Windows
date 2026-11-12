#include "MediaFileState.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

constexpr int CompletePercent = 100;
constexpr int HighestIncompletePercent = CompletePercent - 1;

/// File ids are TDLib int32 values and may arrive as JSON numbers or strings;
/// anything outside that range means "no file" instead of a truncated id.
int fileIdFromJson(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

qint64 nonNegative(qint64 value)
{
    return value > 0 ? value : 0;
}

/// TDLib reports the real size in expected_size; `size` is the deprecated
/// field and may still be the only one a stored update carries.
qint64 expectedSizeFromFile(const QJsonObject& file)
{
    const qint64 expected = jsonInt64(file.value(QStringLiteral("expected_size")));
    if (expected > 0)
        return expected;
    return nonNegative(jsonInt64(file.value(QStringLiteral("size"))));
}

/// TDLib reports transfer errors as `{"code": ..., "message": ...}`; a bare
/// non-empty string is accepted too so a wrapper's short form is not lost.
bool hasError(const QJsonObject& object)
{
    const QJsonValue error = object.value(QStringLiteral("error"));
    if (error.isString())
        return !error.toString().isEmpty();
    if (!error.isObject())
        return false;

    const QJsonObject details = error.toObject();
    if (!details.value(QStringLiteral("message")).toString().isEmpty())
        return true;
    return jsonInt64(details.value(QStringLiteral("code"))) != 0;
}

} // namespace

MediaFileState mediaFileStateFromUpdate(const QJsonObject& updateFile)
{
    MediaFileState state;
    state.fileId = fileIdFromJson(updateFile.value(QStringLiteral("id")));
    // Without an id the snapshot cannot be attributed to a file, so nothing
    // else in the update is worth reporting.
    if (state.fileId == 0)
        return state;

    const QJsonObject local = updateFile.value(QStringLiteral("local")).toObject();
    state.size = expectedSizeFromFile(updateFile);
    state.downloaded = nonNegative(jsonInt64(local.value(QStringLiteral("downloaded_size"))));
    if (state.size > 0 && state.downloaded > state.size)
        state.downloaded = state.size;

    // A failed transfer keeps its progress numbers so the caller can show how
    // far it got, but never exposes the partial file's path.
    if (hasError(updateFile) || hasError(local)) {
        state.state = DownloadState::Failed;
        return state;
    }

    if (local.value(QStringLiteral("is_downloading_completed")).toBool()) {
        state.state = DownloadState::Ready;
        state.path = local.value(QStringLiteral("path")).toString();
        // A completed download has no bytes left, even if downloaded_size was
        // left stale by an earlier update.
        if (state.size > 0)
            state.downloaded = state.size;
        return state;
    }

    if (local.value(QStringLiteral("is_downloading_active")).toBool()) {
        state.state = DownloadState::Downloading;
        return state;
    }

    // Not running and not finished: bytes already on disk mean the transfer was
    // started and is waiting to resume, no bytes at all mean it was never
    // requested from this snapshot's point of view.
    state.state = state.downloaded > 0 ? DownloadState::Pending : DownloadState::Idle;
    return state;
}

int progressPercent(const MediaFileState& state)
{
    switch (state.state) {
    case DownloadState::Ready:
        return CompletePercent;
    case DownloadState::Failed:
        return 0;
    case DownloadState::Idle:
    case DownloadState::Pending:
    case DownloadState::Downloading:
        break;
    }

    if (state.size <= 0 || state.downloaded <= 0)
        return 0;

    const double ratio =
        100.0 * static_cast<double>(state.downloaded) / static_cast<double>(state.size);
    const int percent = static_cast<int>(ratio);
    return qBound(0, percent, HighestIncompletePercent);
}

} // namespace beygram::telegram
