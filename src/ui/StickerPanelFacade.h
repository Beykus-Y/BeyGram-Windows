#pragma once

#include "telegram/RecentStickerModel.h"
#include "telegram/StickerPickerController.h"
#include "telegram/StickerSearchController.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// QML-facing state for the sticker panel.
///
/// The facade is a thin shell over three existing pieces of the sticker stack:
/// StickerPickerController owns the set strip and the current set's stickers,
/// StickerSearchController owns the search query, and RecentStickerModel owns
/// the most-recently-sent order. Nothing here talks to TDLib, downloads a file
/// or logs: the owner hands titles and sticker ids in, QML renders them, and a
/// tapped sticker comes back out through stickerActivated().
///
/// Every value QML can read is also a property with the single changed()
/// notify signal, so a binding never has to know which controller moved. A
/// mutation emits changed() only when the exposed state really differs, and
/// only once per call, which keeps a repaint bound to changed() cheap.
class StickerPanelFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open WRITE setOpen NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QStringList recentStickers READ recentStickers NOTIFY changed)
    Q_PROPERTY(QStringList setTitles READ setTitles NOTIFY changed)
    Q_PROPERTY(int currentSetIndex READ currentSetIndex WRITE setCurrentSetIndex NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    /// Sticker ids of the selected set, as strings. Extra to the minimum the
    /// panel needs, because the grid cannot be built without them.
    Q_PROPERTY(QStringList stickers READ stickers NOTIFY changed)

public:
    explicit StickerPanelFacade(QObject* parent = nullptr);

    /// Whether the panel is showing. Purely presentation state; closing does
    /// not drop the query or the loaded sets.
    [[nodiscard]] bool open() const;
    void setOpen(bool open);

    /// The trimmed search text held by StickerSearchController. Empty means
    /// "no search", and a new query drops the previous results and its loading
    /// flag, exactly like the controller documents.
    [[nodiscard]] QString query() const;
    void setQuery(const QString& query);

    /// Recently sent sticker ids, newest first, as strings.
    [[nodiscard]] QStringList recentStickers() const;

    /// Titles of the set strip, in the order they were handed in. Blank titles
    /// are dropped, so the list can be shorter than the input.
    [[nodiscard]] QStringList setTitles() const;

    /// Index into setTitles(), or -1 while no set is known. Only indices that
    /// address an existing title are accepted; anything else is ignored.
    [[nodiscard]] int currentSetIndex() const;
    void setCurrentSetIndex(int index);

    /// Mirrors StickerSearchController::loading(): true while the owner has a
    /// search request in flight.
    [[nodiscard]] bool loading() const;

    /// Sticker ids of the selected set, as strings, in the order setSetStickers
    /// received them.
    [[nodiscard]] QStringList stickers() const;

    /// Called by the panel when a sticker is tapped: the sticker becomes the
    /// newest recent entry and stickerActivated() asks the owner to send it. A
    /// non-positive or unparsable id is ignored and emits nothing.
    Q_INVOKABLE void recordSent(const QString& stickerId);

    /// Replaces the set strip. Only titles are given; ids are assigned by
    /// position (1..N) so QML never has to invent one. A blank title is
    /// skipped and does not consume an id. The selection is clamped into the
    /// new list, so a shrinking strip cannot leave the panel on a dead tab.
    Q_INVOKABLE void setSets(const QStringList& titles);

    /// Replaces the selected set's stickers. Ids are parsed as 64-bit values;
    /// anything unparsable or non-positive is dropped by the picker controller,
    /// which also keeps a repeated id only once.
    Q_INVOKABLE void setSetStickers(const QStringList& stickerIds);

    /// Drops the open flag, the query, the sets, the stickers, the selection
    /// and the recents.
    Q_INVOKABLE void clear();

signals:
    void changed();
    void stickerActivated(const QString& stickerId);

private:
    /// Everything a QML binding can read. Mutators snapshot it, run, and emit
    /// changed() only when the snapshot moved, so a no-op call stays silent.
    struct State {
        bool open = false;
        QString query;
        QStringList recents;
        QStringList titles;
        int index = -1;
        bool loading = false;
        QStringList stickers;

        bool operator==(const State&) const = default;
    };

    [[nodiscard]] State state() const;
    void emitIfChanged(const State& before);

    telegram::StickerPickerController m_picker;
    telegram::StickerSearchController m_search;
    telegram::RecentStickerModel m_recents;
    QStringList m_setTitles;
    int m_currentSetIndex = -1;
    bool m_open = false;
};

} // namespace beygram::ui
