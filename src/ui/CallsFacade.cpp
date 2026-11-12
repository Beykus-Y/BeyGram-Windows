#include "CallsFacade.h"

#include <QVector>

#include <algorithm>
#include <utility>

namespace beygram::ui {

namespace {

/// Formats a duration as mm:ss, minutes zero-padded to two digits. A call
/// longer than 99 minutes keeps counting ("125:03") instead of wrapping.
QString clockLabel(int seconds)
{
    const int total = std::max(0, seconds);
    const int minutes = total / 60;
    const int rest = total % 60;

    QString label;
    label.reserve(5);
    label += QString::number(minutes).rightJustified(2, QLatin1Char('0'));
    label += QLatin1Char(':');
    label += QString::number(rest).rightJustified(2, QLatin1Char('0'));
    return label;
}

/// One history line, with a real em dash. The source stays pure ASCII so the
/// literal does not depend on the compiler's source charset.
QString historyLine(const QString& name, int duration, const QString& outcome)
{
    const QChar dash(0x2014);
    return name + QLatin1Char(' ') + dash + QLatin1Char(' ') + clockLabel(duration)
        + QLatin1Char(' ') + dash + QLatin1Char(' ') + outcome;
}

/// The last field of a history line, read from the record's status so a missed
/// outgoing call reads as "cancelled" rather than as a plain direction.
QString outcomeLabel(const telegram::CallRecord& record)
{
    if (record.status == QStringLiteral("missed"))
        return QStringLiteral("missed");
    if (record.status == QStringLiteral("cancelled"))
        return QStringLiteral("cancelled");
    return record.outgoing ? QStringLiteral("outgoing") : QStringLiteral("incoming");
}

/// Reads a QML-supplied id. Only a positive decimal number names a call or a
/// user; everything else is refused before it can reach the controller.
bool readId(const QString& text, qint64* value)
{
    bool ok = false;
    const qint64 parsed = text.toLongLong(&ok);
    if (!ok || parsed <= 0)
        return false;

    *value = parsed;
    return true;
}

} // namespace

CallsFacade::CallsFacade(QObject* parent)
    : QObject(parent)
{
    // Both models already suppress no-op changes, so forwarding their signal
    // gives the view one NOTIFY for every property without extra filtering.
    connect(&m_controller, &telegram::CallController::changed, this, &CallsFacade::changed);
    connect(&m_history, &telegram::CallHistoryModel::changed, this, &CallsFacade::changed);
}

bool CallsFacade::active() const
{
    switch (m_controller.phase()) {
    case telegram::CallPhase::Ringing:
    case telegram::CallPhase::Connecting:
    case telegram::CallPhase::Active:
        return true;
    case telegram::CallPhase::Idle:
    case telegram::CallPhase::Ended:
        break;
    }

    return false;
}

QString CallsFacade::phase() const
{
    return m_controller.phaseName();
}

QString CallsFacade::peerName() const
{
    return m_peerName;
}

bool CallsFacade::video() const
{
    return m_controller.video();
}

bool CallsFacade::muted() const
{
    return m_controller.muted();
}

bool CallsFacade::speaker() const
{
    return m_controller.speaker();
}

int CallsFacade::elapsed() const
{
    return m_controller.elapsedSeconds();
}

QStringList CallsFacade::history() const
{
    const QVector<telegram::CallRecord> records = m_history.records();

    QStringList lines;
    lines.reserve(records.size());

    for (const telegram::CallRecord& record : records) {
        // A record added without a name still has to render as something, so an
        // empty name reads as "Unknown" instead of an empty first field.
        QString name = m_historyNames.value(record.messageId);
        if (name.isEmpty())
            name = QStringLiteral("Unknown");

        lines.append(historyLine(name, record.duration, outcomeLabel(record)));
    }

    return lines;
}

void CallsFacade::startOutgoing(const QString& callId, const QString& userId,
                                const QString& peerName, bool video)
{
    qint64 call = 0;
    qint64 user = 0;
    if (!readId(callId, &call) || !readId(userId, &user))
        return;

    // The name and id are stored first so a view that reacts to the changed()
    // the controller emits already reads the new call, not the previous one.
    const QString previousName = m_peerName;
    const telegram::CallPhase previousPhase = m_controller.phase();
    const qint64 previousCallId = m_controller.callId();

    m_callId = callId;
    m_peerName = peerName;
    m_controller.startOutgoing(call, user, video);

    // Re-reporting the same call is silent in the controller, so a corrected
    // peer name still needs its own notification to repaint the call screen.
    const bool moved = m_controller.phase() != previousPhase
        || m_controller.callId() != previousCallId;
    if (!moved && previousName != m_peerName)
        emit changed();

    emit outgoingRequested(callId, userId, video);
}

void CallsFacade::incoming(const QString& callId, const QString& userId,
                           const QString& peerName, bool video)
{
    qint64 call = 0;
    qint64 user = 0;
    if (!readId(callId, &call) || !readId(userId, &user))
        return;

    const QString previousName = m_peerName;
    const telegram::CallPhase previousPhase = m_controller.phase();
    const qint64 previousCallId = m_controller.callId();

    m_callId = callId;
    m_peerName = peerName;
    m_controller.incoming(call, user, video);

    // A repeated report of the same ring is silent in the controller, but a
    // name that arrived late must still reach the screen.
    const bool moved = m_controller.phase() != previousPhase
        || m_controller.callId() != previousCallId;
    if (!moved && previousName != m_peerName)
        emit changed();
}

void CallsFacade::accept()
{
    const telegram::CallPhase before = m_controller.phase();
    m_controller.accept();

    // A phase that did not move means the controller refused the answer (no
    // call, or a call that already ended), so there is nothing to forward.
    if (m_controller.phase() == before)
        return;

    emit answerRequested(m_callId);
}

void CallsFacade::decline()
{
    const telegram::CallPhase before = m_controller.phase();
    m_controller.decline();

    if (m_controller.phase() == before)
        return;

    emit endRequested(m_callId);
}

void CallsFacade::end()
{
    const telegram::CallPhase before = m_controller.phase();
    m_controller.end();

    if (m_controller.phase() == before)
        return;

    emit endRequested(m_callId);
}

void CallsFacade::setMuted(bool muted)
{
    m_controller.setMuted(muted);
}

void CallsFacade::setSpeaker(bool speaker)
{
    m_controller.setSpeaker(speaker);
}

void CallsFacade::setElapsed(int seconds)
{
    m_controller.setElapsedSeconds(seconds);
}

void CallsFacade::addHistory(const QString& name, int duration, bool missed,
                             bool outgoing, bool video)
{
    telegram::CallRecord record;
    // The counter doubles as the record id and its date: CallHistoryModel sorts
    // newest first by date and then by id, so a later call always lands first.
    record.messageId = ++m_nextHistoryId;
    record.date = m_nextHistoryId;
    record.outgoing = outgoing;
    record.video = video;
    record.duration = std::max(0, duration);

    if (!missed)
        record.status = QStringLiteral("ended");
    else
        record.status = outgoing ? QStringLiteral("cancelled") : QStringLiteral("missed");

    m_historyNames.insert(record.messageId, name);

    QVector<telegram::CallRecord> records = m_history.records();
    records.append(record);
    m_history.setRecords(std::move(records));
}

void CallsFacade::clearHistory()
{
    if (m_history.count() == 0 && m_historyNames.isEmpty())
        return;

    m_historyNames.clear();

    if (m_history.count() > 0) {
        m_history.clear();
        return;
    }

    // Only stale names were left, so the model has nothing to report.
    emit changed();
}

} // namespace beygram::ui
