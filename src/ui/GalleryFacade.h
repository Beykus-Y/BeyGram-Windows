#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::telegram {
class AlbumModel;
class MediaGalleryController;
} // namespace beygram::telegram

namespace beygram::ui {

/// QML-facing facade over MediaGalleryController and AlbumModel.
///
/// QML addresses messages by string id, while the controllers speak qint64, so
/// openGallery() and setAlbums() parse the ids and leave every other rule to
/// the objects they own: a repeated id keeps its first copy, an id that cannot
/// address a message is dropped, an unknown current id falls back to the first
/// item, and an empty gallery never replaces the one that is already open.
/// Ids that are not numbers at all are dropped while parsing.
///
/// The gallery and the albums are separate concerns. setAlbums() only feeds the
/// grouping that albumMessageIds() answers from; it never reorders or replaces
/// the gallery, which keeps exactly the order it was opened with.
///
/// Signals: changed() is emitted once per call that really moved something and
/// carries every observable property, so bindings watch a single signal.
/// currentChanged() is emitted only when the item on screen changed, including
/// the move to no item when the gallery closes.
///
/// Invariants:
///  - open() is true exactly while a gallery with a current item is shown.
///  - count() is the number of gallery items and index() the position of the
///    current one, or 0 and -1 while closed.
///  - currentId() is empty while closed.
///  - zoom() is 1.0..5.0 and belongs to the current item, so navigation and
///    close() return it to 1.0.
class GalleryFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ open NOTIFY changed)
    Q_PROPERTY(QString currentId READ currentId NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(double zoom READ zoom NOTIFY changed)

public:
    explicit GalleryFacade(QObject* parent = nullptr);

    [[nodiscard]] bool open() const;
    [[nodiscard]] QString currentId() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] int count() const;
    [[nodiscard]] double zoom() const;

    /// Opens a gallery over \a messageIds with \a currentId on screen. Ignored
    /// while the cleaned list is empty, and a current id that is not part of
    /// the list selects the first item.
    Q_INVOKABLE void openGallery(const QStringList& messageIds, const QString& currentId);

    /// Leaves the gallery. Silent while it is already closed.
    Q_INVOKABLE void close();

    /// Moves to the following item; stops at the last one.
    Q_INVOKABLE void next();

    /// Moves to the preceding item; stops at the first one.
    Q_INVOKABLE void previous();

    /// Sets the zoom factor, clamped to 1.0..5.0; non-finite values are ignored.
    Q_INVOKABLE void setZoom(double zoom);

    /// Restores zoom 1.0 for the current item.
    Q_INVOKABLE void resetZoom();

    /// Replaces the album grouping. \a groupedIds and \a messageIds are
    /// parallel: groupedIds[i] is the grouped_id of messageIds[i], and 0 means
    /// the message is ungrouped. Lists of different length describe
    /// inconsistent data, so the call is ignored and the previous grouping
    /// stays. Two empty lists clear the grouping.
    Q_INVOKABLE void setAlbums(const QStringList& groupedIds, const QStringList& messageIds);

    /// Message ids of the album that contains \a messageId, in album order.
    /// Empty when the id is unknown or when setAlbums() supplied no grouping.
    Q_INVOKABLE QStringList albumMessageIds(const QString& messageId) const;

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted when the item on screen changed; \a messageId is empty once the
    /// gallery is closed.
    void currentChanged(const QString& messageId);

private:
    void handleGalleryChanged();
    void handleAlbumsChanged();

    telegram::MediaGalleryController* m_gallery = nullptr;
    telegram::AlbumModel* m_albums = nullptr;
    QString m_lastCurrentId;
};

} // namespace beygram::ui
