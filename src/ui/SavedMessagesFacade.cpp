#include "SavedMessagesFacade.h"

#include <QDateTime>

namespace beygram::ui {

namespace {

/// Parses a QML-provided id. Anything that is not a positive integer yields 0,
/// which the model treats as "cannot address this entry".
qint64 parseId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

/// The stable row key: both parts are positive, so ':' cannot appear in one.
QString formatKey(qint64 chatId, qint64 messageId)
{
    return QString::number(chatId) + QLatin1Char(':') + QString::number(messageId);
}

} // namespace

SavedMessagesFacade::SavedMessagesFacade(QObject* parent)
    : QObject(parent)
{
    // The model emits only on a real change, so forwarding is enough: a
    // redundant update repaints nothing, a real one refreshes every binding.
    connect(&m_model, &telegram::SavedMessagesModel::changed,
            this, &SavedMessagesFacade::changed);
}

QStringList SavedMessagesFacade::previews() const
{
    const QVector<telegram::SavedMessage> items = m_model.items();

    QStringList result;
    result.reserve(items.size());
    for (const telegram::SavedMessage& item : items)
        result.append(item.preview);

    return result;
}

QStringList SavedMessagesFacade::ids() const
{
    const QVector<telegram::SavedMessage> items = m_model.items();

    QStringList result;
    result.reserve(items.size());
    for (const telegram::SavedMessage& item : items)
        result.append(formatKey(item.chatId, item.messageId));

    return result;
}

QStringList SavedMessagesFacade::dates() const
{
    const QVector<telegram::SavedMessage> items = m_model.items();

    QStringList result;
    result.reserve(items.size());
    for (const telegram::SavedMessage& item : items)
        result.append(QString::number(item.savedAt));

    return result;
}

int SavedMessagesFacade::count() const
{
    return m_model.count();
}

void SavedMessagesFacade::add(const QString& chatId, const QString& messageId,
                              const QString& preview)
{
    addAt(chatId, messageId, preview, QDateTime::currentSecsSinceEpoch());
}

void SavedMessagesFacade::addAt(const QString& chatId, const QString& messageId,
                                const QString& preview, qint64 savedAt)
{
    const qint64 chat = parseId(chatId);
    const qint64 message = parseId(messageId);
    if (chat <= 0 || message <= 0)
        return;

    telegram::SavedMessage item;
    item.chatId = chat;
    item.messageId = message;
    item.savedAt = savedAt;
    item.preview = preview;

    // The model owns the duplicate rule and the ordering, so a pair that is
    // already stored keeps the preview and date it has.
    m_model.append(QVector<telegram::SavedMessage>{item});
}

void SavedMessagesFacade::remove(const QString& chatId, const QString& messageId)
{
    const qint64 chat = parseId(chatId);
    const qint64 message = parseId(messageId);
    if (chat <= 0 || message <= 0)
        return;

    m_model.remove(chat, message);
}

void SavedMessagesFacade::removeAt(int index)
{
    const QVector<telegram::SavedMessage> items = m_model.items();
    if (index < 0 || index >= items.size())
        return;

    // Snapshot the pair: a changed() slot may re-enter the facade, and the
    // visible order is rebuilt from the model every time.
    const telegram::SavedMessage item = items.at(index);
    m_model.remove(item.chatId, item.messageId);
}

void SavedMessagesFacade::clear()
{
    m_model.clear();
}

void SavedMessagesFacade::open(int index)
{
    const QVector<telegram::SavedMessage> items = m_model.items();
    if (index < 0 || index >= items.size())
        return;

    // Snapshot both ids before emitting; a slot on openRequested() may mutate
    // the list and shift the rows.
    const telegram::SavedMessage item = items.at(index);
    emit openRequested(QString::number(item.chatId),
                       QString::number(item.messageId));
}

} // namespace beygram::ui
