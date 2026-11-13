#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace beygram::ui {

/// The chat quick switcher (a command palette) as one QML-facing object.
///
/// The facade owns the chat catalog, the filter, the ranking, the selected row
/// and whether the palette is open. QML only renders titles() and the selection
/// and forwards keys; it never searches, sorts or navigates on its own.
///
/// Catalog:
///  - setChats() reads ids, titles and previews positionally. A missing title
///    or preview reads as empty.
///  - Ids are opaque strings, trimmed on the way in. An empty id is dropped and
///    only the first row of a repeated id is kept, so a chat appears once.
///  - Titles are shown verbatim. Previews are never shown; they are normalized
///    (trimmed and lowercased) and used only for matching.
///
/// Filtering:
///  - query() is stored as typed. Matching trims and lowercases it, so case and
///    surrounding whitespace do not matter.
///  - An empty or whitespace-only query shows the whole catalog in caller order.
///  - Otherwise a chat matches when the query occurs in its normalized title or
///    preview. Ranking mirrors telegram::ChatSearchIndex:
///       title prefix 4, title substring 3, preview prefix 2, preview substring 1.
///    Higher scores come first; equal scores keep the caller's order.
///
/// Selection:
///  - selectedIndex() indexes ids() and titles(), which are always parallel.
///    It is -1 exactly when they are empty, otherwise it is a valid index.
///  - A query change selects the first result. show() starts a fresh session:
///    it opens the palette, clears the query and selects the first row.
///  - moveSelection() wraps at both ends. A move that lands on the current row
///    (a delta of zero or a multiple of the row count) changes nothing.
///  - setChats() keeps the selected chat selected when it is still visible,
///    follows it to its new index when the catalog was reordered, and falls
///    back to the first row when it disappeared or no longer matches.
///
/// Activation:
///  - activateCurrent() emits chatActivated() with the selected id and closes
///    the palette if it was open, so a slot on chatActivated() already sees
///    open() false. It does nothing when there is no selected row.
///  - The facade performs no navigation itself: the caller opens the chat.
///
/// changed() is emitted only when the stored catalog, the query, the open state
/// or the selection really changes. Re-sending an equivalent catalog, setting
/// the same query, hiding a closed palette, showing an already open one and a
/// move that lands on the same row are all silent. Two catalogs are equivalent
/// when their ids and titles match and their previews normalize identically,
/// because previews are only ever matched.
///
/// The facade never touches TDLib, storage, settings or logging. It is not
/// thread-safe; its owner keeps it on the thread that draws the chat list.
class QuickSwitcherFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QStringList titles READ titles NOTIFY changed)
    Q_PROPERTY(QStringList ids READ ids NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)

public:
    explicit QuickSwitcherFacade(QObject* parent = nullptr);

    /// Whether the palette is open.
    [[nodiscard]] bool open() const;

    /// The search text as the user typed it, not normalized.
    [[nodiscard]] QString query() const;

    /// Titles of the visible rows, best match first, parallel to ids().
    [[nodiscard]] QStringList titles() const;

    /// Ids of the visible rows, best match first, parallel to titles().
    [[nodiscard]] QStringList ids() const;

    /// Index into ids() and titles() of the highlighted row, or -1 when the
    /// visible list is empty.
    [[nodiscard]] int selectedIndex() const;

    /// Replaces the catalog from the parallel string lists a chat-list payload
    /// arrives as. Ids are opaque keys; titles and previews are read
    /// positionally and may be shorter than ids.
    ///
    /// Ids are trimmed and an empty one is dropped; only the first row of a
    /// repeated id is kept. The caller's order is otherwise preserved and is
    /// the tie-breaker for equally ranked matches. The current query is left
    /// alone, so a reload does not clear the search box.
    Q_INVOKABLE void setChats(const QStringList& ids, const QStringList& titles,
                              const QStringList& previews);

    /// Opens the palette and starts a fresh session: the query is cleared and
    /// the first visible row is selected. Showing an already open palette is a
    /// no-op, so it never discards text the user is typing.
    Q_INVOKABLE void show();

    /// Closes the palette. The query and selection are left as they were; the
    /// next show() clears the query and starts from the first row. Hiding a
    /// closed palette is a no-op.
    Q_INVOKABLE void hide();

    /// Replaces the search text as typed. The visible rows and the selection
    /// are recomputed; the first result becomes selected, or -1 when nothing
    /// matches. Setting the same text again is a no-op.
    Q_INVOKABLE void setQuery(const QString& query);

    /// Moves the selection by `delta` visible rows, wrapping at both ends.
    /// Negative values move up. With no visible rows, or when the move lands
    /// on the current row, nothing changes and changed() is not emitted.
    Q_INVOKABLE void moveSelection(int delta);

    /// Emits chatActivated() with the selected id and closes the palette when
    /// it is open. Does nothing when there is no selected row.
    Q_INVOKABLE void activateCurrent();

signals:
    /// Emitted whenever open, query, the visible rows or the selection changed.
    void changed();

    /// Emitted by activateCurrent() with the chat the caller should open.
    void chatActivated(const QString& chatId);

private:
    /// One catalog row. titleKey and previewKey are the normalized strings the
    /// filter matches against; the raw preview is not kept because it is never
    /// displayed or compared.
    struct Entry {
        QString id;
        QString title;
        QString titleKey;
        QString previewKey;
    };

    /// trim + lowercase, the normalization used for both matching and catalog
    /// equality of previews.
    [[nodiscard]] static QString normalized(const QString& text);

    /// Ranking score of one entry for a non-empty normalized query, or -1 when
    /// the query occurs in neither field.
    [[nodiscard]] static int scoreFor(const Entry& entry, const QString& key);

    /// Recomputes m_matches from m_chats and m_query, preserving catalog order
    /// within equal scores. Does not touch the selection or emit anything.
    void rebuildMatches();

    /// Whether `chats` is observably the same catalog as the stored one.
    [[nodiscard]] bool sameChats(const QVector<Entry>& chats) const;

    /// The selected chat's id, or an empty string when no row is selected.
    [[nodiscard]] QString selectedChatId() const;

    /// Selects the visible row holding `chatId`, or the first visible row when
    /// that chat is absent or `chatId` is empty. Sets -1 when nothing is
    /// visible.
    void selectChatOrFirst(const QString& chatId);

    QVector<Entry> m_chats;
    QVector<int> m_matches;
    QString m_query;
    int m_selectedIndex = -1;
    bool m_open = false;
};

} // namespace beygram::ui
