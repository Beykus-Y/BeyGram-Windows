#include "AlbumSendPlanner.h"

#include "AlbumSend.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace beygram::telegram {
namespace {

constexpr int kMinAlbumItems = 2;
constexpr int kMaxAlbumItems = 10;
constexpr int kMaxCaptionLength = 1024;
constexpr int kMaxDurationSeconds = 600;

/// Checks one item in the same order as the documented reason codes. A blank
/// path is rejected here even though the shared builder also rejects it, so
/// validateAlbumPlan() stays the single source of truth for callers.
QString itemValidationReason(const AlbumSendItem& item)
{
    if (item.localPath.trimmed().isEmpty())
        return QStringLiteral("empty-path");
    if (item.caption.size() > kMaxCaptionLength)
        return QStringLiteral("caption-too-long");
    if (item.width < 0 || item.height < 0)
        return QStringLiteral("bad-size");
    if (item.durationSeconds < 0 || item.durationSeconds > kMaxDurationSeconds)
        return QStringLiteral("bad-duration");
    return {};
}

AlbumItem toAlbumItem(const AlbumSendItem& item)
{
    AlbumItem albumItem;
    albumItem.path = item.localPath;
    albumItem.caption = item.caption;
    albumItem.width = item.width;
    albumItem.height = item.height;
    albumItem.video = item.video;
    return albumItem;
}

/// sendAlbumRequest() always emits inputVideo.duration = 0 because AlbumItem
/// carries no duration. The planner owns that field: contents keep plan order,
/// so each video can be patched at its own index without searching by type.
void applyVideoDurations(QJsonObject& request, const QVector<AlbumSendItem>& items)
{
    QJsonArray contents = request.value(QStringLiteral("input_message_contents")).toArray();
    const qsizetype count = qMin(items.size(), contents.size());
    for (qsizetype index = 0; index < count; ++index) {
        if (!items.at(index).video)
            continue;

        QJsonObject content = contents.at(index).toObject();
        QJsonObject video = content.value(QStringLiteral("video")).toObject();
        video.insert(QStringLiteral("duration"), items.at(index).durationSeconds);
        content.insert(QStringLiteral("video"), video);
        contents.replace(index, content);
    }
    request.insert(QStringLiteral("input_message_contents"), contents);
}

} // namespace

QString validateAlbumPlan(const AlbumSendPlan& plan)
{
    if (plan.chatId == 0)
        return QStringLiteral("no-chat");
    if (plan.items.size() < kMinAlbumItems)
        return QStringLiteral("too-few");
    if (plan.items.size() > kMaxAlbumItems)
        return QStringLiteral("too-many");

    for (const AlbumSendItem& item : plan.items) {
        const QString reason = itemValidationReason(item);
        if (!reason.isEmpty())
            return reason;
    }
    return {};
}

QVector<QJsonObject> albumSendRequests(const AlbumSendPlan& plan)
{
    if (!validateAlbumPlan(plan).isEmpty())
        return {};

    QVector<AlbumItem> items;
    items.reserve(plan.items.size());
    for (const AlbumSendItem& item : plan.items)
        items.append(toAlbumItem(item));

    QJsonObject request = sendAlbumRequest(plan.chatId, items, plan.replyTo);
    if (request.isEmpty())
        return {};

    applyVideoDurations(request, plan.items);
    return {request};
}

} // namespace beygram::telegram
