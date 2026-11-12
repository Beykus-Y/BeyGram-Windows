#include "MediaPlaylistModel.h"

#include <QSet>

#include <utility>

namespace beygram::telegram {
namespace {

bool sameEntry(const PlaylistEntry& left, const PlaylistEntry& right)
{
    return left.messageId == right.messageId && left.chatId == right.chatId
        && left.title == right.title && left.durationSeconds == right.durationSeconds;
}

bool sameEntries(const QVector<PlaylistEntry>& left, const QVector<PlaylistEntry>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype index = 0; index < left.size(); ++index) {
        if (!sameEntry(left.at(index), right.at(index)))
            return false;
    }
    return true;
}

/// A message id identifies an entry, so a repeated id keeps its first copy and
/// an entry that cannot address a message (non-positive message id or chat id)
/// is dropped.
QVector<PlaylistEntry> cleanEntries(const QVector<PlaylistEntry>& entries)
{
    QVector<PlaylistEntry> kept;
    kept.reserve(entries.size());

    QSet<qint64> seenMessageIds;
    seenMessageIds.reserve(entries.size());

    for (const PlaylistEntry& entry : entries) {
        if (entry.messageId <= 0 || entry.chatId <= 0)
            continue;
        if (seenMessageIds.contains(entry.messageId))
            continue;
        seenMessageIds.insert(entry.messageId);
        kept.append(entry);
    }

    return kept;
}

} // namespace

MediaPlaylistModel::MediaPlaylistModel(QObject* parent) : QObject(parent)
{
}

void MediaPlaylistModel::setEntries(QVector<PlaylistEntry> entries)
{
    QVector<PlaylistEntry> next = cleanEntries(entries);
    // Replacing the queue with the same entries still stops playback when a
    // track was current, so the reset index counts as a change of its own.
    if (m_index < 0 && sameEntries(next, m_entries))
        return;

    m_entries = std::move(next);
    m_index = -1;
    emit changed();
}

QVector<PlaylistEntry> MediaPlaylistModel::entries() const
{
    return m_entries;
}

int MediaPlaylistModel::count() const
{
    return static_cast<int>(m_entries.size());
}

int MediaPlaylistModel::currentIndex() const
{
    return m_index;
}

PlaylistEntry MediaPlaylistModel::current() const
{
    if (m_index < 0 || m_index >= count())
        return PlaylistEntry{};
    return m_entries.at(m_index);
}

bool MediaPlaylistModel::repeatAll() const
{
    return m_repeat;
}

bool MediaPlaylistModel::shuffle() const
{
    return m_shuffle;
}

void MediaPlaylistModel::setRepeatAll(bool on)
{
    if (m_repeat == on)
        return;

    m_repeat = on;
    emit changed();
}

void MediaPlaylistModel::setShuffle(bool on)
{
    if (m_shuffle == on)
        return;

    m_shuffle = on;
    emit changed();
}

void MediaPlaylistModel::playAt(int index)
{
    if (index < 0 || index >= count())
        return;
    if (index == m_index)
        return;

    m_index = index;
    emit changed();
}

bool MediaPlaylistModel::next()
{
    const int total = count();
    if (total == 0)
        return false;

    // Nothing is current yet, so the queue starts at its first entry.
    if (m_index < 0) {
        playAt(0);
        return true;
    }

    if (m_index + 1 < total) {
        playAt(m_index + 1);
        return true;
    }

    if (!m_repeat)
        return false;

    playAt(0);
    return true;
}

bool MediaPlaylistModel::previous()
{
    const int total = count();
    if (total == 0)
        return false;

    // Nothing is current yet, so the queue starts at its last entry.
    if (m_index < 0) {
        playAt(total - 1);
        return true;
    }

    if (m_index > 0) {
        playAt(m_index - 1);
        return true;
    }

    if (!m_repeat)
        return false;

    playAt(total - 1);
    return true;
}

void MediaPlaylistModel::clear()
{
    if (m_entries.isEmpty() && m_index < 0)
        return;

    m_entries.clear();
    m_index = -1;
    emit changed();
}

} // namespace beygram::telegram
