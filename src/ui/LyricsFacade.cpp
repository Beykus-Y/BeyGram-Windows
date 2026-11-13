#include "LyricsFacade.h"

#include <QVector>

namespace beygram::ui {
namespace {

QStringList textsOf(const QVector<telegram::LyricLine>& lines)
{
    QStringList texts;
    texts.reserve(lines.size());
    for (const telegram::LyricLine& line : lines)
        texts.append(line.text);
    return texts;
}

} // namespace

LyricsFacade::LyricsFacade(QObject* parent) : QObject(parent)
{
}

QStringList LyricsFacade::lines() const
{
    return textsOf(m_model.lines());
}

int LyricsFacade::currentIndex() const
{
    return m_model.currentIndex();
}

bool LyricsFacade::hasLyrics() const
{
    return m_model.hasLyrics();
}

LyricsFacade::Snapshot LyricsFacade::snapshot() const
{
    Snapshot state;
    state.lines = lines();
    state.currentIndex = currentIndex();
    state.hasLyrics = hasLyrics();
    return state;
}

void LyricsFacade::commit(const Snapshot& before)
{
    if (before == snapshot())
        return;

    emit changed();
}

void LyricsFacade::setLyrics(const QString& lrc)
{
    const Snapshot before = snapshot();
    m_model.setLyrics(lrc);
    commit(before);
}

void LyricsFacade::setPosition(int positionMs)
{
    const Snapshot before = snapshot();
    m_model.setPositionMs(positionMs);
    commit(before);
}

void LyricsFacade::clear()
{
    const Snapshot before = snapshot();
    m_model.clear();
    commit(before);
}

} // namespace beygram::ui
