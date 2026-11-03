#include "telegram/MediaFileStore.h"
#include "ui/MediaImageProvider.h"

#include <QImage>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace beygram;

class MediaImageTest final : public QObject {
    Q_OBJECT

private slots:
    void roundsAvatarCorners();
    void keepsThumbnailCorners();
};

void MediaImageTest::roundsAvatarCorners()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("avatar.png"));

    QImage source(64, 64, QImage::Format_ARGB32_Premultiplied);
    source.fill(QColor(200, 80, 60));
    QVERIFY(source.save(path));

    telegram::MediaFileStore store;
    store.trackFileObject(QJsonObject{
        {QStringLiteral("id"), 42},
        {QStringLiteral("local"),
         QJsonObject{{QStringLiteral("is_downloading_completed"), true},
                     {QStringLiteral("path"), path}}},
    });
    QVERIFY(store.isReady(42));

    ui::MediaImageProvider provider(&store);
    QSize size;
    const QImage avatar = provider.requestImage(QStringLiteral("42?v=0&round=1"),
                                                &size, QSize(64, 64));
    QVERIFY(!avatar.isNull());
    QCOMPARE(size, QSize(64, 64));
    QCOMPARE(qAlpha(avatar.pixel(0, 0)), 0);
    QCOMPARE(qAlpha(avatar.pixel(63, 63)), 0);
    QVERIFY(qAlpha(avatar.pixel(32, 32)) > 0);
}

void MediaImageTest::keepsThumbnailCorners()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("thumb.png"));

    QImage source(32, 32, QImage::Format_ARGB32_Premultiplied);
    source.fill(QColor(40, 90, 160));
    QVERIFY(source.save(path));

    telegram::MediaFileStore store;
    store.trackFileObject(QJsonObject{
        {QStringLiteral("id"), 7},
        {QStringLiteral("local"),
         QJsonObject{{QStringLiteral("is_downloading_completed"), true},
                     {QStringLiteral("path"), path}}},
    });

    ui::MediaImageProvider provider(&store);
    const QImage thumbnail =
        provider.requestImage(QStringLiteral("7?v=0"), nullptr, QSize(32, 32));
    QVERIFY(!thumbnail.isNull());
    QCOMPARE(qAlpha(thumbnail.pixel(0, 0)), 255);
}

QTEST_GUILESS_MAIN(MediaImageTest)
#include "tst_media_image.moc"
