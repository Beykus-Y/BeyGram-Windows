#include "ChatActionBarModel.h"

namespace beygram::telegram {

ChatActionBarModel::ChatActionBarModel(QObject* parent)
    : QObject(parent)
{
}

void ChatActionBarModel::setAction(ActionBarKind kind, const QString& text)
{
    // None means "no bar", so it can never keep a label or a busy flag from the
    // bar that was shown before.
    const bool visibleKind = kind != ActionBarKind::None;
    const QString stored = visibleKind ? text.trimmed() : QString();
    const bool storedBusy = visibleKind ? m_busy : false;

    if (m_kind == kind && m_text == stored && m_busy == storedBusy)
        return;

    m_kind = kind;
    m_text = stored;
    m_busy = storedBusy;
    emit changed();
}

ActionBarKind ChatActionBarModel::kind() const
{
    return m_kind;
}

QString ChatActionBarModel::text() const
{
    return m_text;
}

bool ChatActionBarModel::visible() const
{
    return m_kind != ActionBarKind::None;
}

void ChatActionBarModel::setBusy(bool busy)
{
    // An invisible bar has no button to disable, and None must keep busy false.
    if (m_kind == ActionBarKind::None)
        return;

    if (m_busy == busy)
        return;

    m_busy = busy;
    emit changed();
}

bool ChatActionBarModel::busy() const
{
    return m_busy;
}

void ChatActionBarModel::clear()
{
    if (m_kind == ActionBarKind::None && m_text.isEmpty() && !m_busy)
        return;

    m_kind = ActionBarKind::None;
    m_text.clear();
    m_busy = false;
    emit changed();
}

} // namespace beygram::telegram
