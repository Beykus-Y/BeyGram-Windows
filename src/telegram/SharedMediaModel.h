#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

// Attachments of a chat profile, split into the tabs the shared-media section
// shows. A page comes from TDLib's `searchChatMessages` with the tab's
// `searchMessagesFilter*`, so the loaded list always belongs to one tab.

namespace beygram::telegram {

/// Tabs of the shared-media section, in the order the tab bar shows them.
enum class SharedMediaTab { Photos, Videos, Files, Links, Voice, Music, Gifs };

/// One attachment row of a shared-media tab.
///
/// `messageId` identifies the message that carries the attachment and is the
/// only identity used when pages overlap. `date` is the message timestamp in
/// Unix seconds and orders the list. `title` and `subtitle` are display labels
/// prepared by the caller. `fileId` is the TDLib file of the attachment (0 when
/// the row has none, as for a link), `sizeBytes` its size and `durationSeconds`
/// the duration for videos, voice notes and music.
struct SharedMediaItem {
    qint64 messageId = 0;
    qint64 date = 0;
    QString title;
    QString subtitle;
    int fileId = 0;
    qint64 sizeBytes = 0;
    int durationSeconds = 0;
};

/// TDLib `searchMessagesFilter*` object for \a tab as compact JSON, for example
/// `{"@type":"searchMessagesFilterPhoto"}`. This is the value a
/// `searchChatMessages` or `getChatMessageCount` request puts into `filter`.
[[nodiscard]] QString sharedMediaFilterJson(SharedMediaTab tab);

/// The attachments of one shared-media tab, newest first.
///
/// A message id identifies an item: setItems() keeps the first copy of a
/// repeated id, and appendItems() adds only ids that are not loaded yet, so
/// overlapping pages merge without duplicates. Both calls normalize the list to
/// newest-first order by date and then by descending message id, which makes
/// the order deterministic regardless of the order TDLib returned them in.
///
/// Changing the tab drops the loaded items because each tab has its own filter;
/// setting the current tab again is a no-op. nextFromMessageId() is the oldest
/// loaded id and therefore the cursor for the next older page, or 0 when
/// nothing is loaded. clear() drops the items but keeps the tab.
///
/// Every mutation that changes the stored state emits changed(); a call that
/// leaves the tab and the items as they were stays silent.
class SharedMediaModel final : public QObject {
    Q_OBJECT

public:
    explicit SharedMediaModel(QObject* parent = nullptr);

    /// Switches the tab. A different tab starts empty; the current tab is kept.
    void setTab(SharedMediaTab tab);
    [[nodiscard]] SharedMediaTab tab() const;

    /// Replaces the list with a de-duplicated copy of \a items in newest-first
    /// order.
    void setItems(QVector<SharedMediaItem> items);
    /// Adds the items whose message id is not loaded yet and re-sorts the list.
    /// The items already stored keep their payload.
    void appendItems(QVector<SharedMediaItem> items);
    [[nodiscard]] QVector<SharedMediaItem> items() const;
    [[nodiscard]] int count() const;
    /// Message id of the oldest loaded item, or 0 when the list is empty.
    [[nodiscard]] qint64 nextFromMessageId() const;

    /// Drops every loaded item; the tab stays.
    void clear();

signals:
    void changed();

private:
    SharedMediaTab m_tab = SharedMediaTab::Photos;
    QVector<SharedMediaItem> m_items;
    /// Whether a next older page can be requested; true while items are loaded.
    bool m_hasMore = false;
};

} // namespace beygram::telegram
