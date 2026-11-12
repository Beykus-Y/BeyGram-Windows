#include "ChatFolderEditController.h"

#include "FolderEdit.h"

#include <QJsonObject>
#include <QVector>

namespace beygram::telegram {
namespace {

/// The schema's folder-name limit, as documented in FolderEdit.h. The draft is
/// already cut to it by the builder; canSave() re-checks it so a future change
/// to the normalisation cannot silently let a too long name through.
constexpr int kMaxTitleLength = 12;

/// Reads the name back out of the request FolderEdit would send. Folding,
/// trimming and the 12-code-point cut therefore have exactly one implementation,
/// and title() can never disagree with the payload the caller transmits.
QString payloadTitle(const QString& title)
{
    const QJsonObject folder =
        createChatFolderRequest(title).value(QStringLiteral("folder")).toObject();
    const QJsonObject name = folder.value(QStringLiteral("name")).toObject();
    const QJsonObject text = name.value(QStringLiteral("text")).toObject();
    return text.value(QStringLiteral("text")).toString();
}

/// Length in Unicode code points, the unit the schema's limit is written in, so
/// a title of emoji is not rejected for being long in UTF-16 units.
int titleLength(const QString& text)
{
    return static_cast<int>(text.toUcs4().size());
}

} // namespace

ChatFolderEditController::ChatFolderEditController(QObject* parent)
    : QObject(parent)
{
}

void ChatFolderEditController::begin(qint64 folderId, const QString& title)
{
    // A folder that does not exist yet has no id; anything else must be a real
    // custom folder id, and a stray negative value is treated as "new" rather
    // than being sent to TDLib.
    const qint64 id = folderId > 0 ? folderId : 0;
    const QString draft = payloadTitle(title);

    if (m_active && m_folderId == id && m_title == draft && m_original == draft)
        return;

    m_folderId = id;
    m_title = draft;
    m_original = draft;
    m_active = true;
    emit changed();
}

void ChatFolderEditController::cancel()
{
    if (!m_active)
        return;

    m_active = false;
    m_folderId = -1;
    m_title.clear();
    m_original.clear();
    emit changed();
}

bool ChatFolderEditController::active() const
{
    return m_active;
}

qint64 ChatFolderEditController::folderId() const
{
    return m_folderId;
}

QString ChatFolderEditController::title() const
{
    return m_title;
}

void ChatFolderEditController::setTitle(const QString& title)
{
    if (!m_active)
        return;

    const QString draft = payloadTitle(title);
    if (draft == m_title)
        return;

    m_title = draft;
    emit changed();
}

bool ChatFolderEditController::dirty() const
{
    return m_title != m_original;
}

bool ChatFolderEditController::canSave() const
{
    // The blank check runs on the trimmed text so a name of only spaces, which
    // TDLib would reject, never enables the Save button.
    return m_active && dirty() && !m_title.trimmed().isEmpty()
        && titleLength(m_title) <= kMaxTitleLength;
}

void ChatFolderEditController::markSaved()
{
    if (!m_active || m_original == m_title)
        return;

    m_original = m_title;
    emit changed();
}

} // namespace beygram::telegram
