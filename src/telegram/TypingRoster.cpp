#include "TypingRoster.h"

#include <QStringList>

#include <algorithm>

namespace beygram::telegram {
namespace {

// A typing action is a heartbeat: the indicator disappears 5 seconds after the
// last one, matching what the desktop client shows.
constexpr qint64 kTypingTimeoutMs = 5000;

// The roster exists to show names. A typing action without a usable name still
// counts as a typist, so the label never collapses to empty while count() is
// non-zero.
QString displayName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("Someone");
    return trimmed;
}

bool nameLessThan(const QString& left, const QString& right)
{
    const int folded = QString::compare(left, right, Qt::CaseInsensitive);
    if (folded != 0)
        return folded < 0;
    // Same letters in a different case: fall back to the exact order so the
    // result stays stable.
    return QString::compare(left, right) < 0;
}

} // namespace

TypingRoster::TypingRoster(QObject* parent)
    : QObject(parent)
{
}

void TypingRoster::applyAction(qint64 userId, const QString& userName,
                               const QString& actionType, qint64 nowMs)
{
    if (actionType != QLatin1String("chatActionTyping")) {
        if (m_entries.remove(userId) > 0)
            emit changed();
        return;
    }

    const QString name = displayName(userName);
    const qint64 expiresAt = nowMs + kTypingTimeoutMs;

    auto it = m_entries.find(userId);
    if (it != m_entries.end()) {
        // Only the deadline moved; the label is unchanged and nobody needs to
        // repaint. A stale name, however, is a visible change.
        it->expiresAt = expiresAt;
        if (it->name == name)
            return;
        it->name = name;
        emit changed();
        return;
    }

    m_entries.insert(userId, Entry{name, expiresAt});
    emit changed();
}

void TypingRoster::expire(qint64 nowMs)
{
    bool removed = false;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (it->expiresAt <= nowMs) {
            it = m_entries.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (removed)
        emit changed();
}

QString TypingRoster::label() const
{
    if (m_entries.isEmpty())
        return QString();

    // Four or more names would not fit under the chat title, so the roster
    // switches to a count.
    if (m_entries.size() >= 4)
        return QStringLiteral("%1 people").arg(count());

    QStringList names;
    names.reserve(m_entries.size());
    for (const Entry& entry : m_entries)
        names.append(entry.name);
    std::sort(names.begin(), names.end(), nameLessThan);

    if (names.size() == 1)
        return names.first();
    if (names.size() == 2)
        return QStringLiteral("%1 and %2").arg(names.at(0), names.at(1));
    return QStringLiteral("%1, %2 and %3").arg(names.at(0), names.at(1), names.at(2));
}

int TypingRoster::count() const
{
    return int(m_entries.size());
}

void TypingRoster::clear()
{
    if (m_entries.isEmpty())
        return;
    m_entries.clear();
    emit changed();
}

} // namespace beygram::telegram
