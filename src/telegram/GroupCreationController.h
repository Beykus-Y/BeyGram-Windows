#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// What is being created. A basic group is a TDLib basic group, while
/// Supergroup and Channel both become supergroups on the server; only the
/// is_channel flag of createNewSupergroupChat tells them apart.
enum class GroupKind { Group, Supergroup, Channel };

/// Holds the draft of one group, supergroup or channel creation and turns it
/// into the ordered request plan that creates and configures the chat.
///
/// Lifecycle: begin(kind) starts a fresh draft and discards whatever was there
/// before, reset() abandons it. active() is true only between those two calls.
/// Every setter is ignored while no draft is active, so a value can never
/// belong to a creation that was already abandoned; begin() always starts from
/// an empty title, description, username and member list.
///
/// Normalisation happens on input, so the getters return exactly what
/// requests() would send:
///   - title is trimmed and cut to 128 Unicode code points;
///   - description is cut to 255 code points but is not trimmed, because the
///     contract only bounds its length;
///   - username is trimmed, lower cased and loses leading '@' characters;
///   - members are positive user ids, added once each, at most 200.
///
/// valid() requires an active draft with a non-empty title and, when the draft
/// is public, a username of 5 to 32 characters from [a-z0-9_]. Members are
/// optional: Telegram accepts a basic group that starts with only its creator.
///
/// requests() is empty unless valid(). The first request creates the chat and
/// the rest configure it and add its members, in this order:
///
///   createNewBasicGroupChat / createNewSupergroupChat
///   setChatTitle
///   setChatDescription
///   setChatUsername            (public drafts only)
///   addChatMember              (one per member, in insertion order)
///
/// The create request is sent without members, so every member is added by its
/// own addChatMember and a failed invitation can be reported per user.
///
/// The new chat id does not exist until TDLib answers the create request, so
/// every following request carries chat_id 0 as a placeholder. The caller
/// replaces that placeholder with the id from the create response before
/// sending; the vector is an ordered plan, not a batch to send unchanged.
/// setChatUsername is the contract's name for the username step: TDLib spells
/// the operation setSupergroupUsername and addresses it by supergroup_id, so
/// the transport maps the step when it sends the plan.
///
/// Nothing here logs, sends or adds @extra: the transport owns correlation.
class GroupCreationController final : public QObject {
    Q_OBJECT

public:
    explicit GroupCreationController(QObject* parent = nullptr);

    /// Starts a fresh draft of the given kind. An existing draft, even of the
    /// same kind, is discarded rather than merged. Silent when the controller
    /// is already active with exactly this empty draft.
    void begin(GroupKind kind);

    /// Abandons the draft and returns to the inactive default state, including
    /// kind() == Group. Silent when there is nothing to discard.
    void reset();

    /// True between begin() and reset().
    [[nodiscard]] bool active() const;

    /// The kind chosen by the last begin(), or Group when inactive.
    [[nodiscard]] GroupKind kind() const;

    /// Records the title. The value is trimmed and cut to 128 code points, so
    /// a whitespace-only title normalises to empty and makes valid() false.
    /// Ignored while inactive; emits changed() only when the stored value
    /// differs.
    void setTitle(const QString& title);

    /// The normalised title, empty when none is set.
    [[nodiscard]] QString title() const;

    /// Records the description. The value is kept as typed except for the
    /// 255-code-point cap; an empty description is allowed. Ignored while
    /// inactive; emits changed() only when the stored value differs.
    void setDescription(const QString& description);

    /// The description, capped at 255 code points, empty when none is set.
    [[nodiscard]] QString description() const;

    /// Records whether the chat is public and the username to claim. The
    /// username is normalised even when isPublic is false, so switching a
    /// draft private and back does not lose it; it is only sent while public.
    /// Ignored while inactive; emits changed() once when either value differs.
    void setPublic(bool isPublic, const QString& username);

    /// True when the draft asks for a public username.
    [[nodiscard]] bool isPublic() const;

    /// The normalised username, empty when none is set.
    [[nodiscard]] QString username() const;

    /// Adds one member. Returns false, without changing anything, for a
    /// non-positive id, an id that is already in the list, a full list of 200
    /// or an inactive draft. Emits changed() on success.
    bool addMember(qint64 userId);

    /// Removes one member. Returns false when the id is not in the list or the
    /// draft is inactive. Emits changed() on success.
    bool removeMember(qint64 userId);

    /// The members in insertion order.
    [[nodiscard]] QVector<qint64> members() const;

    /// True when the draft can be sent: active, a non-empty title and, for a
    /// public draft, a well-formed username. Members are not required.
    [[nodiscard]] bool valid() const;

    /// The ordered request plan described above, or an empty vector when
    /// valid() is false. Asking twice returns the same plan and changes no
    /// state.
    [[nodiscard]] QVector<QJsonObject> requests() const;

signals:
    /// Emitted whenever the observable draft state actually changed.
    void changed();

private:
    GroupKind m_kind = GroupKind::Group;
    QString m_title, m_description, m_username;
    bool m_public = false;
    QVector<qint64> m_members;
    bool m_active = false;
};

} // namespace beygram::telegram
