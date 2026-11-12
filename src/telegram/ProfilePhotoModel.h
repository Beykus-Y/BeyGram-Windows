#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One photo in a profile's photo history.
///
/// id is TDLib's profile-photo id (the chatPhoto id, not a message id),
/// thumbnailFileId is the small file shown in the grid and fullFileId the large
/// file opened in the viewer. Both file ids may be zero when TDLib has not
/// supplied that rendition yet; only id decides whether the row is usable.
struct ProfilePhotoRow {
    qint64 id = 0;
    int thumbnailFileId = 0;
    int fullFileId = 0;

    bool operator==(const ProfilePhotoRow&) const = default;
};

/// Holds the profile photos currently shown, in display order.
///
/// The model is a plain value holder: it stores already-parsed rows and never
/// touches TDLib. Rows without a positive id are dropped because they cannot be
/// addressed for deletion or selection, and later rows repeating an id already
/// stored are dropped so every photo appears once; the first row wins and the
/// incoming order is otherwise preserved. append() only adds ids that are not
/// already present, so refreshing a page never reorders or rewrites existing
/// rows. changed() is emitted only when the stored rows really differ.
class ProfilePhotoModel final : public QObject {
    Q_OBJECT

public:
    explicit ProfilePhotoModel(QObject* parent = nullptr);

    /// Replaces the stored photos, dropping rows with a non-positive id and
    /// later rows whose id is already stored. Emits changed() only when the
    /// resulting list differs from the stored one.
    void setPhotos(QVector<ProfilePhotoRow> photos);

    /// Returns a copy of the stored photos in display order.
    [[nodiscard]] QVector<ProfilePhotoRow> photos() const;

    /// Returns the number of stored photos.
    [[nodiscard]] int count() const;

    /// Removes the photo with photoId. Returns true when a row was removed and
    /// changed() was emitted; an absent id (including a non-positive one) is a
    /// no-op that returns false.
    bool remove(qint64 photoId);

    /// Appends rows whose ids are not stored yet, preserving the existing order
    /// and the incoming order of the new rows. Rows with a non-positive id and
    /// rows repeating an id already present (including earlier rows in the same
    /// call) are ignored. Emits changed() only when at least one row was added.
    void append(QVector<ProfilePhotoRow> photos);

    /// Drops every stored photo, emitting changed() when there was anything to
    /// drop.
    void clear();

signals:
    void changed();

private:
    QVector<ProfilePhotoRow> m_photos;
};

} // namespace beygram::telegram
