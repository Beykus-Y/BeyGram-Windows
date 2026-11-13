#include "ChatPreviewModel.h"

#include <QDateTime>

#include <algorithm>

namespace beygram::telegram {
namespace {

// One preview line is short enough for the widest chat row and keeps a pasted
// wall of text out of the model.
constexpr int kMaxPreviewLength = 200;

constexpr qint64 kMinute = 60;
constexpr qint64 kHour = 60 * kMinute;
constexpr qint64 kDay = 24 * kHour;

int nonNegative(int value)
{
    return std::max(0, value);
}

// Collapses every whitespace run to one space and trims the ends, so the line
// can never wrap or carry a newline into a single-line label. The cap counts
// UTF-16 units like QString::size(), but never leaves half of a surrogate pair
// behind: a lone high surrogate has no glyph to paint.
QString oneLine(const QString& text)
{
    QString line = text.simplified();
    if (line.size() <= kMaxPreviewLength)
        return line;

    qsizetype length = kMaxPreviewLength;
    if (line.at(length - 1).isHighSurrogate())
        --length;
    line.truncate(length);
    return line;
}

ChatPreviewState normalise(ChatPreviewState state)
{
    state.text = oneLine(state.text);
    state.unreadCount = nonNegative(state.unreadCount);
    return state;
}

bool sameState(const ChatPreviewState& left, const ChatPreviewState& right)
{
    return left.text == right.text
        && left.isTyping == right.isTyping
        && left.unreadCount == right.unreadCount
        && left.muted == right.muted
        && left.pinned == right.pinned
        && left.draft == right.draft;
}

// True when a row could paint nothing from the state: no preview line and no
// marker, badge or flag. Such a state is not worth storing.
bool isEmptyState(const ChatPreviewState& state)
{
    return state.text.isEmpty() && !state.isTyping && state.unreadCount == 0
        && !state.muted && !state.pinned && !state.draft;
}

} // namespace

ChatPreviewModel::ChatPreviewModel(QObject* parent)
    : QObject(parent)
{
}

void ChatPreviewModel::setPreview(qint64 chatId, const ChatPreviewState& state)
{
    if (chatId <= 0)
        return;

    // The date is owned by setLastMessageDate(), so replacing the preview must
    // not touch it. A chat with only a date keeps its entry.
    Entry entry = m_entries.value(chatId);
    entry.state = normalise(state);
    if (storeEntry(chatId, entry))
        emit changed();
}

ChatPreviewState ChatPreviewModel::preview(qint64 chatId) const
{
    return m_entries.value(chatId).state;
}

QString ChatPreviewModel::badgeText(qint64 chatId, int maxShown) const
{
    const ChatPreviewState state = preview(chatId);
    if (state.unreadCount <= 0)
        return QString();
    if (state.muted)
        return QStringLiteral("•");

    // A zero or negative limit would produce "0+", which names no count, so
    // the smallest meaningful limit is one.
    const int limit = std::max(1, maxShown);
    if (state.unreadCount > limit)
        return QStringLiteral("%1+").arg(limit);
    return QString::number(state.unreadCount);
}

QString ChatPreviewModel::timeText(qint64 chatId, qint64 nowUnix) const
{
    const auto it = m_entries.constFind(chatId);
    if (it == m_entries.cend() || it->lastMessageDate <= 0)
        return QString();

    const qint64 date = it->lastMessageDate;
    // A future date is clock skew, not a message that has not been sent yet,
    // so it reads as now instead of a negative age. This also keeps the
    // subtraction below in range.
    if (date >= nowUnix)
        return QStringLiteral("now");

    const qint64 elapsed = nowUnix - date;
    if (elapsed < kMinute)
        return QStringLiteral("now");
    if (elapsed < kHour)
        return QStringLiteral("%1m").arg(elapsed / kMinute);
    if (elapsed < kDay)
        return QStringLiteral("%1h").arg(elapsed / kHour);
    if (elapsed < 2 * kDay)
        return QStringLiteral("Yesterday");

    const QDateTime moment = QDateTime::fromSecsSinceEpoch(date).toLocalTime();
    if (!moment.isValid())
        return QString();
    return moment.toString(QStringLiteral("dd.MM"));
}

void ChatPreviewModel::setLastMessageDate(qint64 chatId, qint64 date)
{
    if (chatId <= 0)
        return;

    Entry entry = m_entries.value(chatId);
    entry.lastMessageDate = std::max<qint64>(0, date);
    if (storeEntry(chatId, entry))
        emit changed();
}

void ChatPreviewModel::remove(qint64 chatId)
{
    if (chatId <= 0)
        return;

    if (m_entries.remove(chatId) > 0)
        emit changed();
}

void ChatPreviewModel::clear()
{
    if (m_entries.isEmpty())
        return;

    m_entries.clear();
    emit changed();
}

bool ChatPreviewModel::storeEntry(qint64 chatId, const Entry& entry)
{
    const auto existing = m_entries.constFind(chatId);
    const bool stored = existing != m_entries.cend();

    if (isEmptyState(entry.state) && entry.lastMessageDate <= 0) {
        if (!stored)
            return false;
        m_entries.remove(chatId);
        return true;
    }

    if (stored && existing->lastMessageDate == entry.lastMessageDate
        && sameState(existing->state, entry.state)) {
        return false;
    }

    m_entries.insert(chatId, entry);
    return true;
}

} // namespace beygram::telegram
