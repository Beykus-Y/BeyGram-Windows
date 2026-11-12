#include "StatisticsFacade.h"

#include <QVector>

#include <cmath>
#include <utility>

namespace beygram::ui {

namespace {

/// Groups the digits of a non-negative value by spaces, three at a time from
/// the right: "0", "999", "1 234", "1 234 567". The same grouping the Stars
/// facade and the balance header use, so every screen counts the same way.
QString groupedDigits(qint64 value)
{
    const QString digits = QString::number(value);

    QString grouped;
    grouped.reserve(digits.size() + digits.size() / 3 + 1);

    for (qsizetype i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0)
            grouped += QLatin1Char(' ');
        grouped += digits.at(i);
    }

    return grouped;
}

/// A statistics number as the panel prints it: grouped thousands, at most two
/// decimals, no trailing zeros and a leading "-" only for a real negative.
///
/// Rounding is done on integer hundredths rather than through QString's own
/// number formatting, so the result never depends on the system locale and a
/// value like 12.5 always reads "12.5", not "12,5".
QString groupedNumber(double value)
{
    if (!std::isfinite(value))
        return QStringLiteral("0");

    const double magnitude = std::abs(value);

    // Beyond this the hundredths no longer fit in a qint64; such a value is
    // not a real statistic, so it is printed without grouping rather than
    // wrapping around.
    if (magnitude >= 9.0e15)
        return QString::number(value, 'f', 0);

    const qint64 hundredths = static_cast<qint64>(std::llround(magnitude * 100.0));
    const qint64 whole = hundredths / 100;
    const qint64 fraction = hundredths % 100;

    QString text = groupedDigits(whole);

    if (fraction != 0) {
        QString tail = QString::number(fraction).rightJustified(2, QLatin1Char('0'));
        while (tail.endsWith(QLatin1Char('0')))
            tail.chop(1);
        text += QLatin1Char('.');
        text += tail;
    }

    if (value < 0 && hundredths != 0)
        text.prepend(QLatin1Char('-'));

    return text;
}

/// Glues a unit to a formatted number: "12.5%", "1 234 MB", "3". A unit that
/// starts with a symbol hugs the number; one that starts with a letter or a
/// digit needs the space so "12 MB" does not read as "12MB" -> a word.
QString withUnit(const QString& number, const QString& unit)
{
    if (unit.isEmpty())
        return number;

    if (!unit.at(0).isLetterOrNumber())
        return number + unit;

    return number + QLatin1Char(' ') + unit;
}

/// One payload value split into the number and the suffix that follows it.
struct ParsedValue {
    double value = 0.0;
    QString unit;
};

/// Parses one statistics payload string.
///
/// Grouping spaces are dropped, so "1 234", "1 234 MB" and "12.5%" all read as
/// one number plus an optional suffix. Only a leading number is consumed; the
/// rest of the string (trimmed of the spaces it had between number and unit)
/// becomes the unit, so "12.5%" keeps its percent sign. Text with no leading
/// number reads as 0 with no unit, which the panel shows as "0" instead of
/// guessing at what the payload meant.
ParsedValue parseValue(const QString& text)
{
    QString compact;
    compact.reserve(text.size());
    for (const QChar character : text) {
        if (!character.isSpace())
            compact += character;
    }

    qsizetype index = 0;
    if (index < compact.size()
        && (compact.at(index) == QLatin1Char('+') || compact.at(index) == QLatin1Char('-'))) {
        ++index;
    }

    bool hasDigits = false;

    while (index < compact.size() && compact.at(index).isDigit()) {
        ++index;
        hasDigits = true;
    }

    if (index < compact.size() && compact.at(index) == QLatin1Char('.')) {
        ++index;
        while (index < compact.size() && compact.at(index).isDigit()) {
            ++index;
            hasDigits = true;
        }
    }

    if (!hasDigits)
        return ParsedValue{};

    bool ok = false;
    const double value = compact.left(index).toDouble(&ok);
    if (!ok)
        return ParsedValue{};

    ParsedValue parsed;
    parsed.value = value;
    parsed.unit = compact.mid(index);
    return parsed;
}

/// One overview row as the panel prints it: "1 234", "12.5%", "1 234 MB".
QString formatOverviewValue(const telegram::StatisticsValue& value)
{
    return withUnit(groupedNumber(value.value), value.unit);
}

/// Parses a QML-provided id. Anything that is not a positive integer yields 0,
/// which the message model treats as "ignore".
qint64 parseId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

} // namespace

StatisticsFacade::StatisticsFacade(QObject* parent) : QObject(parent)
{
    // Each model already suppresses no-op changes, so forwarding all three is
    // enough: a redundant update emits nothing, a real one repaints every
    // binding on this object.
    connect(&m_chat, &telegram::ChatStatisticsModel::changed, this, &StatisticsFacade::changed);
    connect(&m_message, &telegram::MessageStatisticsModel::changed,
            this, &StatisticsFacade::changed);
    connect(&m_boosts, &telegram::BoostModel::changed, this, &StatisticsFacade::changed);
}

bool StatisticsFacade::chatStatsAvailable() const
{
    return m_chat.hasData();
}

QStringList StatisticsFacade::overviewLabels() const
{
    const QVector<telegram::StatisticsValue> overview = m_chat.overview();

    QStringList labels;
    labels.reserve(overview.size());
    for (const telegram::StatisticsValue& value : overview)
        labels.append(value.label);

    return labels;
}

QStringList StatisticsFacade::overviewValues() const
{
    const QVector<telegram::StatisticsValue> overview = m_chat.overview();

    QStringList values;
    values.reserve(overview.size());
    for (const telegram::StatisticsValue& value : overview)
        values.append(formatOverviewValue(value));

    return values;
}

QStringList StatisticsFacade::chartLabels() const
{
    return m_chat.chartLabels();
}

QStringList StatisticsFacade::chartValues() const
{
    const QVector<double> series = m_chat.chartSeries();

    QStringList values;
    values.reserve(series.size());
    for (const double point : series)
        values.append(groupedNumber(point));

    return values;
}

void StatisticsFacade::setChatStats(const QStringList& labels, const QStringList& values,
                                    const QStringList& chartLabelList,
                                    const QStringList& chartValueList)
{
    QVector<telegram::StatisticsValue> overview;
    overview.reserve(labels.size());

    for (qsizetype i = 0; i < labels.size(); ++i) {
        const ParsedValue parsed = i < values.size() ? parseValue(values.at(i)) : ParsedValue{};
        overview.append(telegram::StatisticsValue{labels.at(i), parsed.value, parsed.unit});
    }

    QVector<double> series;
    series.reserve(chartValueList.size());
    for (const QString& text : chartValueList)
        series.append(parseValue(text).value);

    // The model owns the normalization rules (drop empty labels, de-duplicate,
    // align the series with its labels), so the facade does not repeat them.
    m_chat.setOverview(std::move(overview));
    m_chat.setChart(chartLabelList, std::move(series));
}

void StatisticsFacade::requestChatStats()
{
    emit chatStatsRequested();
}

void StatisticsFacade::openMessageStats(const QString& chatId, const QString& messageId)
{
    const qint64 chat = parseId(chatId);
    const qint64 message = parseId(messageId);
    if (chat <= 0 || message <= 0)
        return;

    // Selecting the message resets the counters, so an answer for the previous
    // message cannot be read as this one's.
    m_message.setMessage(chat, message);

    // Snapshot the normalized ids: emitting may re-enter the facade.
    emit messageStatsRequested(QString::number(chat), QString::number(message));
}

bool StatisticsFacade::messageStatsActive() const
{
    return m_message.active();
}

void StatisticsFacade::setMessageCounts(int views, int forwards, int reactions)
{
    m_message.setCounts(views, forwards, reactions);
}

int StatisticsFacade::views() const
{
    return m_message.views();
}

int StatisticsFacade::forwards() const
{
    return m_message.forwards();
}

int StatisticsFacade::reactions() const
{
    return m_message.reactions();
}

void StatisticsFacade::setBoosts(int level, int count, int nextLevelCount)
{
    m_boosts.setStatus(level, count, nextLevelCount);
}

int StatisticsFacade::boostLevel() const
{
    return m_boosts.level();
}

int StatisticsFacade::boostCount() const
{
    return m_boosts.boostCount();
}

double StatisticsFacade::boostProgress() const
{
    return m_boosts.progressToNextLevel();
}

void StatisticsFacade::clear()
{
    m_chat.clear();
    m_message.clear();
    m_boosts.clear();
}

} // namespace beygram::ui
