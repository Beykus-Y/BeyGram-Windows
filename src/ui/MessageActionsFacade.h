#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The message context menu's QML-facing state.
///
/// Wraps beygram::telegram::availableActions(): configure() describes the
/// message once, actions exposes the offered actions as stable identifiers in
/// MessageAction declaration order, and trigger() reports the user's choice as
/// one of the typed request signals. The facade never touches TDLib, the
/// network or the message store; the owner knows which message the menu was
/// opened for and turns a request signal into the corresponding request.
///
/// Identifiers are the lower-case English names of the actions ("reply",
/// "forward", "copy", "edit", "delete", "pin", "save", "react", "translate",
/// "report"). They are deliberately not localized: QML maps them to translated
/// labels, so the contract between the facade and the menu survives a change of
/// UI language.
///
/// actions() is empty until configure() is called, because the menu must not
/// guess an availability before the caller described the message. configure()
/// emits changed() only when the offered set differs, so re-configuring the
/// same message does not rebuild the menu.
///
/// trigger() only reports actions that are currently offered. An identifier
/// that is unknown, empty, or withheld for the configured message is ignored,
/// which keeps a menu left open across a message update from sending a request
/// the message no longer allows.
class MessageActionsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList actions READ actions NOTIFY changed)

public:
    explicit MessageActionsFacade(QObject* parent = nullptr);

    /// The offered actions as identifiers, in MessageAction declaration order.
    [[nodiscard]] QStringList actions() const;

    /// Describes the message the menu is for and recomputes actions().
    ///
    /// The parameters mirror beygram::telegram::ActionContext field for field,
    /// in declaration order. Emits changed() when the offered set differs from
    /// the previous one.
    Q_INVOKABLE void configure(bool outgoing, bool canSend, bool isChannel, bool isSecret,
                               bool protectedContent, bool paidMedia, bool pending, bool failed,
                               bool hasText, bool isService, bool canPin, bool canDelete,
                               bool canEdit);

    /// Reports the chosen action as its request signal. Ignored unless the
    /// action is currently offered.
    Q_INVOKABLE void trigger(const QString& action);

signals:
    void changed();

    void replyRequested();
    void forwardRequested();
    void copyRequested();
    void editRequested();
    void deleteRequested();
    void pinRequested();
    void saveRequested();
    void reactRequested();
    void translateRequested();
    void reportRequested();

private:
    QStringList m_actions;
};

} // namespace beygram::ui
