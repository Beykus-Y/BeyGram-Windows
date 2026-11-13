#include "MultiForwardFacade.h"

#include "telegram/ForwardBatch.h"

#include <QSet>
#include <utility>

namespace beygram::ui {

namespace {

/// Mirrors the limits documented and enforced by ForwardBatchPlanner. They are
/// repeated here because begin() and toggleTarget() must reject an over-limit
/// action before a full plan exists.
constexpr qsizetype kMaxMessages = 100;
constexpr qsizetype kMaxTargets = 20;

/// Parses a QML-provided chat id. The source chat may be negative (groups and
/// channels), so only a missing or non-numeric value becomes 0.
qint64 parseChatId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok ? id : 0;
}

/// Positive and unique, first occurrence wins, input order preserved.
QVector<qint64> usableMessageIds(const QStringList& values)
{
    QVector<qint64> ids;
    ids.reserve(values.size());
    QSet<qint64> seen;
    seen.reserve(values.size());
    for (const QString& value : values) {
        const qint64 id = parseChatId(value);
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);
        ids.append(id);
    }
    return ids;
}

QStringList toStringList(const QVector<qint64>& ids)
{
    QStringList result;
    result.reserve(ids.size());
    for (const qint64 id : ids)
        result.append(QString::number(id));
    return result;
}

telegram::ForwardBatchPlan planFor(qint64 fromChatId, const QVector<qint64>& messageIds,
                                   const QVector<qint64>& targetIds)
{
    telegram::ForwardBatchPlan plan;
    plan.fromChatId = fromChatId;
    plan.messageIds = messageIds;
    plan.targetChatIds = targetIds;
    return plan;
}

} // namespace

MultiForwardFacade::MultiForwardFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList MultiForwardFacade::chatTitles() const
{
    QStringList titles;
    titles.reserve(m_chats.size());
    for (const ChatEntry& chat : m_chats) {
        if (matchesFilter(chat.title))
            titles.append(chat.title);
    }
    return titles;
}

QString MultiForwardFacade::chatIdAt(int index) const
{
    if (index < 0)
        return {};

    int visibleIndex = 0;
    for (const ChatEntry& chat : m_chats) {
        if (!matchesFilter(chat.title))
            continue;
        if (visibleIndex == index)
            return QString::number(chat.id);
        ++visibleIndex;
    }
    return {};
}

QStringList MultiForwardFacade::selectedIds() const
{
    return toStringList(targetIds());
}

QString MultiForwardFacade::filter() const
{
    return m_filter;
}

void MultiForwardFacade::setFilter(const QString& filter)
{
    if (m_filter == filter)
        return;

    m_filter = filter;
    emit changed();
}

bool MultiForwardFacade::canForward() const
{
    const QString validation = telegram::ForwardBatchPlanner::validate(
        planFor(m_fromChatId, m_messageIds, targetIds()));
    return validation.isEmpty();
}

QString MultiForwardFacade::error() const
{
    return errorText();
}

void MultiForwardFacade::begin(const QString& fromChatId, const QStringList& messageIds)
{
    const qint64 source = parseChatId(fromChatId);
    const QVector<qint64> ids = usableMessageIds(messageIds);

    Issue issue = Issue::None;
    if (source == 0)
        issue = Issue::NoSource;
    else if (ids.isEmpty())
        issue = Issue::NoMessages;
    else if (ids.size() > kMaxMessages)
        issue = Issue::TooManyMessages;

    if (issue != Issue::None) {
        // A rejected begin must not leave an older session armed: the dialog
        // would otherwise offer to forward messages the user no longer sees.
        const bool hadState = m_fromChatId != 0 || !m_messageIds.isEmpty()
            || !m_targets.isEmpty() || !m_filter.isEmpty() || m_issue != issue;
        m_fromChatId = 0;
        m_messageIds.clear();
        m_targets.clear();
        m_filter.clear();
        m_issue = issue;
        if (hadState)
            emit changed();
        return;
    }

    const bool stateChanged = m_fromChatId != source || m_messageIds != ids
        || !m_targets.isEmpty() || !m_filter.isEmpty() || m_issue != Issue::None;

    m_fromChatId = source;
    m_messageIds = ids;
    m_targets.clear();
    m_filter.clear();
    m_issue = Issue::None;

    if (stateChanged)
        emit changed();
}

void MultiForwardFacade::setChats(const QStringList& ids, const QStringList& titles)
{
    QVector<ChatEntry> chats;
    chats.reserve(ids.size());
    QSet<qint64> seen;
    seen.reserve(ids.size());

    for (qsizetype i = 0; i < ids.size(); ++i) {
        const qint64 id = parseChatId(ids.at(i));
        if (id <= 0 || seen.contains(id))
            continue;
        seen.insert(id);

        const QString supplied = i < titles.size() ? titles.at(i).trimmed() : QString();
        ChatEntry chat;
        chat.id = id;
        chat.title = supplied.isEmpty() ? QString::number(id) : supplied;
        chats.append(chat);
    }

    bool targetsChanged = false;
    for (auto it = m_targets.begin(); it != m_targets.end();) {
        if (!seen.contains(*it)) {
            it = m_targets.erase(it);
            targetsChanged = true;
        } else {
            ++it;
        }
    }

    bool issueChanged = false;
    if (m_issue == Issue::TooManyTargets && m_targets.size() < kMaxTargets) {
        m_issue = Issue::None;
        issueChanged = true;
    }

    bool chatsChanged = chats.size() != m_chats.size();
    if (!chatsChanged) {
        for (qsizetype i = 0; i < chats.size(); ++i) {
            if (chats.at(i).id != m_chats.at(i).id
                || chats.at(i).title != m_chats.at(i).title) {
                chatsChanged = true;
                break;
            }
        }
    }

    if (!chatsChanged && !targetsChanged && !issueChanged)
        return;

    m_chats = std::move(chats);
    emit changed();
}

void MultiForwardFacade::toggleTarget(const QString& chatId)
{
    const qint64 id = parseChatId(chatId);
    if (id <= 0)
        return;

    bool known = false;
    for (const ChatEntry& chat : m_chats) {
        if (chat.id == id) {
            known = true;
            break;
        }
    }
    if (!known)
        return;

    if (m_targets.contains(id)) {
        m_targets.remove(id);
        if (m_issue == Issue::NoTargets || m_issue == Issue::TooManyTargets)
            m_issue = Issue::None;
        emit changed();
        return;
    }

    if (m_targets.size() >= kMaxTargets) {
        if (m_issue != Issue::TooManyTargets) {
            m_issue = Issue::TooManyTargets;
            emit changed();
        }
        return;
    }

    m_targets.insert(id);
    if (m_issue == Issue::NoTargets || m_issue == Issue::TooManyTargets)
        m_issue = Issue::None;
    emit changed();
}

bool MultiForwardFacade::confirm()
{
    const QString validation = telegram::ForwardBatchPlanner::validate(
        planFor(m_fromChatId, m_messageIds, targetIds()));

    if (!validation.isEmpty()) {
        const Issue issue = issueFor(validation);
        if (m_issue != issue) {
            m_issue = issue;
            emit changed();
        }
        return false;
    }

    // Snapshot before any signal: a changed() or forwardRequested() slot may
    // start the next session and would otherwise change the payload mid-emit.
    const QString from = QString::number(m_fromChatId);
    const QStringList messages = toStringList(m_messageIds);
    const QStringList targets = selectedIds();
    const bool hadIssue = m_issue != Issue::None;

    m_issue = Issue::None;
    if (hadIssue)
        emit changed();

    emit forwardRequested(from, messages, targets);
    return true;
}

MultiForwardFacade::Issue MultiForwardFacade::issueFor(const QString& validationCode)
{
    if (validationCode == QStringLiteral("no-source"))
        return Issue::NoSource;
    if (validationCode == QStringLiteral("no-messages"))
        return Issue::NoMessages;
    if (validationCode == QStringLiteral("no-targets"))
        return Issue::NoTargets;
    if (validationCode == QStringLiteral("too-many-messages"))
        return Issue::TooManyMessages;
    if (validationCode == QStringLiteral("too-many-targets"))
        return Issue::TooManyTargets;
    return Issue::None;
}

bool MultiForwardFacade::matchesFilter(const QString& title) const
{
    return m_filter.isEmpty() || title.contains(m_filter, Qt::CaseInsensitive);
}

QVector<qint64> MultiForwardFacade::targetIds() const
{
    QVector<qint64> ids;
    ids.reserve(m_targets.size());
    for (const ChatEntry& chat : m_chats) {
        if (m_targets.contains(chat.id))
            ids.append(chat.id);
    }
    return ids;
}

QString MultiForwardFacade::errorText() const
{
    switch (m_issue) {
    case Issue::None:
        return {};
    case Issue::NoSource:
        return tr("Choose a chat to forward from.");
    case Issue::NoMessages:
        return tr("Choose at least one message to forward.");
    case Issue::NoTargets:
        return tr("Choose at least one chat to forward to.");
    case Issue::TooManyMessages:
        return tr("You can forward up to %1 messages at once.")
            .arg(static_cast<int>(kMaxMessages));
    case Issue::TooManyTargets:
        return tr("You can forward to up to %1 chats at once.")
            .arg(static_cast<int>(kMaxTargets));
    }
    return {};
}

} // namespace beygram::ui
