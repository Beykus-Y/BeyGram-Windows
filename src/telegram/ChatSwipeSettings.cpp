#include "ChatSwipeSettings.h"

namespace beygram::telegram {
namespace {

struct ActionName {
    const char* name;
    SwipeAction action;
};

// The persistence format, kept in one place so actionName() and
// actionFromName() cannot drift apart. The strings are lowercase and stable:
// stored settings refer to them, not to the enumerator's numeric value.
constexpr ActionName kActionNames[] = {
    {"none", SwipeAction::None},
    {"archive", SwipeAction::Archive},
    {"mute", SwipeAction::Mute},
    {"read", SwipeAction::Read},
    {"pin", SwipeAction::Pin},
    {"delete", SwipeAction::Delete},
};

// The right side installed for an incoming pair that names the same action on
// both sides. The defaults differ, so at most one of them can equal the
// requested left and the result always differs from it.
SwipeAction repairedRight(SwipeAction left)
{
    const SwipeSettings defaults;
    if (left != defaults.right)
        return defaults.right;
    return defaults.left;
}

} // namespace

ChatSwipeSettingsModel::ChatSwipeSettingsModel(QObject* parent)
    : QObject(parent)
{
}

void ChatSwipeSettingsModel::setSettings(const SwipeSettings& settings)
{
    SwipeSettings next = settings;
    if (next.left == next.right)
        next.right = repairedRight(next.left);

    if (next == m_settings)
        return;

    m_settings = next;
    emit changed();
}

SwipeSettings ChatSwipeSettingsModel::settings() const
{
    return m_settings;
}

bool ChatSwipeSettingsModel::setLeft(SwipeAction action)
{
    if (m_settings.left == action)
        return false;

    if (m_settings.right == action) {
        // The action already belongs to the other side, so the two swap
        // instead of leaving both directions on the same gesture.
        m_settings.right = m_settings.left;
    }
    m_settings.left = action;
    emit changed();
    return true;
}

bool ChatSwipeSettingsModel::setRight(SwipeAction action)
{
    if (m_settings.right == action)
        return false;

    if (m_settings.left == action) {
        // Mirror of setLeft(): the action moves over and the old right takes
        // the left direction.
        m_settings.left = m_settings.right;
    }
    m_settings.right = action;
    emit changed();
    return true;
}

QString ChatSwipeSettingsModel::actionName(SwipeAction action) const
{
    for (const ActionName& entry : kActionNames) {
        if (entry.action == action)
            return QString::fromLatin1(entry.name);
    }

    // A value outside the enumeration has no stored name; read it as the inert
    // action rather than inventing one.
    return QString::fromLatin1(kActionNames[0].name);
}

SwipeAction ChatSwipeSettingsModel::actionFromName(const QString& name) const
{
    for (const ActionName& entry : kActionNames) {
        if (name == QLatin1String(entry.name))
            return entry.action;
    }

    return SwipeAction::None;
}

bool ChatSwipeSettingsModel::isDefault() const
{
    const SwipeSettings defaults;
    return m_settings == defaults;
}

void ChatSwipeSettingsModel::reset()
{
    if (isDefault())
        return;

    m_settings = SwipeSettings{};
    emit changed();
}

} // namespace beygram::telegram
