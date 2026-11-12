#include "SyncedLyricsModel.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

// Real lyrics are a few kilobytes; anything larger is not LRC worth parsing.
constexpr int kMaxInputChars = 256 * 1024;

const QRegularExpression& timestampPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("\\[(\\d{1,3}):(\\d{1,2})(?:[.:](\\d{1,3}))?\\]"));
    return pattern;
}

const QRegularExpression& offsetPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\[offset:\\s*([+-]?\\d{1,7})\\]$"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Converts one [mm:ss], [mm:ss.x], [mm:ss.xx] or [mm:ss.xxx] match to
// milliseconds. Seconds past 59 make the timestamp malformed.
bool timestampToMillis(const QRegularExpressionMatch& match, int& startMs)
{
    bool ok = false;
    const int minutes = match.captured(1).toInt(&ok);
    if (!ok)
        return false;

    const int seconds = match.captured(2).toInt(&ok);
    if (!ok || seconds >= 60)
        return false;

    const QString fraction = match.captured(3);
    int fractionMs = 0;
    if (!fraction.isEmpty()) {
        fractionMs = fraction.toInt(&ok);
        if (!ok)
            return false;

        // A fraction of one or two digits means tenths or hundredths.
        switch (fraction.size()) {
        case 1:
            fractionMs *= 100;
            break;
        case 2:
            fractionMs *= 10;
            break;
        default:
            break;
        }
    }

    startMs = minutes * 60000 + seconds * 1000 + fractionMs;
    return true;
}

bool sameLines(const QVector<LyricLine>& lhs, const QVector<LyricLine>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (qsizetype i = 0; i < lhs.size(); ++i) {
        if (lhs.at(i).startMs != rhs.at(i).startMs || lhs.at(i).text != rhs.at(i).text)
            return false;
    }

    return true;
}

} // namespace

SyncedLyricsModel::SyncedLyricsModel(QObject* parent) : QObject(parent)
{
}

void SyncedLyricsModel::setLyrics(const QString& lrc)
{
    QVector<LyricLine> next;

    if (lrc.size() <= kMaxInputChars && !lrc.trimmed().isEmpty()) {
        // Accept CRLF and lone CR input the same way LF input is accepted.
        QString normalized = lrc;
        normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

        int offsetMs = 0;

        for (const QString& rawLine : normalized.split(QLatin1Char('\n'))) {
            const QString line = rawLine.trimmed();
            if (line.isEmpty())
                continue;

            const QRegularExpressionMatch offsetMatch = offsetPattern().match(line);
            if (offsetMatch.hasMatch()) {
                bool ok = false;
                const int value = offsetMatch.captured(1).toInt(&ok);
                // The last offset tag wins; an unreadable value clears it.
                offsetMs = ok ? value : 0;
                continue;
            }

            QVector<QRegularExpressionMatch> matches;
            QRegularExpressionMatchIterator iterator = timestampPattern().globalMatch(line);
            while (iterator.hasNext())
                matches.append(iterator.next());

            // Lines without a timestamp are metadata or malformed; skip them.
            if (matches.isEmpty())
                continue;

            // All timestamps on a line share the text after the last one.
            const QString text = line.mid(matches.last().capturedEnd()).trimmed();
            if (text.isEmpty())
                continue;

            for (const QRegularExpressionMatch& match : matches) {
                int startMs = 0;
                if (!timestampToMillis(match, startMs))
                    continue;
                next.append(LyricLine{startMs, text});
            }
        }

        // LRC convention: a positive offset makes lyrics appear earlier.
        for (LyricLine& line : next)
            line.startMs = std::max(0, line.startMs - offsetMs);
    }

    std::stable_sort(next.begin(), next.end(),
                     [](const LyricLine& left, const LyricLine& right) {
                         return left.startMs < right.startMs;
                     });

    // Equal starts are duplicates of the same moment; the stable sort keeps
    // the first parsed line, which wins over later ones.
    QVector<LyricLine> merged;
    merged.reserve(next.size());
    for (const LyricLine& line : next) {
        if (!merged.isEmpty() && merged.last().startMs == line.startMs)
            continue;
        merged.append(line);
    }

    if (sameLines(merged, m_lines))
        return;

    m_lines = std::move(merged);
    emit changed();
}

QVector<LyricLine> SyncedLyricsModel::lines() const
{
    return m_lines;
}

int SyncedLyricsModel::count() const
{
    return static_cast<int>(m_lines.size());
}

void SyncedLyricsModel::setPositionMs(int positionMs)
{
    const int clamped = std::max(0, positionMs);
    if (clamped == m_position)
        return;

    m_position = clamped;
    emit changed();
}

int SyncedLyricsModel::currentIndex() const
{
    if (m_lines.isEmpty())
        return -1;

    const auto it = std::upper_bound(m_lines.cbegin(), m_lines.cend(), m_position,
                                     [](int position, const LyricLine& line) {
                                         return position < line.startMs;
                                     });

    return static_cast<int>(it - m_lines.cbegin()) - 1;
}

QString SyncedLyricsModel::currentText() const
{
    const int index = currentIndex();
    return index < 0 ? QString() : m_lines.at(index).text;
}

bool SyncedLyricsModel::hasLyrics() const
{
    return !m_lines.isEmpty();
}

void SyncedLyricsModel::clear()
{
    if (m_lines.isEmpty())
        return;

    m_lines.clear();
    emit changed();
}

} // namespace beygram::telegram
