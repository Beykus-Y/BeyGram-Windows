#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace beygram::telegram {

/// One entry of the emoji picker catalog.
///
/// emoji is the character(s) that get inserted into the composer, name is the
/// searchable label and category the picker section it belongs to. All three
/// are already-parsed values; nothing here touches TDLib.
struct EmojiEntry {
    QString emoji;
    QString name;
    QString category;
};

/// Holds the emoji picker catalog and the recently used emoji.
///
/// The model is a plain value holder: it stores already-parsed EmojiEntry
/// values and answers category and search lookups without touching TDLib or
/// the UI. Entries without an emoji are dropped on assignment because they
/// cannot be inserted, and later entries repeating an already stored emoji are
/// dropped so every emoji appears once; the first entry wins and the incoming
/// order is otherwise kept.
///
/// A category is the section title the picker groups entries under. Empty
/// categories mean "uncategorized": they never appear in categories() and
/// byCategory() with an empty name matches nothing.
///
/// The recent list is independent of the entries: it stores raw emoji strings
/// newest-first, so an emoji stays in the list even after the catalog that
/// supplied it is replaced.
class EmojiCatalogModel final : public QObject {
    Q_OBJECT

public:
    explicit EmojiCatalogModel(QObject* parent = nullptr);

    /// Replaces the stored catalog, dropping entries without an emoji and
    /// later entries whose emoji is already stored. Emits changed() only when
    /// the resulting catalog differs from the stored one. The recent list is
    /// left untouched.
    void setEntries(QVector<EmojiEntry> entries);

    /// Returns the number of stored entries.
    [[nodiscard]] int count() const;

    /// Returns the distinct non-empty categories in first-seen order, spelled
    /// as the first entry that introduced them. Categories differing only by
    /// case are the same category because byCategory() ignores case.
    [[nodiscard]] QStringList categories() const;

    /// Returns the entries of one category in stored order, compared
    /// case-insensitively. An empty name matches nothing.
    [[nodiscard]] QVector<EmojiEntry> byCategory(const QString& category) const;

    /// Returns the entries whose emoji or name contains `query`, compared
    /// case-insensitively, in stored order. An empty query matches every
    /// entry. A limit of 0 yields nothing, a negative limit means "no limit"
    /// and a positive limit caps the result.
    [[nodiscard]] QVector<EmojiEntry> search(const QString& query, int limit = 50) const;

    /// Records a used emoji at the front of the recent list, removing an older
    /// occurrence so the list stays unique and newest-first, and keeping at
    /// most maxRecent items. An empty emoji or a non-positive maxRecent is
    /// ignored. Emits changed() only when the list actually changed.
    void addRecent(const QString& emoji, int maxRecent = 24);

    /// Returns a copy of the recent emoji, newest first.
    [[nodiscard]] QStringList recent() const;

    /// Drops the catalog and the recent list, emitting changed() when there
    /// was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<EmojiEntry> m_entries;
    QStringList m_recent;
};

} // namespace beygram::telegram
