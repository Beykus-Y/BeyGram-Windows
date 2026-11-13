#include "DraftStore.h"

#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QSaveFile>

namespace beygram::telegram {
namespace {
Q_LOGGING_CATEGORY(lcDrafts, "beygram.telegram.drafts")

constexpr int StoreVersion = 1;
constexpr int MaxDrafts = 500;

/// Drafts are ordered newest first; equal timestamps fall back to the chat id
/// so the result stays stable between runs.
bool newerFirst(const Draft& left, const Draft& right)
{
    if (left.updatedAt != right.updatedAt)
        return left.updatedAt > right.updatedAt;
    return left.chatId > right.chatId;
}

QVector<Draft> sortedDrafts(const QHash<qint64, Draft>& drafts)
{
    QVector<Draft> ordered;
    ordered.reserve(drafts.size());
    for (const Draft& draft : drafts)
        ordered.append(draft);
    std::sort(ordered.begin(), ordered.end(), newerFirst);
    return ordered;
}

/// Drops the least recently edited drafts until the store fits the limit.
void trimDrafts(QHash<qint64, Draft>& drafts, int limit = MaxDrafts)
{
    if (drafts.size() <= limit)
        return;

    const QVector<Draft> ordered = sortedDrafts(drafts);
    const int excess = static_cast<int>(ordered.size()) - limit;
    for (int i = 0; i < excess; ++i)
        drafts.remove(ordered.at(ordered.size() - 1 - i).chatId);
}

/// Accepts a string id (preferred, JSON-safe) or a plain number.
qint64 readInt64(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().toLongLong(&ok);
        return ok ? parsed : 0;
    }
    if (value.isDouble())
        return static_cast<qint64>(value.toDouble());
    return 0;
}

QJsonObject toJson(const Draft& draft)
{
    return QJsonObject{
        {QStringLiteral("chatId"), QString::number(draft.chatId)},
        {QStringLiteral("text"), draft.text},
        {QStringLiteral("replyToMessageId"), static_cast<double>(draft.replyToMessageId)},
        {QStringLiteral("updatedAt"), static_cast<double>(draft.updatedAt)},
    };
}

} // namespace

DraftStore::DraftStore(QString filePath, QObject* parent)
    : QObject(parent), m_filePath(std::move(filePath))
{
}

QString DraftStore::filePath() const
{
    return m_filePath;
}

int DraftStore::count() const
{
    return static_cast<int>(m_drafts.size());
}

Draft DraftStore::draft(qint64 chatId) const
{
    const auto it = m_drafts.constFind(chatId);
    if (it != m_drafts.cend())
        return *it;

    Draft empty;
    empty.chatId = chatId;
    return empty;
}

QVector<Draft> DraftStore::drafts() const
{
    return sortedDrafts(m_drafts);
}

void DraftStore::switchTo(const QString& filePath)
{
    if (m_filePath == filePath)
        return;
    m_filePath = filePath;
    m_drafts.clear();
    load();
}

void DraftStore::load()
{
    m_drafts.clear();
    if (m_filePath.isEmpty())
        return;

    QFile file(m_filePath);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDrafts).noquote() << "Draft file is unreadable:" << file.errorString();
        return;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        qCWarning(lcDrafts).noquote() << "Draft file is malformed:" << parseError.errorString();
        return;
    }

    const QJsonObject root = document.object();
    const int version = root.value(QStringLiteral("version")).toInt();
    if (version != StoreVersion) {
        qCWarning(lcDrafts) << "Unsupported draft file version" << version;
        return;
    }

    const QJsonArray entries = root.value(QStringLiteral("drafts")).toArray();
    for (const QJsonValue& value : entries) {
        if (!value.isObject())
            continue;

        const QJsonObject object = value.toObject();
        Draft draft;
        draft.chatId = readInt64(object, QStringLiteral("chatId"));
        if (draft.chatId == 0)
            continue;
        draft.text = object.value(QStringLiteral("text")).toString();
        if (draft.text.trimmed().isEmpty())
            continue;
        draft.replyToMessageId = readInt64(object, QStringLiteral("replyToMessageId"));
        draft.updatedAt = readInt64(object, QStringLiteral("updatedAt"));
        m_drafts.insert(draft.chatId, draft);
    }

    trimDrafts(m_drafts);
}

void DraftStore::setDraft(qint64 chatId, const QString& text, qint64 replyToMessageId)
{
    if (chatId == 0)
        return;

    if (text.trimmed().isEmpty()) {
        clearDraft(chatId);
        return;
    }

    const auto it = m_drafts.constFind(chatId);
    if (it != m_drafts.cend() && it->text == text && it->replyToMessageId == replyToMessageId)
        return;

    Draft draft;
    draft.chatId = chatId;
    draft.replyToMessageId = replyToMessageId;
    draft.text = text;
    draft.updatedAt = QDateTime::currentSecsSinceEpoch();

    // Make room first, so the draft that was just set can never be the one
    // evicted when several drafts share the same timestamp.
    trimDrafts(m_drafts, MaxDrafts - 1);
    m_drafts.insert(chatId, draft);

    persist();
    emit draftChanged(chatId);
}

void DraftStore::clearDraft(qint64 chatId)
{
    if (m_drafts.remove(chatId) == 0)
        return;

    persist();
    emit draftChanged(chatId);
}

void DraftStore::removeChat(qint64 chatId)
{
    clearDraft(chatId);
}

void DraftStore::persist()
{
    if (m_filePath.isEmpty())
        return;

    const QString directory = QFileInfo(m_filePath).absolutePath();
    if (!directory.isEmpty() && !QDir().mkpath(directory)) {
        qCWarning(lcDrafts).noquote() << "Could not create the draft directory";
        return;
    }

    QJsonArray entries;
    for (const Draft& draft : sortedDrafts(m_drafts))
        entries.append(toJson(draft));

    const QJsonObject root{
        {QStringLiteral("version"), StoreVersion},
        {QStringLiteral("drafts"), entries},
    };

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcDrafts).noquote() << "Could not open the draft file:" << file.errorString();
        return;
    }

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        qCWarning(lcDrafts).noquote() << "Could not write the draft file:" << file.errorString();
        return;
    }

    if (!file.commit())
        qCWarning(lcDrafts).noquote() << "Could not commit the draft file:" << file.errorString();
}

} // namespace beygram::telegram
