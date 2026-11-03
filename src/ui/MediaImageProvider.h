#pragma once

#include <QQuickImageProvider>

namespace beygram::telegram {
class MediaFileStore;
}

namespace beygram::ui {

/// Serves cached TDLib thumbnails and avatars to QML.
/// URL: image://beygrammedia/<fileId>?v=<revision>
class MediaImageProvider final : public QQuickImageProvider {
public:
    explicit MediaImageProvider(telegram::MediaFileStore* store);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    telegram::MediaFileStore* m_store = nullptr;
};

} // namespace beygram::ui
