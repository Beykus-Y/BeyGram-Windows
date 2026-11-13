#pragma once

#include "telegram/ChatSwipeSettings.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The chat-list swipe gestures as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the pair
/// of gestures lives in ChatSwipeSettingsModel, and every getter reads straight
/// from it. The facade only translates between the model's SwipeAction values
/// and the stable lowercase names QML can bind to, and it exposes the list of
/// offered names a segmented picker needs.
///
/// Names:
///  - actions() lists every offered gesture by its stored name, in display
///    order: "none", "archive", "mute", "read", "pin", "delete". "none" is a
///    real choice: it leaves that direction's gesture inert without hiding the
///    setting.
///  - leftAction() and rightAction() return the stored names, so they are
///    always one of actions().
///  - setLeft() and setRight() accept exactly those names. A name that is not
///    in actions(), including an empty string, a localized label or a
///    differently-cased spelling, is ignored. It is not read as "none": a typo
///    in a view must not silently switch a gesture off.
///
/// Behaviour, all owned by the model:
///  - The two sides always differ. Assigning the action the other side already
///    holds swaps the two instead of duplicating it, so the gesture the user
///    picked stays available in the opposite direction.
///  - Assigning the action a side already holds is a no-op and emits nothing.
///  - reset() restores the defaults, Archive on the left and Read on the right,
///    and is silent while the pair already is at them.
///  - changed() is emitted at most once per real change, so a redundant write
///    neither repaints the screen nor re-triggers a binding loop.
///
/// The facade never talks to TDLib, never logs, performs no I/O and never reads
/// the clock. Persisting the pair, and loading it back through the model, is the
/// caller's job.
class SwipeSettingsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString leftAction READ leftAction NOTIFY changed)
    Q_PROPERTY(QString rightAction READ rightAction NOTIFY changed)
    Q_PROPERTY(QStringList actions READ actions NOTIFY changed)

public:
    explicit SwipeSettingsFacade(QObject* parent = nullptr);

    /// The stored name of the left gesture, "archive" by default.
    [[nodiscard]] QString leftAction() const;

    /// The stored name of the right gesture, "read" by default.
    [[nodiscard]] QString rightAction() const;

    /// Every offered gesture's stored name, in display order. The same list on
    /// every call; it is the vocabulary setLeft() and setRight() accept.
    [[nodiscard]] QStringList actions() const;

    /// Assigns the left gesture by stored name. A name outside actions() is
    /// ignored. The model swaps the sides when the name is the action the right
    /// side already holds, and stays silent when nothing really changed.
    Q_INVOKABLE void setLeft(const QString& action);

    /// Assigns the right gesture, with the same name, swap and signal rules as
    /// setLeft().
    Q_INVOKABLE void setRight(const QString& action);

    /// Restores Archive on the left and Read on the right. Emits changed() once
    /// if anything moved and stays silent when the pair already is the default.
    Q_INVOKABLE void reset();

signals:
    /// Emitted whenever either gesture really changed.
    void changed();

private:
    telegram::ChatSwipeSettingsModel m_settings;
};

} // namespace beygram::ui
