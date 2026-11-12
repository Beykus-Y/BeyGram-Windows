#include "ArchiveSettingsModel.h"

namespace beygram::telegram {

ArchiveSettingsModel::ArchiveSettingsModel(QObject* parent)
    : QObject(parent)
{
}

void ArchiveSettingsModel::setArchiveAndMuteNewChats(bool on)
{
    if (m_newChats == on)
        return;
    m_newChats = on;
    emit changed();
}

bool ArchiveSettingsModel::archiveAndMuteNewChats() const
{
    return m_newChats;
}

void ArchiveSettingsModel::setKeepArchivedUnmuted(bool on)
{
    if (m_keepUnmuted == on)
        return;
    m_keepUnmuted = on;
    emit changed();
}

bool ArchiveSettingsModel::keepArchivedUnmuted() const
{
    return m_keepUnmuted;
}

void ArchiveSettingsModel::setKeepArchivedFolders(bool on)
{
    if (m_keepFolders == on)
        return;
    m_keepFolders = on;
    emit changed();
}

bool ArchiveSettingsModel::keepArchivedFolders() const
{
    return m_keepFolders;
}

void ArchiveSettingsModel::setArchiveStories(bool on)
{
    if (m_stories == on)
        return;
    m_stories = on;
    emit changed();
}

bool ArchiveSettingsModel::archiveStories() const
{
    return m_stories;
}

void ArchiveSettingsModel::reset()
{
    // A model that never left its defaults has nothing to restore, so the
    // screen is not repainted for a reset that changes nothing.
    if (isDefault())
        return;

    m_newChats = false;
    m_keepUnmuted = false;
    m_keepFolders = false;
    m_stories = false;
    emit changed();
}

bool ArchiveSettingsModel::isDefault() const
{
    return !m_newChats && !m_keepUnmuted && !m_keepFolders && !m_stories;
}

} // namespace beygram::telegram
