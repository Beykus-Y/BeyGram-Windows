#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// Holds the four editable profile fields twice: once as the server last
/// reported them (the "original") and once as the edit form currently shows
/// them (the "draft"). Everything the profile editor needs to decide whether
/// the Save button does anything lives here, so the dialog itself stays free of
/// comparison logic.
///
/// Comparison is done on the values the request builders would actually send,
/// not on the raw strings. Typing a trailing space, a line feed in the bio or a
/// leading '@' on the username therefore does not count as an edit and produces
/// no request. Trimming, whitespace folding and '@'-stripping stay the business
/// of ProfileEdit's builders; this class only asks them what they would send.
///
/// Only the current user's profile is modelled. A profile photo is edited
/// through setProfilePhoto/deleteProfilePhoto, which need media-layer state and
/// are deliberately out of scope here.
class ProfileEditController final : public QObject {
    Q_OBJECT

public:
    explicit ProfileEditController(QObject* parent = nullptr);

    /// Loads the server's copy of the profile and resets the draft to it, i.e.
    /// a fresh load starts clean. Callers that want to show in-progress text
    /// call setDraft() right after; there is no partial merge, because a reload
    /// of the profile is always a reload of the whole form.
    void setOriginal(QString first, QString last, QString username, QString bio);

    /// Records what the form shows after an edit. Values are stored as typed;
    /// normalisation happens when the values are compared or sent, so the caller
    /// can keep the raw text in its own widgets too.
    void setDraft(QString first, QString last, QString username, QString bio);

    /// True when at least one field would really change on the server. An empty
    /// bio or an empty username counts as a change when the original had text,
    /// because Telegram treats both as "clear this field".
    [[nodiscard]] bool dirty() const;

    /// The requests needed to store the draft, in a stable order:
    /// setName (first and last name travel together), setBio, setUsername.
    /// Fields that did not change are left out, so a clean controller returns an
    /// empty vector and the caller never sends a no-op request.
    [[nodiscard]] QVector<QJsonObject> requests() const;

    /// Moves the original onto the draft after the requests were accepted. The
    /// draft is kept, so the form still shows what the user typed and a further
    /// edit is measured against the freshly saved values.
    void markSaved();

signals:
    /// Emitted when the edit state changes: the draft moved to a different
    /// value, the original was reloaded, or markSaved() accepted the draft.
    /// Repeated setDraft() calls with an equivalent value (only whitespace
    /// differs) stay silent.
    void changed();

private:
    QString m_first, m_last, m_username, m_bio, o_first, o_last, o_username, o_bio;
};

} // namespace beygram::telegram
