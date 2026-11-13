#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// The link preview the cache last saw for one URL.
///
/// `fetchedAt` is the Unix second the entry was stored, stamped by put(), so a
/// caller-supplied value is only a placeholder. Recency is measured by it.
struct CachedLinkPreview {
    QString url;              // always trimmed and non-empty in a stored entry
    QString title;
    QString description;
    QString siteName;
    int imageFileId = 0;
    bool isVideo = false;
    qint64 fetchedAt = 0;
};

/// Bounded memory of link previews, keyed by URL.
///
/// Entries are identified by their trimmed URL: putting a URL that is already
/// here replaces its fields instead of adding a second row, and makes it the
/// most recent entry because its fetchedAt becomes the new `nowUnix`.
///
/// The store is bounded. capacity() is clamped to 8..500, and once a put() or a
/// smaller capacity would exceed it, the least recently fetched entries go
/// first. Equal fetchedAt values are evicted by ascending URL, so the outcome
/// never depends on the order previews happened to arrive in.
///
/// Reading is pure: get() and contains() never change recency, so a preview
/// that is looked at often but not re-fetched can still be evicted. Only put()
/// moves an entry to the most recent position.
///
/// changed() is emitted only when the stored set really differs: a URL that is
/// empty after trimming is ignored, re-putting an entry with the same fields
/// and the same `nowUnix` is a no-op, and so is removing an absent URL or
/// clearing an empty store. A newcomer that arrives at capacity and is older
/// than everything already stored is evicted immediately, which also leaves
/// the store unchanged.
///
/// Nothing here is persisted and the class is not thread-safe; the owner keeps
/// it on the thread that resolves previews.
class LinkPreviewCache final : public QObject {
    Q_OBJECT

public:
    explicit LinkPreviewCache(QObject* parent = nullptr);

    /// Sets how many previews are kept, clamped to 8..500.
    ///
    /// Lowering the capacity evicts the least recently fetched entries at once.
    /// Setting the value it already has does nothing and does not signal.
    void setCapacity(int capacity);

    [[nodiscard]] int capacity() const;

    /// Stores the preview for preview.url as it was fetched at `nowUnix`.
    ///
    /// The URL is trimmed; an empty one is ignored. The entry is stored with
    /// `fetchedAt = nowUnix` and every other field is kept as given. A URL
    /// already present is replaced and becomes the most recent entry. An older
    /// `nowUnix` is accepted: the entry simply becomes the oldest one and can
    /// be evicted right away.
    void put(const CachedLinkPreview& preview, qint64 nowUnix);

    /// The stored preview, or a default-constructed one (empty url) when the
    /// URL is not cached. The lookup URL is trimmed; reading does not touch
    /// recency. Use contains() to tell a miss apart from an empty preview.
    [[nodiscard]] CachedLinkPreview get(const QString& url) const;

    [[nodiscard]] bool contains(const QString& url) const;

    /// Drops one URL. Removing an absent or empty URL does nothing.
    void remove(const QString& url);

    /// Drops every preview. Clearing an empty store does nothing.
    void clear();

    [[nodiscard]] int count() const;

signals:
    void changed();

private:
    static constexpr int kMinimumCapacity = 8;
    static constexpr int kMaximumCapacity = 500;
    static constexpr int kDefaultCapacity = 100;

    /// Index of the trimmed `url` in m_entries, or -1 when empty or absent.
    [[nodiscard]] int indexOf(const QString& url) const;

    /// Removes the least recently fetched entries until count() fits capacity().
    void evictOverflow();

    QVector<CachedLinkPreview> m_entries;
    int m_capacity = kDefaultCapacity;
};

} // namespace beygram::telegram
