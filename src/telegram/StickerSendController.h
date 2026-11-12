#pragma once

#include <QJsonObject>
#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Owns the target chat and the locally remembered recent stickers for the
/// sticker picker, and turns a chosen sticker into a TDLib sendMessage request
/// through sendStickerRequest().
///
/// A chat id of zero means "no target": request() then returns an empty object
/// and canSend() is false. Group and channel ids are negative in TDLib, so any
/// non-zero chat id is accepted. A reply target is normalised to zero unless it
/// is positive.
///
/// The recent list is unique and newest-first: adding an id moves it to the
/// front, invalid ids are ignored and the list is capped at the requested size.
/// changed() is emitted only when the stored state actually changed.
class StickerSendController final : public QObject {
    Q_OBJECT

public:
    explicit StickerSendController(QObject* parent = nullptr);

    /// Sets the chat the next sticker is sent to. A reply is attached only for
    /// a positive replyTo. A zero chat id is rejected and leaves the target
    /// untouched.
    void setTarget(qint64 chatId, qint64 replyTo = 0);

    /// True when a target chat is set and both ids are usable. A zero file id
    /// is valid and makes the request fall back to a remote sticker identifier;
    /// negative file ids are rejected.
    [[nodiscard]] bool canSend(qint64 stickerId, int fileId) const;

    /// Builds the sendMessage request for the current target. Returns an empty
    /// object when canSend() is false.
    [[nodiscard]] QJsonObject request(qint64 stickerId, int fileId) const;

    /// Records a sticker as the most recent one. Non-positive sticker ids and
    /// non-positive limits are ignored; the list keeps at most maxRecent
    /// entries and drops duplicates.
    void addRecent(qint64 stickerId, int maxRecent = 30);

    /// Snapshot of the remembered sticker ids, newest first.
    [[nodiscard]] QVector<qint64> recent() const;

signals:
    /// Emitted whenever the target or the recent list actually changed.
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_replyTo = 0;
    QVector<qint64> m_recent;
};

} // namespace beygram::telegram
