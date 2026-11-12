#include "MediaCaptionModel.h"

#include <QChar>

namespace beygram::telegram {
namespace {

// Telegram's caption limit, in UTF-16 units.
constexpr int kMaxCaptionLength = 1024;

/// Truncates to at most limit UTF-16 units without splitting a surrogate pair,
/// so the stored text never ends in half of a character.
QString capped(const QString& text, int limit)
{
    if (text.size() <= limit)
        return text;

    QString result = text.left(limit);
    if (result.at(result.size() - 1).isHighSurrogate())
        result.chop(1);
    return result;
}

} // namespace

MediaCaptionModel::MediaCaptionModel(QObject* parent)
    : QObject(parent)
{
}

void MediaCaptionModel::begin(const QString& original)
{
    const QString next = original.trimmed();
    if (m_active && next == m_original && next == m_text)
        return;

    m_active = true;
    m_original = next;
    m_text = next;
    emit changed();
}

bool MediaCaptionModel::active() const
{
    return m_active;
}

void MediaCaptionModel::setText(const QString& text)
{
    if (!m_active)
        return;

    const QString next = capped(text, kMaxCaptionLength);
    if (next == m_text)
        return;

    m_text = next;
    emit changed();
}

QString MediaCaptionModel::text() const
{
    return m_text;
}

bool MediaCaptionModel::dirty() const
{
    return m_active && m_text != m_original;
}

int MediaCaptionModel::length() const
{
    return static_cast<int>(m_text.size());
}

int MediaCaptionModel::remaining() const
{
    return kMaxCaptionLength - length();
}

bool MediaCaptionModel::canSave() const
{
    return m_active && dirty() && length() <= kMaxCaptionLength;
}

void MediaCaptionModel::markSaved()
{
    if (!m_active || m_original == m_text)
        return;

    m_original = m_text;
    emit changed();
}

void MediaCaptionModel::cancel()
{
    if (!m_active && m_text.isEmpty() && m_original.isEmpty())
        return;

    m_active = false;
    m_text.clear();
    m_original.clear();
    emit changed();
}

} // namespace beygram::telegram
