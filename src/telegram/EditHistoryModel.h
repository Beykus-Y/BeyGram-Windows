#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One stored edit of a message: the text as it looked after that edit, and the
/// moment TDLib reported for it.
///
/// `text` is kept exactly as given, so surrounding whitespace, newlines and an
/// empty string are meaningful versions. `editedAt` is always positive in a
/// stored version; record() refuses anything else because this model has no
/// clock and will not invent a timestamp.
struct EditVersion {
    QString text;
    qint64 editedAt = 0;
};

/// The edit history of messages, oldest version first, keyed by message id.
///
/// Telegram reports an edit as the message's new text, not as a diff, so the
/// only way to offer "edited from …" is to remember what the text used to be.
/// record() appends the new text to that message's list, originalText() and
/// latestText() read the two ends, and versions() hands out the whole list.
///
/// Each message's list is capped by the `maxVersions` passed to record(), which
/// is clamped to at least one; once the list is longer, the oldest versions are
/// dropped first. A record() whose text equals the newest stored text is the
/// same edit echoed again and is ignored, even when its timestamp differs.
///
/// The model keeps no clock. An edit with a non-positive `editedAt` cannot be
/// placed in time and is ignored rather than stamped with a made-up value, and
/// a message id that is not positive cannot name a message, so it is ignored
/// the same way.
///
/// changed() is emitted only when a stored history really moved: on an appended
/// version, on a message removed by clearMessage(), and on clear() when
/// anything was stored. Ignored calls, duplicate texts and clearing an absent
/// message or an empty model stay silent.
///
/// Nothing here is persisted and the class is not thread-safe; the owner keeps
/// it on the thread that renders the message.
class EditHistoryModel final : public QObject {
    Q_OBJECT

public:
    explicit EditHistoryModel(QObject* parent = nullptr);

    /// Appends `text` as the newest version of `messageId`.
    ///
    /// The text is stored verbatim and may be empty. The call is ignored when
    /// `messageId` is not positive, when `editedAt` is not positive, or when
    /// `text` equals the newest stored text. `maxVersions` is clamped to at
    /// least one, and the oldest versions are removed once the list is longer
    /// than that.
    void record(qint64 messageId, const QString& text, qint64 editedAt, int maxVersions = 20);

    /// The stored versions of `messageId`, oldest first. Empty when the message
    /// has no history or the id is not positive.
    [[nodiscard]] QVector<EditVersion> versions(qint64 messageId) const;

    /// The text of the first stored version, or an empty string when there is
    /// none. An empty string can also be a stored version, so use
    /// versionCount() to tell the two apart.
    [[nodiscard]] QString originalText(qint64 messageId) const;

    /// The text of the newest stored version, or an empty string when there is
    /// none.
    [[nodiscard]] QString latestText(qint64 messageId) const;

    [[nodiscard]] int versionCount(qint64 messageId) const;

    /// Drops the whole history of one message. Silent when it has none.
    void clearMessage(qint64 messageId);

    /// Drops every message's history. Silent when the model is already empty.
    void clear();

signals:
    /// Emitted whenever a stored history really changed.
    void changed();

private:
    QHash<qint64, QVector<EditVersion>> m_history;
};

} // namespace beygram::telegram
