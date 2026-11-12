#include "SelectionFacade.h"

#include "telegram/ForwardTargetModel.h"
#include "telegram/MessageJumpController.h"
#include "telegram/MessageSelectionController.h"
#include "telegram/TelegramTypes.h"

#include <QVector>

#include <utility>

namespace beygram::ui {

namespace {

/// Parses a QML-provided id. Anything that is not a positive integer yields 0,
/// which every controller treats as "ignore".
qint64 parseId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

QString formatId(qint64 id)
{
    return id > 0 ? QString::number(id) : QString();
}

QStringList toStringList(const QVector<qint64>& ids)
{
    QStringList result;
    result.reserve(ids.size());
    for (const qint64 id : ids)
        result.append(QString::number(id));
    return result;
}

} // namespace

SelectionFacade::SelectionFacade(QObject* parent)
    : QObject(parent)
    , m_selection(new telegram::MessageSelectionController(this))
    , m_forward(new telegram::ForwardTargetModel(this))
    , m_jump(new telegram::MessageJumpController(this))
{
    connect(m_selection, &telegram::MessageSelectionController::changed,
            this, &SelectionFacade::changed);
    connect(m_forward, &telegram::ForwardTargetModel::changed,
            this, &SelectionFacade::changed);
    connect(m_jump, &telegram::MessageJumpController::changed,
            this, &SelectionFacade::changed);
}

bool SelectionFacade::active() const
{
    return m_selection->active();
}

int SelectionFacade::count() const
{
    return m_selection->count();
}

QStringList SelectionFacade::ids() const
{
    return toStringList(m_selection->ids());
}

void SelectionFacade::toggle(const QString& messageId)
{
    m_selection->toggle(parseId(messageId));
}

void SelectionFacade::select(const QString& messageId)
{
    m_selection->select(parseId(messageId));
}

void SelectionFacade::clear()
{
    m_selection->clear();
    if (m_forward->active())
        m_forward->reset();
}

void SelectionFacade::beginForward()
{
    if (!m_selection->active())
        return;

    // A fresh picker never inherits the previous search text or target chat.
    m_forward->setFilter(QString());
    m_forward->open(m_selection->idsForForward());
}

void SelectionFacade::cancelForward()
{
    m_forward->reset();
}

bool SelectionFacade::forwardOpen() const
{
    return m_forward->active();
}

QString SelectionFacade::forwardFilter() const
{
    return m_forward->filter();
}

void SelectionFacade::setForwardFilter(const QString& filter)
{
    m_forward->setFilter(filter);
}

void SelectionFacade::setForwardChats(const QStringList& chatIds)
{
    QVector<telegram::ChatData> chats;
    chats.reserve(chatIds.size());
    for (const QString& value : chatIds) {
        const qint64 id = parseId(value);
        if (id <= 0)
            continue;

        telegram::ChatData chat;
        chat.id = id;
        chat.title = QString::number(id);
        chat.type = QStringLiteral("chat");
        chats.append(chat);
    }

    m_forward->setChats(std::move(chats));
}

void SelectionFacade::chooseForwardChat(const QString& chatId)
{
    const qint64 id = parseId(chatId);
    if (id <= 0)
        return;

    m_forward->setSelectedChat(id);
    if (!canForward())
        return;

    // Snapshot both sides: emitting may re-enter the facade. The payload uses
    // the session's ids, which are the messages the picker actually showed.
    const QString target = QString::number(m_forward->selectedChatId());
    const QStringList messages = toStringList(m_forward->messageIds());
    emit forwardRequested(target, messages);
}

bool SelectionFacade::canForward() const
{
    return m_forward->active() && m_forward->canSend();
}

void SelectionFacade::beginJump(const QString& chatId, const QString& messageId)
{
    const qint64 chat = parseId(chatId);
    const qint64 message = parseId(messageId);
    if (chat <= 0 || message <= 0)
        return;

    m_jump->begin(chat, message);
    emit jumpRequested(QString::number(chat), QString::number(message));
}

void SelectionFacade::finishJump()
{
    m_jump->finish();
}

QString SelectionFacade::highlightId() const
{
    return formatId(m_jump->highlightId());
}

} // namespace beygram::ui
