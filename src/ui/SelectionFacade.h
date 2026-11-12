#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {
class ForwardTargetModel;
class MessageJumpController;
class MessageSelectionController;
} // namespace beygram::telegram

namespace beygram::ui {

/// QML-facing facade over the message selection, forward picker and jump
/// controllers. It translates between the string ids QML uses and the qint64
/// ids the controllers speak, and re-emits a single changed() notification so
/// bindings do not have to watch three separate objects.
///
/// The facade owns its controllers. It holds no transport and performs no
/// logging; forwarding and jumping are only announced through
/// forwardRequested() and jumpRequested() for the caller to act on.
///
/// Invariants:
///  - active() is true exactly when at least one message is selected.
///  - ids() is the selection in ascending numeric order.
///  - forwardOpen() is true exactly while a forward session is open.
///  - canForward() implies forwardOpen() and a selected target chat.
///  - highlightId() is empty unless a jump is in flight.
class SelectionFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QStringList ids READ ids NOTIFY changed)
    Q_PROPERTY(bool forwardOpen READ forwardOpen NOTIFY changed)
    Q_PROPERTY(QString forwardFilter READ forwardFilter WRITE setForwardFilter NOTIFY changed)
    Q_PROPERTY(bool canForward READ canForward NOTIFY changed)

public:
    explicit SelectionFacade(QObject* parent = nullptr);

    [[nodiscard]] bool active() const;
    [[nodiscard]] int count() const;
    [[nodiscard]] QStringList ids() const;

    /// Adds the message when it is not selected yet, removes it otherwise.
    Q_INVOKABLE void toggle(const QString& messageId);

    /// Adds the message when it is not selected yet; selecting twice is a no-op.
    Q_INVOKABLE void select(const QString& messageId);

    /// Drops the whole selection and any open forward session.
    Q_INVOKABLE void clear();

    /// Opens the forward picker for the current selection. Ignored when the
    /// selection is empty.
    Q_INVOKABLE void beginForward();

    /// Closes the forward picker and drops its candidates, filter and target.
    Q_INVOKABLE void cancelForward();

    [[nodiscard]] bool forwardOpen() const;
    [[nodiscard]] QString forwardFilter() const;
    void setForwardFilter(const QString& filter);

    /// Replaces the candidate chats. Each entry is a chat id as a string; the
    /// id doubles as the chat title so the filter can match it.
    Q_INVOKABLE void setForwardChats(const QStringList& chatIds);

    /// Selects the target chat. When the picker is then ready, emits
    /// forwardRequested() with the chat id and the selected message ids.
    Q_INVOKABLE void chooseForwardChat(const QString& chatId);

    [[nodiscard]] bool canForward() const;

    /// Starts a jump to a message and announces it through jumpRequested().
    Q_INVOKABLE void beginJump(const QString& chatId, const QString& messageId);

    /// Completes the jump and clears the highlight.
    Q_INVOKABLE void finishJump();

    Q_PROPERTY(QString highlightId READ highlightId NOTIFY changed)
    [[nodiscard]] QString highlightId() const;

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted when the user picked a forward target for the selection.
    void forwardRequested(const QString& chatId, const QStringList& messageIds);

    /// Emitted when a jump to a message was started.
    void jumpRequested(const QString& chatId, const QString& messageId);

private:
    telegram::MessageSelectionController* m_selection = nullptr;
    telegram::ForwardTargetModel* m_forward = nullptr;
    telegram::MessageJumpController* m_jump = nullptr;
};

} // namespace beygram::ui
