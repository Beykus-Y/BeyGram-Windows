#pragma once

#include "PresenceRequests.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// How a user's presence is shown to this client.
///
/// The values are a normalised form of the TDLib UserStatus variants that
/// PresenceInfo::status carries verbatim:
///
///   PresenceInfo::status   PresenceKind
///   "userStatusOnline"     Online        online now
///   "userStatusOffline"    Offline       last seen at PresenceInfo::lastSeen
///   "userStatusRecently"   Recently      "last seen recently"
///   "userStatusLastWeek"   LastWeek      "last seen within a week"
///   "userStatusLastMonth"  LastMonth     "last seen within a month"
///   "userStatusEmpty"      Hidden        status withheld by privacy settings
///   empty / unrecognised   Unknown       nothing usable was reported
///
/// Hidden is therefore the visible "we may not know" state, while Unknown means
/// this model has not received a status at all yet.
enum class PresenceKind {
    Unknown,
    Online,
    Offline,
    Recently,
    LastWeek,
    LastMonth,
    Hidden,
};

/// Holds the presence of one user and turns it into a display label.
///
/// Only Offline carries a timestamp: every other kind forces lastSeenUnix() back
/// to 0, because "last seen" is meaningless for an online or a withheld status.
/// All mutators are no-ops when the normalised values would not change, so
/// changed() is emitted once per real change and status views never repaint for
/// nothing. There is no logging and no session state here.
class UserStatusModel final : public QObject {
    Q_OBJECT

public:
    explicit UserStatusModel(QObject* parent = nullptr);

    /// Stores the new kind. lastSeenUnix is kept only for Offline and is clamped
    /// to 0 when negative; any other kind stores 0.
    void setStatus(PresenceKind kind, qint64 lastSeenUnix = 0);

    /// Stores the result of presenceFromUser(). The TDLib status string is
    /// mapped as documented on PresenceKind and lastSeen is carried over for
    /// userStatusOffline. This is the only bridge between the transport-shaped
    /// PresenceInfo and the display-shaped PresenceKind.
    void setStatus(const PresenceInfo& presence);

    [[nodiscard]] PresenceKind kind() const { return m_kind; }

    /// Unix seconds of the last seen time, or 0 for every kind but Offline.
    [[nodiscard]] qint64 lastSeenUnix() const { return m_lastSeen; }

    /// True only for PresenceKind::Online.
    [[nodiscard]] bool online() const { return m_kind == PresenceKind::Online; }

    /// Label for the UI: "online", "last seen recently", "last seen within a
    /// week", "last seen within a month" or "offline". Empty for Unknown and
    /// Hidden, which have nothing to show.
    [[nodiscard]] QString label() const;

    /// Resets to Unknown with no timestamp. Does nothing, and emits nothing,
    /// when the model is already in that state.
    void clear();

signals:
    /// Emitted whenever kind() or lastSeenUnix() actually changed.
    void changed();

private:
    PresenceKind m_kind = PresenceKind::Unknown;
    qint64 m_lastSeen = 0;
};

} // namespace beygram::telegram
