#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One message handed to the document writer. The writer never fetches or
/// downloads anything; mediaType is the already-resolved label (for example
/// "photo", "video" or "document").
struct ExportedMessage {
    qint64 id = 0;
    qint64 date = 0;
    QString sender;
    QString text;
    QString mediaType;
};

/// Builds the two self-contained chat-export documents. Both formats sort
/// messages by date and then id, use the title "Chat" when the supplied title
/// is blank, and produce deterministic output for identical input.
class ChatExportWriter final {
public:
    /// A complete HTML document with inline styling. Every user-supplied string
    /// is escaped, and timestamps are rendered in UTC.
    [[nodiscard]] static QString toHtml(const QString& chatTitle,
                                        const QVector<ExportedMessage>& messages);

    /// The machine-readable document. Each message carries the Unix date and
    /// its UTC ISO-8601 spelling in dateIso (empty for a non-positive date).
    [[nodiscard]] static QJsonObject toJson(const QString& chatTitle,
                                            const QVector<ExportedMessage>& messages);
};

} // namespace beygram::telegram
