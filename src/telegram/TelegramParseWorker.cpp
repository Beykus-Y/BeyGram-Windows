#include "TelegramParseWorker.h"

#include <QMetaType>

#include <algorithm>

namespace beygram::telegram {

TelegramParseWorker::TelegramParseWorker(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<ChatData>("beygram::telegram::ChatData");
    qRegisterMetaType<MessageData>("beygram::telegram::MessageData");
    qRegisterMetaType<QVector<MessageData>>("QVector<beygram::telegram::MessageData>");
}

void TelegramParseWorker::parseChat(quint64 jobId, qint64 chatId,
                                    const QJsonObject& chat)
{
    const ChatData parsed = chatFromJson(chat);
    emit chatParsed(jobId, chatId, parsed);
}

void TelegramParseWorker::parseHistory(quint64 jobId, qint64 chatId, bool older,
                                       const QJsonArray& messages)
{
    QVector<MessageData> parsed;
    parsed.reserve(messages.size());
    for (const QJsonValue& value : messages) {
        const QJsonObject object = value.toObject();
        if (object.isEmpty())
            continue;
        parsed.append(messageFromJson(object));
    }

    // TDLib returns newest-first; the model stores oldest-first.
    std::reverse(parsed.begin(), parsed.end());
    emit historyParsed(jobId, chatId, older, parsed);
}

} // namespace beygram::telegram
