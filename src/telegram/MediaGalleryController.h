#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// State of the full-screen media viewer: which messages can be viewed, which
/// one is on screen, and how that one is zoomed and panned.
///
/// open() takes the message ids of the gallery in display order plus the
/// message that was opened. Ids that cannot address a message (non-positive)
/// are dropped and a repeated id keeps its first copy, so the stored order is
/// always usable. An open() whose list is empty after that cleaning is
/// rejected and leaves the controller exactly as it was, so an empty gallery
/// never replaces a running one. When the requested current id is not in the
/// list, the gallery falls back to its first item.
///
/// next() and previous() step by one and stop at the edges: a gallery has a
/// first and a last image, so navigation never wraps. At an edge they report
/// false and keep the index.
///
/// Zoom and pan belong to the item on screen, so they return to their defaults
/// whenever the current item changes (open, next, previous), exactly like
/// close(). zoom() is clamped to 1.0..5.0, each pan axis to -1.0..1.0 where 0
/// means centred, and non-finite values are ignored.
///
/// close() only acts while active() and forgets the list and the view state.
///
/// changed() is emitted once per call that really moved something; rejected
/// calls, no-op setters and edge navigation stay silent.
///
/// The gallery stores message ids only. AlbumMediaItem was considered for the
/// stored items, but it carries the thumbnail and full-size file ids plus the
/// caption of an album strip, none of which the viewer needs, and open()
/// receives bare ids; keeping QVector<qint64> avoids copying that payload into
/// a second model.
class MediaGalleryController final : public QObject {
    Q_OBJECT

public:
    explicit MediaGalleryController(QObject* parent = nullptr);

    /// Opens a gallery over \a messageIds with \a currentMessageId on screen.
    /// Ignored while the cleaned list is empty. A current id that is not in the
    /// list selects the first item. Resets zoom and pan.
    void open(QVector<qint64> messageIds, qint64 currentMessageId);

    /// Leaves the gallery and restores zoom and pan. Silent while inactive.
    void close();

    /// True while a gallery with a current item is open.
    [[nodiscard]] bool active() const;

    /// The gallery order, as a copy.
    [[nodiscard]] QVector<qint64> messageIds() const;

    /// Index of the current item, or -1 while inactive.
    [[nodiscard]] int currentIndex() const;

    /// The current message id, or 0 while inactive.
    [[nodiscard]] qint64 currentMessageId() const;

    /// Moves to the following item. False at the end, where the index stays.
    bool next();

    /// Moves to the preceding item. False at the start, where the index stays.
    bool previous();

    /// Sets the zoom factor, clamped to 1.0..5.0.
    void setZoom(double zoom);
    [[nodiscard]] double zoom() const;

    /// Sets both pan axes, each clamped to -1.0..1.0.
    void setPan(double x, double y);
    [[nodiscard]] double panX() const;
    [[nodiscard]] double panY() const;

    /// Restores zoom 1.0 and pan 0/0.
    void resetZoom();

signals:
    void changed();

private:
    QVector<qint64> m_ids;
    int m_index = -1;
    double m_zoom = 1.0;
    double m_panX = 0.0;
    double m_panY = 0.0;
};

} // namespace beygram::telegram
