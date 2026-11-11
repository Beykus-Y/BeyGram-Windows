#include "TelegramRepository.h"

#include "core/Logging.h"
#include "core/Redaction.h"
#include "MediaFileStore.h"
#include "MessageReactions.h"
#include "MessageSearch.h"
#include "NotificationFormatter.h"
#include "PinnedMessages.h"
#include "ChatFolders.h"
#include "FolderChats.h"
#include "TelegramParseWorker.h"
#include "TelegramRequests.h"
#include "TelegramSession.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>

#include <algorithm>

namespace beygram::telegram {
namespace {
Q_LOGGING_CATEGORY(lcRepo, "beygram.telegram.repository")

constexpr int ChatPageSize = 100;
constexpr int MessagePageSize = 50;

bool isPrivateChat(const QString& type)
{
    return type == QLatin1String("chatTypePrivate") || type == QLatin1String("chatTypeSecret");
}

QString senderNameFromUser(const QJsonObject& user)
{
    const QString first = user.value(QStringLiteral("first_name")).toString().trimmed();
    const QString last = user.value(QStringLiteral("last_name")).toString().trimmed();
    if (!first.isEmpty() && !last.isEmpty())
        return first + QLatin1Char(' ') + last;
    return first.isEmpty() ? last : first;
}

QString chatTypeLabel(const QString& type)
{
    if (type == QLatin1String("chatTypePrivate")) return QStringLiteral("Private chat");
    if (type == QLatin1String("chatTypeSecret")) return QStringLiteral("Secret chat");
    if (type == QLatin1String("chatTypeBasicGroup")) return QStringLiteral("Group");
    if (type == QLatin1String("chatTypeSupergroup")) return QStringLiteral("Group");
    return QStringLiteral("Telegram");
}
} // namespace

TelegramRepository::TelegramRepository(TelegramSession* session, ChatListModel* chatModel,
                                       MessageListModel* messageModel, MediaFileStore* mediaStore,
                                       QObject* parent)
    : QObject(parent),
      m_session(session),
      m_chatModel(chatModel),
      m_messageModel(messageModel),
      m_mediaStore(mediaStore)
{
    connect(m_session, &TelegramSession::updateReceived, this,
            &TelegramRepository::handleUpdate);
    connect(&m_scheduler, &TelegramUpdateScheduler::batchReady, this,
            &TelegramRepository::processUpdates);

    m_chatListRebuildTimer = new QTimer(this);
    m_chatListRebuildTimer->setSingleShot(true);
    m_chatListRebuildTimer->setInterval(120);
    connect(m_chatListRebuildTimer, &QTimer::timeout, this, [this] {
        m_chatListRebuildPending = false;
        rebuildChatListNow();
    });

    // Parsing and text formatting run off the GUI thread. TDLib still receives
    // on its own thread, and the parse thread hands immutable snapshots back
    // through queued signals.
    m_parseThread = new QThread;
    m_parseWorker = new TelegramParseWorker;
    m_parseWorker->moveToThread(m_parseThread);
    connect(m_parseThread, &QThread::finished, m_parseWorker, &QObject::deleteLater);
    connect(this, &TelegramRepository::parseChatRequested, m_parseWorker,
            &TelegramParseWorker::parseChat);
    connect(this, &TelegramRepository::parseHistoryRequested, m_parseWorker,
            &TelegramParseWorker::parseHistory);
    connect(m_parseWorker, &TelegramParseWorker::chatParsed, this,
            &TelegramRepository::applyParsedChat);
    connect(m_parseWorker, &TelegramParseWorker::historyParsed, this,
            &TelegramRepository::applyHistory);
    m_parseThread->start();

    if (m_mediaStore) {
        connect(m_mediaStore, &MediaFileStore::fileReady, this, [this](int fileId) {
            for (ChatData& chat : m_chats) {
                if (chat.photoFileId != fileId)
                    continue;
                chat.photoReady = true;
                ++chat.photoRevision;
            }
            m_chatModel->markPhotoReady(fileId);
            m_messageModel->markMediaReady(fileId);
            if (fileId == m_currentChatPhotoFileId) {
                m_currentChatPhotoReady = true;
                ++m_currentChatPhotoRevision;
                emit currentChatChanged();
            }
            emit stateChanged();
        });
    }
}

TelegramRepository::~TelegramRepository()
{
    if (!m_parseThread)
        return;
    m_parseThread->quit();
    m_parseThread->wait();
    m_parseWorker = nullptr;
    delete m_parseThread;
    m_parseThread = nullptr;
}

void TelegramRepository::start()
{
    if (m_started)
        return;
    m_started = true;
    qCDebug(lcRepo) << "Starting chat repository";
    m_loadingChats = true;
    emit stateChanged();
    requestChats();
}

void TelegramRepository::requestChats()
{
    m_loadingChats = true;
    emit stateChanged();

    auto requestLocalChats = [this] {
        sendRequest(
            QJsonObject{{QStringLiteral("@type"), QStringLiteral("getChats")},
                        {QStringLiteral("chat_list"), QJsonValue::Null},
                        {QStringLiteral("limit"), ChatPageSize}},
            [this](const QJsonObject& chatsResponse) {
                if (chatsResponse.value(QStringLiteral("@type")).toString()
                    == QLatin1String("error")) {
                    setError(QStringLiteral("Could not read the chat list: %1")
                                 .arg(chatsResponse.value(QStringLiteral("message")).toString()));
                    emit stateChanged();
                    return;
                }

                const QJsonArray ids = chatsResponse.value(QStringLiteral("chat_ids")).toArray();
                qCInfo(lcRepo) << "getChats returned" << ids.size() << "ids";
                for (const QJsonValue& value : ids) {
                    const qint64 chatId = jsonInt64(value);
                    auto it = m_chats.find(chatId);
                    if (it == m_chats.end()) {
                        if (!m_requestedChats.contains(chatId)) {
                            m_requestedChats.insert(chatId);
                            sendRequest(
                                QJsonObject{{QStringLiteral("@type"), QStringLiteral("getChat")},
                                            {QStringLiteral("chat_id"), chatId}},
                                [this, chatId](const QJsonObject& chatResponse) {
                                    if (chatResponse.value(QStringLiteral("@type")).toString()
                                        == QLatin1String("chat")) {
                                        enqueueChatParse(chatId, chatResponse);
                                    } else {
                                        m_requestedChats.remove(chatId);
                                    }
                                },
                                QStringLiteral("Could not read a chat"));
                        }
                    } else {
                        it->inMainList = true;
                    }
                }
                rebuildChatList();
                emit stateChanged();
            },
            QStringLiteral("Could not read chats"));
    };

    // Read what is already in the local TDLib database first, then ask TDLib
    // to load the remaining chat list from the server. Updates fill the model
    // while the server request is in flight.
    requestLocalChats();

    sendRequest(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("loadChats")},
                    {QStringLiteral("chat_list"), QJsonValue::Null},
                    {QStringLiteral("limit"), ChatPageSize}},
        [this, requestLocalChats](const QJsonObject& response) {
            const QString type = response.value(QStringLiteral("@type")).toString();
            if (type == QLatin1String("error")) {
                const int code = response.value(QStringLiteral("code")).toInt();
                if (code != 404) {
                    setError(QStringLiteral("Could not load the chat list: %1")
                                 .arg(response.value(QStringLiteral("message")).toString()));
                }
            }
            m_loadingChats = false;
            requestLocalChats();
            emit stateChanged();
        },
        QStringLiteral("Could not load chats"), 180000);
}

void TelegramRepository::rebuildChatList()
{
    m_chatListRebuildPending = true;
    if (m_chatListRebuildTimer && !m_chatListRebuildTimer->isActive())
        m_chatListRebuildTimer->start();
}

void TelegramRepository::rebuildChatListNow()
{
    QVector<ChatData> chats;
    chats.reserve(m_chats.size());
    const QSet<qint64> folderIds = m_currentFolderId != 0
        ? m_folderChatIds.value(m_currentFolderId)
        : QSet<qint64>{};
    for (const ChatData& chat : m_chats) {
        if (m_currentFolderId != 0) {
            if (!folderIds.contains(chat.id))
                continue;
        } else if (!chat.inMainList || chat.order == 0) {
            continue;
        }
        chats.append(chat);
    }

    std::sort(chats.begin(), chats.end(), [](const ChatData& left, const ChatData& right) {
        if (left.order != right.order)
            return left.order > right.order;
        if (left.lastMessageDate != right.lastMessageDate)
            return left.lastMessageDate > right.lastMessageDate;
        return left.title.localeAwareCompare(right.title) < 0;
    });

    m_chatModel->setChats(std::move(chats));
}

void TelegramRepository::enqueueChatParse(qint64 chatId, const QJsonObject& chat,
                                          bool inMainList)
{
    const quint64 jobId = ++m_nextParseJob;
    m_parseChatContext.insert(jobId, inMainList);
    emit parseChatRequested(jobId, chatId, chat);
}

void TelegramRepository::applyParsedChat(quint64 jobId, qint64 chatId, const ChatData& chat)
{
    const bool inMainList = m_parseChatContext.take(jobId);
    m_requestedChats.remove(chatId);
    if (chat.id == 0)
        return;
    ChatData data = chat;
    const auto existing = m_chats.constFind(chatId != 0 ? chatId : data.id);
    data.inMainList = existing != m_chats.cend() ? existing->inMainList : inMainList;
    if (m_mediaStore)
        data.photoReady = m_mediaStore->isReady(data.photoFileId);
    if (existing != m_chats.cend() && existing->photoFileId == data.photoFileId) {
        // Keep the revision that already made the avatar visible. Resetting it
        // here used to swap the image URL and made avatars flicker on refresh.
        data.photoReady = data.photoReady || existing->photoReady;
        data.photoRevision = existing->photoRevision;
    }
    m_chats.insert(chatId != 0 ? chatId : data.id, data);
    rebuildChatList();
    emit stateChanged();
}

void TelegramRepository::openChat(qint64 chatId)
{
    if (chatId == 0)
        return;
    if (chatId == m_currentChatId && m_messageModel->count() > 0)
        return;
    if (chatId != m_currentChatId)
        m_lastReadMessageId = 0;

    const ChatData chat = m_chats.value(chatId);
    m_currentChatId = chatId;
    m_currentChatTitle = chat.title;
    m_currentChatType = chat.type;
    m_currentChatSubtitle = chatTypeLabel(chat.type);
    m_currentChatPhotoFileId = chat.photoFileId;
    m_currentChatPhotoReady = chat.photoReady;
    m_currentChatPhotoRevision = chat.photoRevision;
    if (m_mediaStore && chat.photoFileId != 0) {
        m_mediaStore->trackFileId(chat.photoFileId, 8);
        m_currentChatPhotoReady = m_mediaStore->isReady(chat.photoFileId);
    }
    m_messageModel->clear();
    m_oldestMessageId = 0;
    m_hasMoreHistory = true;
    m_historyLoading = true;
    m_error.clear();
    clearSearch();
    m_pinnedMessageId = 0;
    emit pinnedMessageChanged(0, QString());
    emit currentChatChanged();
    emit stateChanged();

    requestHistory(chatId, 0, false);
    requestPinnedMessage();
}

void TelegramRepository::closeChat()
{
    if (m_currentChatId == 0)
        return;
    m_currentChatId = 0;
    m_currentChatTitle.clear();
    m_currentChatSubtitle.clear();
    m_currentChatType.clear();
    m_currentChatPhotoFileId = 0;
    m_currentChatPhotoReady = false;
    m_currentChatPhotoRevision = 0;
    m_messageModel->clear();
    m_oldestMessageId = 0;
    m_lastReadMessageId = 0;
    m_hasMoreHistory = true;
    clearSearch();
    emit currentChatChanged();
    emit stateChanged();
}

void TelegramRepository::loadOlder()
{
    if (m_currentChatId == 0 || m_historyLoading || !m_hasMoreHistory
        || m_oldestMessageId == 0) {
        return;
    }
    m_historyLoading = true;
    emit stateChanged();
    requestHistory(m_currentChatId, m_oldestMessageId, true);
}

void TelegramRepository::requestHistory(qint64 chatId, qint64 fromMessageId, bool older)
{
    const quint64 jobId = ++m_historyJob;
    sendRequest(
        getChatHistoryRequest(chatId, fromMessageId, MessagePageSize, false),
        [this, chatId, older, jobId](const QJsonObject& response) {
            handleHistory(response, chatId, older, jobId);
        },
        older ? QStringLiteral("Could not load older messages")
              : QStringLiteral("Could not load message history"));
}

void TelegramRepository::handleHistory(const QJsonObject& response, qint64 chatId,
                                       bool older, quint64 jobId)
{
    if (chatId != m_currentChatId || jobId != m_historyJob)
        return;

    if (response.value(QStringLiteral("@type")).toString() == QLatin1String("error")) {
        m_historyLoading = false;
        setError(QStringLiteral("Could not load messages: %1")
                     .arg(response.value(QStringLiteral("message")).toString()));
        emit stateChanged();
        return;
    }

    const QJsonArray messages = response.value(QStringLiteral("messages")).toArray();
    emit parseHistoryRequested(jobId, chatId, older, messages);
}

void TelegramRepository::applyHistory(quint64 jobId, qint64 chatId, bool older,
                                      const QVector<MessageData>& parsed)
{
    if (chatId != m_currentChatId || jobId != m_historyJob)
        return;

    QVector<MessageData> messages = parsed;
    qCDebug(lcRepo) << "history page parsed off-thread" << messages.size()
                    << "older" << older;
    for (MessageData& message : messages) {
        trackMessageMedia(message);
        if (isPrivateChat(m_currentChatType) && !message.outgoing)
            message.senderName = m_currentChatTitle;
        resolveSender(message);
    }

    if (older) {
        for (const MessageData& message : messages)
            m_messageModel->addOrUpdate(message);
    } else {
        m_messageModel->setMessages(messages);
        m_lastReadMessageId = 0;
    }
    m_messageModel->resolveReplyPreviews();

    if (!messages.isEmpty())
        m_oldestMessageId = m_messageModel->firstMessageId();
    else
        m_hasMoreHistory = false;

    if (messages.size() < MessagePageSize)
        m_hasMoreHistory = false;

    m_historyLoading = false;
    if (!older) {
        markCurrentChatRead();
        emit initialHistoryLoaded();
    }
    emit stateChanged();
}

void TelegramRepository::searchInChat(const QString& query)
{
    if (m_currentChatId == 0) {
        clearSearch();
        return;
    }
    const QString body = query.trimmed();
    if (body.isEmpty()) {
        clearSearch();
        return;
    }

    m_searchQuery = body;
    m_searchLoading = true;
    emit stateChanged();
    const qint64 chatId = m_currentChatId;
    sendRequest(
        searchChatMessagesRequest(chatId, body, 0, 50),
        [this, chatId, body](const QJsonObject& response) {
            if (chatId != m_currentChatId || body != m_searchQuery)
                return;
            m_searchLoading = false;
            if (response.value(QStringLiteral("@type")).toString()
                == QLatin1String("error")) {
                m_searchModel.clear();
                emit errorOccurred(QStringLiteral("Search failed: %1")
                                       .arg(response.value(QStringLiteral("message")).toString()));
            } else {
                m_searchModel.setHits(searchHitsFromResponse(response));
            }
            emit stateChanged();
        },
        QStringLiteral("Could not search messages"));
}

void TelegramRepository::clearSearch()
{
    m_searchQuery.clear();
    m_searchLoading = false;
    m_searchModel.clear();
    emit stateChanged();
}

void TelegramRepository::requestProfile(qint64 chatId)
{
    if (chatId == 0)
        return;

    sendRequest(
        getChatRequest(chatId),
        [this, chatId](const QJsonObject& chatResponse) {
            if (chatResponse.value(QStringLiteral("@type")).toString()
                != QLatin1String("chat")) {
                return;
            }

            const QJsonObject chatType =
                chatResponse.value(QStringLiteral("type")).toObject();
            const QString type = chatType.value(QStringLiteral("@type")).toString();

            if (type == QLatin1String("chatTypePrivate")
                || type == QLatin1String("chatTypeSecret")) {
                const qint64 userId = jsonInt64(chatType.value(QStringLiteral("user_id")));
                if (userId == 0) {
                    emit profileLoaded(profileFromChat(chatResponse));
                    return;
                }
                sendRequest(
                    getUserRequest(userId),
                    [this, chatId, userId, chatResponse](const QJsonObject& userResponse) {
                        if (userResponse.value(QStringLiteral("@type")).toString()
                            != QLatin1String("user")) {
                            return;
                        }
                        sendRequest(
                            getUserFullInfoRequest(userId),
                            [this, chatId, userResponse](const QJsonObject& fullResponse) {
                                const QJsonObject full =
                                    fullResponse.value(QStringLiteral("@type")).toString()
                                            == QLatin1String("userFullInfo")
                                        ? fullResponse
                                        : QJsonObject{};
                                UserProfile profile = profileFromUser(userResponse, full);
                                profile.chatId = chatId;
                                emit profileLoaded(profile);
                            },
                            QStringLiteral("Could not load profile details"));
                    },
                    QStringLiteral("Could not load the user profile"));
                return;
            }

            if (type == QLatin1String("chatTypeSupergroup")) {
                const qint64 supergroupId =
                    jsonInt64(chatType.value(QStringLiteral("supergroup_id")));
                if (supergroupId != 0) {
                    sendRequest(
                        getSupergroupFullInfoRequest(supergroupId),
                        [this, chatResponse](const QJsonObject& fullResponse) {
                            const QJsonObject full =
                                fullResponse.value(QStringLiteral("@type")).toString()
                                        == QLatin1String("supergroupFullInfo")
                                    ? fullResponse
                                    : QJsonObject{};
                            emit profileLoaded(profileFromChat(chatResponse, full));
                        },
                        QStringLiteral("Could not load group details"));
                    return;
                }
            }

            emit profileLoaded(profileFromChat(chatResponse));
        },
        QStringLiteral("Could not load the profile"));
}

void TelegramRepository::selectFolder(qint64 folderId)
{
    if (folderId == m_currentFolderId)
        return;
    m_currentFolderId = folderId;
    if (folderId == 0) {
        rebuildChatList();
        emit stateChanged();
        return;
    }

    sendRequest(
        getChatsForFolderRequest(folderId, 100),
        [this, folderId](const QJsonObject& response) {
            if (folderId != m_currentFolderId)
                return;
            if (!responseIsChats(response)) {
                m_folderChatIds.remove(folderId);
                rebuildChatList();
                emit stateChanged();
                return;
            }

            const QVector<qint64> ids = chatIdsFromResponse(response);
            QSet<qint64> folderSet;
            for (qint64 id : ids)
                folderSet.insert(id);
            m_folderChatIds.insert(folderId, folderSet);

            for (qint64 chatId : ids) {
                if (m_chats.contains(chatId) || m_requestedChats.contains(chatId))
                    continue;
                m_requestedChats.insert(chatId);
                sendRequest(
                    QJsonObject{{QStringLiteral("@type"), QStringLiteral("getChat")},
                                {QStringLiteral("chat_id"), chatId}},
                    [this, chatId](const QJsonObject& chatResponse) {
                        if (chatResponse.value(QStringLiteral("@type")).toString()
                            == QLatin1String("chat")) {
                            enqueueChatParse(chatId, chatResponse, false);
                        } else {
                            m_requestedChats.remove(chatId);
                        }
                    },
                    QStringLiteral("Could not read a folder chat"));
            }

            rebuildChatList();
            emit stateChanged();
        },
        QStringLiteral("Could not load the chat folder"));
}

void TelegramRepository::sendText(const QString& text, qint64 replyToMessageId)
{
    if (m_currentChatId == 0)
        return;
    const QString body = text.trimmed();
    if (body.isEmpty())
        return;

    m_error.clear();
    sendRequest(
        sendTextMessageRequest(m_currentChatId, body, replyToMessageId),
        [this, chatId = m_currentChatId](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString() == QLatin1String("message")) {
                MessageData message = messageFromJson(response);
                trackMessageMedia(message);
                if (chatId == m_currentChatId) {
                    resolveSender(message);
                    m_messageModel->addOrUpdate(message);
                    m_messageModel->resolveReplyPreviews();
                    markCurrentChatRead();
                }
            } else if (response.value(QStringLiteral("@type")).toString()
                       == QLatin1String("error")) {
                setError(QStringLiteral("Could not send the message: %1")
                             .arg(response.value(QStringLiteral("message")).toString()));
            }
        },
        QStringLiteral("Could not send the message"), 120000);
    emit stateChanged();
}

void TelegramRepository::sendMedia(OutgoingMediaKind kind, const QString& localPath,
                                   const QString& caption, qint64 replyToMessageId)
{
    if (m_currentChatId == 0)
        return;
    if (!isSupportedMediaPath(localPath, kind)) {
        setError(QStringLiteral("Unsupported or missing media file"));
        emit stateChanged();
        return;
    }

    m_error.clear();
    sendRequest(
        sendMediaMessageRequest(m_currentChatId, kind, localPath, caption.trimmed(),
                                replyToMessageId),
        [this, chatId = m_currentChatId](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString() == QLatin1String("message")) {
                MessageData message = messageFromJson(response);
                trackMessageMedia(message);
                if (chatId == m_currentChatId) {
                    resolveSender(message);
                    m_messageModel->addOrUpdate(message);
                    m_messageModel->resolveReplyPreviews();
                    markCurrentChatRead();
                }
            } else if (response.value(QStringLiteral("@type")).toString()
                       == QLatin1String("error")) {
                setError(QStringLiteral("Could not send the media: %1")
                             .arg(response.value(QStringLiteral("message")).toString()));
            }
            emit stateChanged();
        },
        QStringLiteral("Could not send the media"), 120000);
    emit stateChanged();
}

void TelegramRepository::sendReaction(qint64 chatId, qint64 messageId,
                                      const QString& emoji)
{
    if (chatId == 0 || messageId == 0 || emoji.isEmpty())
        return;
    sendRequest(sendReactionRequest(chatId, messageId, emoji),
                [this](const QJsonObject& response) {
                    if (response.value(QStringLiteral("@type")).toString()
                        == QLatin1String("error")) {
                        setError(QStringLiteral("Could not update the reaction: %1")
                                     .arg(response.value(QStringLiteral("message")).toString()));
                    }
                    emit stateChanged();
                },
                QStringLiteral("Could not update the reaction"));
}

void TelegramRepository::requestPinnedMessage()
{
    if (m_currentChatId == 0)
        return;
    const qint64 chatId = m_currentChatId;
    sendRequest(
        getChatPinnedMessageRequest(chatId),
        [this, chatId](const QJsonObject& response) {
            if (chatId != m_currentChatId)
                return;
            if (response.value(QStringLiteral("@type")).toString()
                == QLatin1String("message")) {
                const MessageData message = messageFromJson(response);
                m_pinnedMessageId = message.id;
                QString preview = message.text;
                if (preview.isEmpty())
                    preview = QStringLiteral("[%1]").arg(message.contentType);
                emit pinnedMessageChanged(message.id, preview);
            } else {
                m_pinnedMessageId = 0;
                emit pinnedMessageChanged(0, QString());
            }
        },
        QStringLiteral("Could not load the pinned message"));
}

void TelegramRepository::unpinPinnedMessage()
{
    if (m_currentChatId == 0 || m_pinnedMessageId == 0)
        return;
    const qint64 chatId = m_currentChatId;
    const qint64 messageId = m_pinnedMessageId;
    m_pinnedMessageId = 0;
    emit pinnedMessageChanged(0, QString());
    sendRequest(unpinChatMessageRequest(chatId, messageId),
                [this](const QJsonObject& response) {
                    if (response.value(QStringLiteral("@type")).toString()
                        == QLatin1String("error")) {
                        setError(QStringLiteral("Could not unpin the message: %1")
                                     .arg(response.value(QStringLiteral("message")).toString()));
                    }
                    emit stateChanged();
                },
                QStringLiteral("Could not unpin the message"));
}

void TelegramRepository::editMessage(qint64 messageId, const QString& text)
{
    if (m_currentChatId == 0 || messageId == 0)
        return;
    const QString body = text.trimmed();
    if (body.isEmpty())
        return;

    sendRequest(
        editTextMessageRequest(m_currentChatId, messageId, body),
        [this, messageId](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString() == QLatin1String("message")) {
                m_messageModel->updateContent(
                    messageId, response.value(QStringLiteral("content")).toObject(), true);
            } else if (response.value(QStringLiteral("@type")).toString()
                       == QLatin1String("error")) {
                setError(QStringLiteral("Could not edit the message: %1")
                             .arg(response.value(QStringLiteral("message")).toString()));
            }
        },
        QStringLiteral("Could not edit the message"));
}

void TelegramRepository::deleteMessage(qint64 messageId)
{
    if (m_currentChatId == 0 || messageId == 0)
        return;

    sendRequest(
        deleteMessagesRequest(m_currentChatId, QJsonArray{messageId}, true),
        [this, messageId](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString() == QLatin1String("error")) {
                setError(QStringLiteral("Could not delete the message: %1")
                             .arg(response.value(QStringLiteral("message")).toString()));
                return;
            }
            m_messageModel->removeIds({messageId});
        },
        QStringLiteral("Could not delete the message"));
}

void TelegramRepository::markCurrentChatRead()
{
    if (m_currentChatId == 0 || m_lastReadMessageId == 0)
        return;
    const qint64 newest = m_messageModel->lastMessageId();
    if (newest == 0 || newest <= m_lastReadMessageId)
        return;

    QJsonArray ids;
    const int count = m_messageModel->count();
    for (int row = qMax(0, count - 20); row < count; ++row) {
        const qint64 id = m_messageModel->index(row).data(MessageListModel::MessageIdRole).toLongLong();
        if (id > 0)
            ids.append(id);
    }
    if (ids.isEmpty())
        return;

    m_lastReadMessageId = newest;
    sendRequest(
        viewMessagesRequest(m_currentChatId, ids, false),
        [](const QJsonObject&) {}, QStringLiteral("Could not mark messages as read"));
}

void TelegramRepository::handleUpdate(const QJsonObject& update)
{
    if (!m_started)
        return;
    m_scheduler.enqueue(update);
}

void TelegramRepository::processUpdates(const QVector<QJsonObject>& updates)
{
    if (updates.size() > 1)
        qCDebug(lcRepo) << "scheduler batch" << updates.size();
    for (const QJsonObject& update : updates)
        processUpdate(update);
}

void TelegramRepository::processUpdate(const QJsonObject& update)
{
    const QString type = update.value(QStringLiteral("@type")).toString();

    if (type.startsWith(QLatin1String("updateChat")) || type == QLatin1String("updateNewChat")
        || type == QLatin1String("updateNewMessage")) {
        qCDebug(lcRepo) << "Update" << type;
    }

    if (type == QLatin1String("updateChatFolders")) {
        const ChatFoldersState folders = chatFoldersFromUpdate(update);
        m_folderModel.setState(folders);
        bool stillExists = m_currentFolderId == 0;
        for (const ChatFolderInfo& folder : folders.folders) {
            if (folder.id == m_currentFolderId)
                stillExists = true;
        }
        if (!stillExists) {
            m_currentFolderId = 0;
            rebuildChatList();
        }
        emit stateChanged();
        return;
    }

    if (type == QLatin1String("updateNewChat")) {
        const QJsonObject chat = update.value(QStringLiteral("chat")).toObject();
        ChatData data = chatFromJson(chat);
        if (data.id == 0)
            return;
        if (m_mediaStore) {
            data.photoReady = m_mediaStore->isReady(data.photoFileId);
        }
        const auto existing = m_chats.constFind(data.id);
        if (existing != m_chats.cend() && existing->photoFileId == data.photoFileId) {
            data.photoReady = data.photoReady || existing->photoReady;
            data.photoRevision = existing->photoRevision;
        }
        m_chats.insert(data.id, data);
        m_requestedChats.remove(data.id);
        rebuildChatList();
        emit stateChanged();
        return;
    }

    if (type == QLatin1String("updateChatTitle")) {
        const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
        auto it = m_chats.find(chatId);
        if (it != m_chats.end()) {
            it->title = update.value(QStringLiteral("title")).toString();
            it->initials = initialsForTitle(it->title);
            if (chatId == m_currentChatId) {
                m_currentChatTitle = it->title;
                emit currentChatChanged();
            }
            rebuildChatList();
        }
        return;
    }

    if (type == QLatin1String("updateChatPosition")) {
        const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
        auto it = m_chats.find(chatId);
        if (it == m_chats.end())
            return;
        applyChatPosition(*it, update.value(QStringLiteral("position")).toObject());
        rebuildChatList();
        emit stateChanged();
        return;
    }

    if (type == QLatin1String("updateChatLastMessage")) {
        const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
        auto it = m_chats.find(chatId);
        if (it == m_chats.end())
            return;
        const QJsonObject last = update.value(QStringLiteral("last_message")).toObject();
        if (last.isEmpty()) {
            it->preview.clear();
            it->lastMessageDate = 0;
            it->lastMessageId = 0;
        } else {
            it->preview = chatPreview(last);
            it->lastMessageDate = jsonInt64(last.value(QStringLiteral("date")));
            it->lastMessageId = jsonInt64(last.value(QStringLiteral("id")));
        }
        const QJsonArray positions = update.value(QStringLiteral("positions")).toArray();
        for (const QJsonValue& value : positions)
            applyChatPosition(*it, value.toObject());
        rebuildChatList();
        emit stateChanged();
        return;
    }

    if (type == QLatin1String("updateChatReadInbox")) {
        const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
        auto it = m_chats.find(chatId);
        if (it != m_chats.end()) {
            it->unreadCount = update.value(QStringLiteral("unread_count")).toInt();
            rebuildChatList();
        }
        return;
    }

    if (type == QLatin1String("updateChatNotificationSettings")) {
        const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
        auto it = m_chats.find(chatId);
        if (it == m_chats.end())
            return;
        it->muted = update.value(QStringLiteral("notification_settings"))
                        .toObject()
                        .value(QStringLiteral("mute_for"))
                        .toInt()
            > 0;
        rebuildChatList();
        return;
    }

    if (type == QLatin1String("updateChatAddedToList")
        || type == QLatin1String("updateChatRemovedFromList")) {
        const QString list = update.value(QStringLiteral("chat_list"))
                                 .toObject()
                                 .value(QStringLiteral("@type"))
                                 .toString();
        if (list != QLatin1String("chatListMain"))
            return;
        const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
        auto it = m_chats.find(chatId);
        if (it == m_chats.end())
            return;
        it->inMainList = type == QLatin1String("updateChatAddedToList");
        rebuildChatList();
        return;
    }

    if (type == QLatin1String("updateNewMessage")) {
        const QJsonObject object = update.value(QStringLiteral("message")).toObject();
        MessageData message = messageFromJson(object);
        trackMessageMedia(message);
        const auto chatIt = m_chats.constFind(message.chatId);
        const QString chatTitle = chatIt != m_chats.cend() ? chatIt->title : QString();
        const bool muted = chatIt != m_chats.cend() && chatIt->muted;
        const bool privateChat = chatIt != m_chats.cend()
            ? isPrivateChat(chatIt->type)
            : (message.chatId == m_currentChatId && isPrivateChat(m_currentChatType));
        const NotificationPayload payload = notificationForMessage(
            object, chatTitle, privateChat, m_currentChatId, muted);
        if (payload.shouldNotify)
            emit notificationRequested(payload.title, payload.body);
        if (message.chatId == m_currentChatId) {
            if (isPrivateChat(m_currentChatType) && !message.outgoing)
                message.senderName = m_currentChatTitle;
            resolveSender(message);
            m_messageModel->addOrUpdate(message);
            m_messageModel->resolveReplyPreviews();
            markCurrentChatRead();
            emit stateChanged();
        }
        updateChatFromMessage(message.chatId, object);
        return;
    }

    if (type == QLatin1String("updateMessageSendSucceeded")) {
        const QJsonObject object = update.value(QStringLiteral("message")).toObject();
        const qint64 oldId = jsonInt64(update.value(QStringLiteral("old_message_id")));
        if (jsonInt64(object.value(QStringLiteral("chat_id"))) == m_currentChatId) {
            MessageData message = messageFromJson(object);
            trackMessageMedia(message);
            resolveSender(message);
            m_messageModel->replaceId(oldId, message);
            m_messageModel->resolveReplyPreviews();
            markCurrentChatRead();
            emit stateChanged();
        }
        return;
    }

    if (type == QLatin1String("updateMessageSendFailed")) {
        const qint64 oldId = jsonInt64(update.value(QStringLiteral("old_message_id")));
        if (jsonInt64(update.value(QStringLiteral("message"))
                          .toObject()
                          .value(QStringLiteral("chat_id")))
            == m_currentChatId) {
            m_messageModel->markFailed(oldId);
            setError(QStringLiteral("A message could not be sent: %1")
                         .arg(update.value(QStringLiteral("error"))
                                  .toObject()
                                  .value(QStringLiteral("message"))
                                  .toString()));
            emit stateChanged();
        }
        return;
    }

    if (type == QLatin1String("updateMessageContent")) {
        if (jsonInt64(update.value(QStringLiteral("chat_id"))) != m_currentChatId)
            return;
        m_messageModel->updateContent(jsonInt64(update.value(QStringLiteral("message_id"))),
                                      update.value(QStringLiteral("new_content")).toObject(),
                                      false);
        return;
    }

    if (type == QLatin1String("updateMessageEdited")) {
        if (jsonInt64(update.value(QStringLiteral("chat_id"))) != m_currentChatId)
            return;
        m_messageModel->markEdited(jsonInt64(update.value(QStringLiteral("message_id"))));
        return;
    }

    if (type == QLatin1String("updateDeleteMessages")) {
        const qint64 chatId = jsonInt64(update.value(QStringLiteral("chat_id")));
        if (chatId != m_currentChatId)
            return;
        QVector<qint64> ids;
        for (const QJsonValue& value : update.value(QStringLiteral("message_ids")).toArray())
            ids.append(jsonInt64(value));
        m_messageModel->removeIds(ids);
        return;
    }

    if (type == QLatin1String("updateUser")) {
        const QJsonObject user = update.value(QStringLiteral("user")).toObject();
        const qint64 userId = jsonInt64(user.value(QStringLiteral("id")));
        const QString name = senderNameFromUser(user);
        if (userId != 0 && !name.isEmpty()) {
            m_userNames.insert(userId, name);
            m_messageModel->updateSenderName(userId, name);
        }
        return;
    }
}

void TelegramRepository::updateChatFromMessage(qint64 chatId, const QJsonObject& message)
{
    auto it = m_chats.find(chatId);
    if (it == m_chats.end())
        return;
    it->preview = chatPreview(message);
    it->lastMessageDate = jsonInt64(message.value(QStringLiteral("date")));
    it->lastMessageId = jsonInt64(message.value(QStringLiteral("id")));
    rebuildChatList();
}

void TelegramRepository::trackMessageMedia(MessageData& message)
{
    if (!m_mediaStore || message.mediaFileId == 0)
        return;
    m_mediaStore->trackFileId(message.mediaFileId, 16);
    message.mediaReady = m_mediaStore->isReady(message.mediaFileId);
}

void TelegramRepository::resolveSender(MessageData& message)
{
    if (message.outgoing || message.senderUserId == 0)
        return;
    const QString cached = m_userNames.value(message.senderUserId);
    if (!cached.isEmpty()) {
        message.senderName = cached;
        return;
    }
    if (isPrivateChat(m_currentChatType)) {
        message.senderName = m_currentChatTitle;
        return;
    }
    message.senderName = QStringLiteral("Member");
    requestUser(message.senderUserId);
}

void TelegramRepository::requestUser(qint64 userId)
{
    if (userId == 0 || m_requestedUsers.contains(userId) || m_userNames.contains(userId))
        return;
    m_requestedUsers.insert(userId);
    sendRequest(
        QJsonObject{{QStringLiteral("@type"), QStringLiteral("getUser")},
                    {QStringLiteral("user_id"), userId}},
        [this, userId](const QJsonObject& user) {
            m_requestedUsers.remove(userId);
            if (user.value(QStringLiteral("@type")).toString() != QLatin1String("user"))
                return;
            const QString name = senderNameFromUser(user);
            if (name.isEmpty())
                return;
            m_userNames.insert(userId, name);
            m_messageModel->updateSenderName(userId, name);
        },
        QStringLiteral("Could not read a user"));
}

void TelegramRepository::setError(const QString& message)
{
    m_error = logging::redact(message);
    qCWarning(lcRepo).noquote() << m_error;
    emit errorOccurred(m_error);
}

void TelegramRepository::sendRequest(const QJsonObject& request,
                                     std::function<void(const QJsonObject&)> handler,
                                     const QString& description, int timeoutMs)
{
    qCDebug(lcRepo) << "Sending" << request.value(QStringLiteral("@type")).toString();
    m_session->sendRequest(
        request,
        [this, handler = std::move(handler), description](const QJsonObject& response) {
            if (response.value(QStringLiteral("@type")).toString() == QLatin1String("error")) {
                const int code = response.value(QStringLiteral("code")).toInt();
                if (code == -2 || code == -3) {
                    setError(QStringLiteral("%1: %2").arg(
                        description, response.value(QStringLiteral("message")).toString()));
                }
            }
            if (handler)
                handler(response);
        },
        timeoutMs);
}

} // namespace beygram::telegram
