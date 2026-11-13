#include "ui/SwipeSettingsFacade.h"

namespace beygram::ui {
namespace {

// The offered gestures, in the order a picker shows them: the inert choice
// first, then the gestures. The names themselves belong to the model, so this
// list holds enumerators only and actions() asks actionName() for each one; a
// renamed persistence value therefore cannot drift away from the facade.
constexpr telegram::SwipeAction kOfferedActions[] = {
    telegram::SwipeAction::None,
    telegram::SwipeAction::Archive,
    telegram::SwipeAction::Mute,
    telegram::SwipeAction::Read,
    telegram::SwipeAction::Pin,
    telegram::SwipeAction::Delete,
};

// Whether the name is one the model actually stores. actionFromName() reads an
// unknown name as None, which is a real gesture here, so membership has to be
// checked before a setter forwards the name; otherwise a typo would silently
// switch the gesture off.
bool isOfferedAction(const telegram::ChatSwipeSettingsModel& model,
                     const QString& name)
{
    for (const telegram::SwipeAction action : kOfferedActions) {
        if (model.actionName(action) == name)
            return true;
    }

    return false;
}

} // namespace

SwipeSettingsFacade::SwipeSettingsFacade(QObject* parent)
    : QObject(parent)
    , m_settings(this)
{
    // The facade holds no state of its own, so the model's changes are the
    // facade's changes. The model emits at most once per real change.
    connect(&m_settings, &telegram::ChatSwipeSettingsModel::changed,
            this, &SwipeSettingsFacade::changed);
}

QString SwipeSettingsFacade::leftAction() const
{
    return m_settings.actionName(m_settings.settings().left);
}

QString SwipeSettingsFacade::rightAction() const
{
    return m_settings.actionName(m_settings.settings().right);
}

QStringList SwipeSettingsFacade::actions() const
{
    QStringList names;

    for (const telegram::SwipeAction action : kOfferedActions)
        names.append(m_settings.actionName(action));

    return names;
}

void SwipeSettingsFacade::setLeft(const QString& action)
{
    if (!isOfferedAction(m_settings, action))
        return;

    m_settings.setLeft(m_settings.actionFromName(action));
}

void SwipeSettingsFacade::setRight(const QString& action)
{
    if (!isOfferedAction(m_settings, action))
        return;

    m_settings.setRight(m_settings.actionFromName(action));
}

void SwipeSettingsFacade::reset()
{
    m_settings.reset();
}

} // namespace beygram::ui
