#include "FoldersFacade.h"

#include <QtGlobal>

namespace beygram::ui {

FoldersFacade::FoldersFacade(QObject* parent)
    : QObject(parent)
{
    // Both helpers own their own "changed only on real changes" rules; the
    // facade only relays them so every property shares one notification.
    connect(&m_edit, &telegram::ChatFolderEditController::changed,
            this, &FoldersFacade::changed);
    connect(&m_archive, &telegram::ArchiveSettingsModel::changed,
            this, &FoldersFacade::changed);
}

QStringList FoldersFacade::folderTitles() const
{
    return m_titles;
}

QStringList FoldersFacade::folderIds() const
{
    return m_ids;
}

void FoldersFacade::setFolders(const QStringList& ids, const QStringList& titles)
{
    // A mismatched pair is truncated rather than padded: inventing an id or an
    // empty title would let the strip address a folder that does not exist.
    const int count = qMin(ids.size(), titles.size());
    const QStringList nextIds = ids.mid(0, count);
    const QStringList nextTitles = titles.mid(0, count);

    if (nextIds == m_ids && nextTitles == m_titles)
        return;

    m_ids = nextIds;
    m_titles = nextTitles;
    if (m_currentIndex >= m_ids.size())
        m_currentIndex = -1;

    emit changed();
}

int FoldersFacade::currentIndex() const
{
    return m_currentIndex;
}

void FoldersFacade::selectFolder(int index)
{
    if (index < -1 || index >= m_ids.size())
        return;
    if (index == m_currentIndex)
        return;

    m_currentIndex = index;
    emit changed();
    if (index >= 0)
        emit folderSelected(m_ids.at(index));
}

bool FoldersFacade::editing() const
{
    return m_edit.active();
}

QString FoldersFacade::draftTitle() const
{
    return m_edit.title();
}

bool FoldersFacade::canSave() const
{
    return m_edit.canSave();
}

void FoldersFacade::beginCreate()
{
    // 0 is ChatFolderEditController's marker for "does not exist yet".
    m_edit.begin(0, QString());
}

void FoldersFacade::beginEdit(int index)
{
    if (index < 0 || index >= m_ids.size())
        return;

    bool ok = false;
    const qint64 id = m_ids.at(index).toLongLong(&ok);
    // The main list is numbered 0 and is not a folder; a malformed row is not
    // addressable either. Both stay unedited instead of sending a request.
    if (!ok || id <= 0)
        return;

    m_edit.begin(id, m_titles.at(index));
}

void FoldersFacade::setDraftTitle(const QString& title)
{
    m_edit.setTitle(title);
}

void FoldersFacade::cancelEdit()
{
    m_edit.cancel();
}

bool FoldersFacade::save()
{
    if (!m_edit.canSave())
        return false;

    const qint64 id = m_edit.folderId();
    const QString title = m_edit.title();

    if (id == 0)
        emit createRequested(title);
    else
        emit renameRequested(QString::number(id), title);

    // canSave() guarantees the draft is dirty, so this always moves the
    // original and relays one changed() through the controller.
    m_edit.markSaved();
    return true;
}

void FoldersFacade::requestDelete(int index)
{
    if (index < 0 || index >= m_ids.size())
        return;

    bool ok = false;
    const qint64 id = m_ids.at(index).toLongLong(&ok);
    if (!ok || id <= 0)
        return;

    // Deleting the folder that is being renamed closes that edit first, so the
    // draft cannot outlive the folder it points at.
    if (m_edit.folderId() == id)
        m_edit.cancel();

    emit deleteRequested(QString::number(id));
}

bool FoldersFacade::archiveAndMuteNewChats() const
{
    return m_archive.archiveAndMuteNewChats();
}

void FoldersFacade::setArchiveAndMuteNewChats(bool on)
{
    m_archive.setArchiveAndMuteNewChats(on);
}

} // namespace beygram::ui
