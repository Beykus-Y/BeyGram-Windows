#include "MusicSmartCollections.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

constexpr int kRecentLimit = 30;
constexpr int kLongTrackSeconds = 600;
constexpr int kShortTrackSeconds = 120;

template <typename Predicate>
QVector<QString> collectMatchingIds(const QVector<MusicTrackModel>& tracks,
                                    Predicate predicate)
{
    QVector<QString> ids;
    ids.reserve(tracks.size());

    QSet<QString> seen;
    seen.reserve(tracks.size());

    for (const MusicTrackModel& track : tracks) {
        if (!predicate(track))
            continue;

        const QString id = track.id.trimmed();
        if (id.isEmpty() || seen.contains(id))
            continue;

        seen.insert(id);
        ids.append(id);
    }

    return ids;
}

// Scans backwards so the newest 30 distinct tracks survive, then restores the
// order they were given in.
QVector<QString> recentIds(const QVector<MusicTrackModel>& tracks)
{
    const qsizetype limit = std::min<qsizetype>(tracks.size(), kRecentLimit);

    QVector<QString> ids;
    ids.reserve(limit);

    QSet<QString> seen;
    seen.reserve(limit);

    for (qsizetype index = tracks.size() - 1; index >= 0 && ids.size() < limit;
         --index) {
        const QString id = tracks.at(index).id.trimmed();
        if (id.isEmpty() || seen.contains(id))
            continue;

        seen.insert(id);
        ids.prepend(id);
    }

    return ids;
}

} // namespace

QVector<SmartCollection> smartCollections(const QVector<MusicTrackModel>& tracks,
                                          qint64)
{
    QVector<SmartCollection> collections;
    collections.reserve(4);

    const auto add = [&collections](const QString& id, const QString& title,
                                    QVector<QString> trackIds) {
        if (trackIds.isEmpty())
            return;
        collections.append(SmartCollection{id, title, std::move(trackIds)});
    };

    add(QStringLiteral("recent"), QStringLiteral("Recent"), recentIds(tracks));
    add(QStringLiteral("long"), QStringLiteral("Long tracks"),
        collectMatchingIds(tracks, [](const MusicTrackModel& track) {
            return track.durationSeconds >= kLongTrackSeconds;
        }));
    add(QStringLiteral("short"), QStringLiteral("Short tracks"),
        collectMatchingIds(tracks, [](const MusicTrackModel& track) {
            return track.durationSeconds <= kShortTrackSeconds;
        }));
    add(QStringLiteral("no-album"), QStringLiteral("No album"),
        collectMatchingIds(tracks, [](const MusicTrackModel& track) {
            return track.albumId.trimmed().isEmpty();
        }));

    return collections;
}

} // namespace beygram::telegram
