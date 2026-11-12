#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {
class ReactionPickerModel;
}

namespace beygram::ui {

/// The reaction picker's QML-facing state.
///
/// Wraps beygram::telegram::ReactionPickerModel, which owns the catalog,
/// selection and expanded flag, and adds the two things QML must not hold
/// itself: the message the picker was opened for, and the request signal that
/// carries the chosen emoji back to the caller. The facade never touches TDLib,
/// the network or the message store; it only reports what the user picked, and
/// the owner turns that into an addMessageReaction request (an empty emoji means
/// the reaction was removed).
///
/// The catalog is supplied as plain emoji strings because the picker only offers
/// "emoji" reactions; custom-emoji and paid reactions stay out of this popup.
/// Empty strings are dropped from both lists, while order and duplicates are
/// kept exactly as the caller supplied them, matching the model's contract.
///
/// A selection only produces a request while a target is open (see openFor());
/// choosing an emoji before any message is targeted, or after the target was
/// cleared, still updates the local selection but emits nothing. clearSelection()
/// is a local reset used when the popup is dismissed and never emits a request.
class ReactionsFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList top READ top NOTIFY changed)
    Q_PROPERTY(QStringList recent READ recent NOTIFY changed)
    Q_PROPERTY(int maxCount READ maxCount NOTIFY changed)
    Q_PROPERTY(QString selected READ selected NOTIFY changed)
    Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY changed)

public:
    explicit ReactionsFacade(QObject* parent = nullptr);

    /// The top reactions in server order, as emoji.
    [[nodiscard]] QStringList top() const;

    /// The recently used reactions in server order, as emoji. May overlap top().
    [[nodiscard]] QStringList recent() const;

    /// The catalog's reaction limit, or 0 when the catalog carried none.
    [[nodiscard]] int maxCount() const;

    /// The currently selected emoji, or an empty string.
    [[nodiscard]] QString selected() const;

    /// Replaces the catalog. `topEmoji` and `recentEmoji` keep their order and
    /// duplicates; empty entries are ignored. A selection the new catalog no
    /// longer offers is dropped. Emits changed() only when something differed.
    Q_INVOKABLE void setCatalog(const QStringList& topEmoji,
                                const QStringList& recentEmoji, int maxCount);

    /// Selects `emoji`, or clears the selection when it was already selected.
    /// Emits reactionRequested() when that changed the selection and a target is
    /// open; removing the last selection reports an empty emoji. An emoji the
    /// catalog does not offer, and an empty emoji, are ignored.
    Q_INVOKABLE void toggle(const QString& emoji);

    /// Drops the local selection without emitting reactionRequested().
    Q_INVOKABLE void clearSelection();

    /// Whether the picker shows its full catalog instead of the top row.
    [[nodiscard]] bool expanded() const;
    void setExpanded(bool expanded);

    /// Targets the message the picker is open for. The selection is reset, so a
    /// stale emoji from a previously opened message is never reported; empty ids
    /// leave the facade without a target and suppress later requests.
    Q_INVOKABLE void openFor(const QString& chatId, const QString& messageId);

signals:
    void changed();

    /// Emitted for an open target when toggle() changed the selection. `emoji`
    /// is empty when the user removed their reaction.
    void reactionRequested(const QString& chatId, const QString& messageId,
                           const QString& emoji);

private:
    [[nodiscard]] bool hasTarget() const;

    telegram::ReactionPickerModel* m_model = nullptr;
    QString m_chatId;
    QString m_messageId;
};

} // namespace beygram::ui
