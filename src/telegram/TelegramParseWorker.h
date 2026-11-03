#pragma once

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QObject>
#include <QVector>

namespace beygram::telegram {

/// Pure JSON-to-domain parsing that runs on a dedicated worker thread.
///
/// TDLib already receives on its own thread, but formatting text entities,
/// building messages and parsing chat objects is CPU work that used to happen
/// on the GUI thread. Results come back through queued signals; the repository
/// remains the only owner of TDLib state and of the Qt models.
class TelegramParseWorker final : public QObject {
    Q_OBJECT

public:
    explicit TelegramParseWorker(QObject* parent = nullptr);

public slots:
    void parseChat(quint64 jobId, qint64 chatId, const QJsonObject& chat);
    void parseHistory(quint64 jobId, qint64 chatId, bool older, const QJsonArray& messages);

signals:
    void chatParsed(quint64 jobId, qint64 chatId,
                    const beygram::telegram::ChatData& chat);
    void historyParsed(quint64 jobId, qint64 chatId, bool older,
                       const QVector<beygram::telegram::MessageData>& messages);
};

} // namespace beygram::telegram
