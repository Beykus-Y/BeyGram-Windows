#include "CallHistoryFacade.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>

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

/// Reads a QML-supplied id. Only a positive decimal number names a call;
/// everything else is refused before it can reach an entry.
bool readId(const QString& text, qint64* value)
{
    bool ok = false;
    const qint64 parsed = text.trimmed().toLongLong(&ok);
    if (!ok || parsed <= 0)
        return false;

    *value = parsed;
    return true;
}

/// Epoch seconds, or 0 when the caller did not supply a usable date.
qint64 readDate(const QString& text)
{
    bool ok = false;
    const qint64 parsed = text.trimmed().toLongLong(&ok);
    return ok && parsed > 0 ? parsed : 0;
}

/// Seconds, never negative. An unparsable value reads as 0, like a missed call.
int readDuration(const QString& text)
{
    bool ok = false;
    const int parsed = text.trimmed().toInt(&ok);
    return ok ? std::max(0, parsed) : 0;
}

struct Outcome {
    bool outgoing;
    bool missed;
};

/// Reads the `missed` list, which accepts a flag as well as the status token a
/// CallRecord stores. A missed call is inbound unless the token says the call
/// was cancelled, which is an outgoing call the peer never answered.
Outcome readOutcome(const QString& token)
{
    const QString value = token.trimmed().toLower();

    if (value == QStringLiteral("missed") || value == QStringLiteral("true")
        || value == QStringLiteral("1") || value == QStringLiteral("yes")) {
        return { false, true };
    }

    if (value == QStringLiteral("cancelled") || value == QStringLiteral("canceled")
        || value == QStringLiteral("outgoing") || value == QStringLiteral("noanswer")
        || value == QStringLiteral("no-answer") || value == QStringLiteral("no_answer")) {
        return { true, false };
    }

    // "incoming", "declined", an empty flag and every other status all describe
    // an inbound call that was not missed.
    return { false, false };
}

} // namespace

bool CallHistoryFacade::Entry::operator==(const Entry& other) const
{
    return id == other.id && name == other.name && idValue == other.idValue
        && date == other.date && duration == other.duration
        && outgoing == other.outgoing && missed == other.missed;
}

CallHistoryFacade::CallHistoryFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList CallHistoryFacade::entries() const
{
    return m_lines;
}

int CallHistoryFacade::missedCount() const
{
    int count = 0;
    for (const Entry& entry : m_entries) {
        if (entry.missed)
            ++count;
    }

    return count;
}

bool CallHistoryFacade::empty() const
{
    return m_entries.isEmpty();
}

void CallHistoryFacade::setRecords(const QStringList& ids, const QStringList& names,
                                   const QStringList& dates, const QStringList& durations,
                                   const QStringList& missed)
{
    QVector<Entry> prepared;
    prepared.reserve(ids.size());

    QSet<qint64> seen;

    for (int index = 0; index < ids.size(); ++index) {
        qint64 id = 0;
        if (!readId(ids.at(index), &id))
            continue;

        // The first record of an id wins, so a page that overlaps the previous
        // one cannot duplicate a row.
        if (seen.contains(id))
            continue;
        seen.insert(id);

        Entry entry;
        entry.id = ids.at(index).trimmed();
        entry.idValue = id;
        entry.name = names.value(index);
        if (entry.name.trimmed().isEmpty())
            entry.name = QStringLiteral("Unknown");
        entry.date = readDate(dates.value(index));
        entry.duration = readDuration(durations.value(index));

        const Outcome outcome = readOutcome(missed.value(index));
        entry.outgoing = outcome.outgoing;
        entry.missed = outcome.missed;

        prepared.append(entry);
    }

    std::sort(prepared.begin(), prepared.end(), [](const Entry& left, const Entry& right) {
        if (left.date != right.date)
            return left.date > right.date;
        return left.idValue > right.idValue;
    });

    if (prepared == m_entries)
        return;

    m_entries = std::move(prepared);

    m_lines.clear();
    m_lines.reserve(m_entries.size());
    for (const Entry& entry : std::as_const(m_entries))
        m_lines.append(entryJson(entry));

    emit changed();
}

void CallHistoryFacade::clear()
{
    if (m_entries.isEmpty() && m_lines.isEmpty())
        return;

    m_entries.clear();
    m_lines.clear();
    emit changed();
}

QString CallHistoryFacade::entryJson(const Entry& entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), entry.id);
    object.insert(QStringLiteral("name"), entry.name);
    object.insert(QStringLiteral("outgoing"), entry.outgoing);
    object.insert(QStringLiteral("missed"), entry.missed);
    // Epoch seconds fit in a double exactly, and QML reads the field as a
    // Number before handing it to BeyGramLang.formatDateTime().
    object.insert(QStringLiteral("date"), QJsonValue(static_cast<double>(entry.date)));
    object.insert(QStringLiteral("duration"), entry.duration);
    object.insert(QStringLiteral("durationLabel"), clockLabel(entry.duration));

    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

} // namespace beygram::ui
