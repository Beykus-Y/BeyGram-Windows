#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One unsent message per chat. Mirrors the draft behaviour of the Android
/// client: the composer text and the reply target survive restarts.
struct Draft {
    qint64 chatId = 0;
    qint64 replyToMessageId = 0;
    QString text;
    qint64 updatedAt = 0;
};

/// Durable per-chat message drafts backed by one small JSON file.
///
/// At most 500 drafts are kept; the least recently edited one is dropped when
/// the limit is exceeded. A missing or corrupt file simply yields an empty
/// store. Message text never reaches the log - only counts and error strings.
class DraftStore final : public QObject {
    Q_OBJECT

public:
    explicit DraftStore(QString filePath, QObject* parent = nullptr);

    /// Reads the JSON file. Missing, unreadable or corrupt input is ignored.
    void load();

    [[nodiscard]] QString filePath() const;
    [[nodiscard]] Draft draft(qint64 chatId) const;
    /// All drafts, most recently edited first; ties broken by chat id.
    [[nodiscard]] QVector<Draft> drafts() const;
    [[nodiscard]] int count() const;

    /// Stores `text` for `chatId`. Trimmed-empty text clears the draft instead.
    void setDraft(qint64 chatId, const QString& text, qint64 replyToMessageId = 0);
    void clearDraft(qint64 chatId);
    /// Forgets a chat entirely, e.g. after it was deleted or left.
    void removeChat(qint64 chatId);

signals:
    void draftChanged(qint64 chatId);

private:
    void persist();
    QString m_filePath;
    QHash<qint64, Draft> m_drafts;
};

} // namespace beygram::telegram
