#include "NotificationSoundModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

// A sound id names the TDLib file of a clip, so 0 and negative ids are not
// sounds. "Silent" is represented by the selection being 0, not by an option.
bool isSoundId(qint64 soundId)
{
    return soundId > 0;
}

bool storesId(const QVector<SoundOption>& options, qint64 soundId)
{
    return std::any_of(options.cbegin(), options.cend(),
                       [soundId](const SoundOption& option) {
                           return option.soundId == soundId;
                       });
}

// SoundOption carries no operator==, so the change check compares the three
// fields explicitly. Keeps setOptions() from notifying on a no-op assignment.
bool sameOptions(const QVector<SoundOption>& lhs, const QVector<SoundOption>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                      [](const SoundOption& left, const SoundOption& right) {
                          return left.soundId == right.soundId
                              && left.title == right.title
                              && left.durationMs == right.durationMs;
                      });
}

} // namespace

NotificationSoundModel::NotificationSoundModel(QObject* parent)
    : QObject(parent)
{
}

void NotificationSoundModel::setOptions(QVector<SoundOption> options)
{
    QVector<SoundOption> next;
    next.reserve(options.size());

    for (SoundOption& option : options) {
        if (!isSoundId(option.soundId))
            continue;
        // An id identifies a sound; the first spelling, title and duration win
        // over later repeats so the list never shows the same sound twice.
        if (storesId(next, option.soundId))
            continue;
        // A clip cannot last a negative time, so it is stored as the shortest
        // possible duration instead of a value the player cannot honour.
        option.durationMs = std::max(option.durationMs, 0);
        next.append(std::move(option));
    }

    if (sameOptions(next, m_options))
        return;

    m_options = std::move(next);
    emit changed();
}

QVector<SoundOption> NotificationSoundModel::options() const
{
    return m_options;
}

int NotificationSoundModel::count() const
{
    return static_cast<int>(m_options.size());
}

void NotificationSoundModel::select(qint64 soundId)
{
    // 0 is the silent choice and always acceptable. Any other id has to name a
    // stored sound, otherwise the caller would select something that cannot be
    // played.
    if (soundId != 0 && !storesId(m_options, soundId))
        return;

    if (m_selected == soundId)
        return;

    m_selected = soundId;
    emit changed();
}

qint64 NotificationSoundModel::selectedId() const
{
    return m_selected;
}

SoundOption NotificationSoundModel::selected() const
{
    if (m_selected == 0)
        return SoundOption{};

    const auto it = std::find_if(m_options.cbegin(), m_options.cend(),
                                 [this](const SoundOption& option) {
                                     return option.soundId == m_selected;
                                 });
    if (it == m_options.cend())
        return SoundOption{};

    return *it;
}

QString NotificationSoundModel::selectedTitle() const
{
    if (m_selected == 0)
        return QStringLiteral("Silent");

    const auto it = std::find_if(m_options.cbegin(), m_options.cend(),
                                 [this](const SoundOption& option) {
                                     return option.soundId == m_selected;
                                 });
    // The selected sound is not in the current list, so there is no title to
    // show. The id is kept in case a later list brings the sound back.
    if (it == m_options.cend())
        return QString();

    return it->title;
}

void NotificationSoundModel::clear()
{
    if (m_options.isEmpty() && m_selected == 0)
        return;

    m_options.clear();
    m_selected = 0;
    emit changed();
}

} // namespace beygram::telegram
