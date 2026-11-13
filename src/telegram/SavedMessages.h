#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of the Saved Messages list.
///
/// chatId is the chat the message lives in (the Saved Messages chat itself or
/// the chat it was forwarded from), messageId is its id inside that chat and
/// savedAt is the Unix time the entry was saved. preview is the single-line
/// text the row shows; it is stored verbatim, so the caller decides how much of
/// the message it quotes.
struct SavedMessage {
    qint64 chatId = 0;
    qint64 messageId = 0;
    qint64 savedAt = 0;
    QString preview;
};

/// Builds the forwardMessages call that copies messageIds from fromChatId into
/// Saved Messages.
///
/// Shape follows the pinned td_api.tl (TDLib 1.8.69):
///   forwardMessages chat_id:int53 topic_id:MessageTopic from_chat_id:int53
///                   message_ids:vector<int53> options:messageSendOptions
///                   send_copy:Bool remove_caption:Bool = Messages;
///
/// TDLib addresses Saved Messages like any other chat, by its id, and has no
/// "to self" flag, so this signature cannot name the target. The builder
/// therefore writes chat_id 0 as a placeholder: the caller or transport copies
/// the current user id, the "my_id" option answered by
/// getSavedMessagesChatRequest(), into chat_id before sending. TDLib rejects
/// the placeholder unchanged.
///
/// Non-positive and duplicate message ids are dropped, keeping the first
/// occurrence, because TDLib rejects both. An input without a single usable id
/// yields an empty message_ids array, which TDLib also rejects, so the caller
/// checks the selection first. fromChatId is written verbatim: it must be a
/// real chat id, and a non-positive value reaches TDLib unchanged.
///
/// The messages are forwarded as they are: send_copy and remove_caption are
/// false, so the original sender stays visible. Nothing here logs or adds
/// @extra; the transport owns correlation.
[[nodiscard]] QJsonObject forwardToSavedMessagesRequest(qint64 fromChatId,
                                                       const QVector<qint64>& messageIds);

/// Builds the request that resolves the Saved Messages chat id.
///
/// td_api.tl has no getSavedMessagesChat method: Saved Messages is the private
/// chat with the current user, and TDLib documents its id as getOption
/// ("my_id"). This builder is therefore that getOption request; its int64
/// answer is the chat_id forwardToSavedMessagesRequest() needs, and the user id
/// to pass to createPrivateChat (force: false) when the chat still has to be
/// opened.
[[nodiscard]] QJsonObject getSavedMessagesChatRequest();

/// Newest-first list of Saved Messages entries.
///
/// The class is pure state: no transport, no logging, no timers. Every
/// mutation normalises its input before storing it, so the getters always
/// return exactly what the list shows:
///  - an entry needs chatId > 0 and messageId > 0; anything else is dropped
///    because TDLib cannot address it;
///  - an entry is identified by (chatId, messageId). setItems() keeps the first
///    copy of a pair and append() ignores a pair that is already stored;
///  - entries are ordered newest first by savedAt, then by messageId
///    descending. Entries that tie on both keep their arrival order, so two
///    chats holding the same message id do not swap places between reloads;
///  - changed() is emitted only when the visible list actually changed, so a
///    repeated setItems() or a remove() of an absent entry stays silent.
class SavedMessagesModel final : public QObject {
    Q_OBJECT

public:
    explicit SavedMessagesModel(QObject* parent = nullptr);

    /// Replaces the whole list. Invalid entries are dropped, duplicate pairs
    /// keep their first copy and the result is sorted newest first. Setting the
    /// list to what it already shows is a no-op.
    void setItems(QVector<SavedMessage> items);

    /// Adds the entries that are not stored yet. An already stored pair keeps
    /// the copy it has, so a later preview or savedAt never rewrites it.
    /// Nothing is added when every pair is already present.
    void append(QVector<SavedMessage> items);

    [[nodiscard]] QVector<SavedMessage> items() const;
    [[nodiscard]] int count() const;

    /// Removes the entry with exactly this (chatId, messageId) pair. Returns
    /// true when one was removed; a pair that is not stored is a no-op.
    bool remove(qint64 chatId, qint64 messageId);

    /// Drops every entry. Clearing an empty list is a no-op.
    void clear();

signals:
    /// Emitted whenever items() or count() actually changed.
    void changed();

private:
    QVector<SavedMessage> m_items;
};

} // namespace beygram::telegram
