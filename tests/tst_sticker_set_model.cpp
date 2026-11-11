#include "telegram/StickerSetModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class StickerSetModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesStickerRoles();
    void countsRows();
    void resetsOncePerSet();
    void clearsStickers();
    void ignoresInvalidIndex();
    void mapsRoleNames();
};

namespace {

StickerInfo makeSticker(qint64 stickerId, const QString& emoji, int thumbnailFileId, int fullFileId,
                        bool isAnimated, bool isVideo)
{
    StickerInfo sticker;
    sticker.stickerId = stickerId;
    sticker.setId = 500;
    sticker.emoji = emoji;
    sticker.width = 512;
    sticker.height = 512;
    sticker.thumbnailFileId = thumbnailFileId;
    sticker.fullFileId = fullFileId;
    sticker.isAnimated = isAnimated;
    sticker.isVideo = isVideo;
    return sticker;
}

} // namespace

void StickerSetModelTest::exposesStickerRoles()
{
    StickerSetModel model;
    model.setStickers({
        makeSticker(100, QStringLiteral("🙂"), 5, 9, false, false),
        makeSticker(101, QStringLiteral("🎉"), 11, 12, true, false),
        makeSticker(102, QStringLiteral("🕺"), 14, 15, false, true),
    });

    const QModelIndex animated = model.index(1);
    QCOMPARE(animated.data(StickerSetModel::StickerIdRole).toLongLong(), qint64(101));
    QCOMPARE(animated.data(StickerSetModel::EmojiRole).toString(), QStringLiteral("🎉"));
    QCOMPARE(animated.data(StickerSetModel::ThumbnailFileIdRole).toInt(), 11);
    QCOMPARE(animated.data(StickerSetModel::FullFileIdRole).toInt(), 12);
    QVERIFY(animated.data(StickerSetModel::AnimatedRole).toBool());
    QVERIFY(!animated.data(StickerSetModel::VideoRole).toBool());

    const QModelIndex video = model.index(2);
    QVERIFY(!video.data(StickerSetModel::AnimatedRole).toBool());
    QVERIFY(video.data(StickerSetModel::VideoRole).toBool());
    QCOMPARE(video.data(StickerSetModel::FullFileIdRole).toInt(), 15);

    const QModelIndex plain = model.index(0);
    QCOMPARE(plain.data(StickerSetModel::StickerIdRole).toLongLong(), qint64(100));
    QVERIFY(!plain.data(StickerSetModel::AnimatedRole).toBool());
    QVERIFY(!plain.data(StickerSetModel::VideoRole).toBool());

    // An unknown role must not leak sticker data.
    QVERIFY(!plain.data(Qt::DisplayRole).isValid());
}

void StickerSetModelTest::countsRows()
{
    StickerSetModel model;
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.rowCount(), 0);

    QSignalSpy countSpy(&model, &StickerSetModel::countChanged);
    model.setStickers({makeSticker(1, QStringLiteral("a"), 2, 3, false, false),
                       makeSticker(2, QStringLiteral("b"), 4, 5, true, false)});
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(countSpy.count(), 1);

    // A valid parent index has no children in a flat list.
    QCOMPARE(model.rowCount(model.index(0)), 0);
}

void StickerSetModelTest::resetsOncePerSet()
{
    StickerSetModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &StickerSetModel::countChanged);

    model.setStickers({makeSticker(1, QStringLiteral("a"), 2, 3, false, false)});
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    model.setStickers({makeSticker(4, QStringLiteral("b"), 6, 7, false, true),
                       makeSticker(8, QStringLiteral("c"), 9, 10, false, false)});
    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(countSpy.count(), 2);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.index(0).data(StickerSetModel::StickerIdRole).toLongLong(), qint64(4));

    // Replacing a set with an empty one still reports the new row count.
    model.setStickers({});
    QCOMPARE(resetSpy.count(), 3);
    QCOMPARE(countSpy.count(), 3);
    QCOMPARE(model.count(), 0);
}

void StickerSetModelTest::clearsStickers()
{
    StickerSetModel model;
    model.setStickers({makeSticker(1, QStringLiteral("a"), 2, 3, false, false),
                       makeSticker(2, QStringLiteral("b"), 4, 5, true, false)});
    QCOMPARE(model.count(), 2);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &StickerSetModel::countChanged);

    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    // Clearing an already empty model is a no-op.
    model.clear();
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(model.count(), 0);
}

void StickerSetModelTest::ignoresInvalidIndex()
{
    StickerSetModel model;
    model.setStickers({makeSticker(1, QStringLiteral("a"), 2, 3, true, false)});

    const QModelIndex invalid;
    QVERIFY(!invalid.isValid());
    for (int role = StickerSetModel::StickerIdRole; role <= StickerSetModel::VideoRole; ++role)
        QVERIFY(!model.data(invalid, role).isValid());

    // Out-of-range rows and an empty model both yield no data.
    QVERIFY(!model.data(model.index(-1), StickerSetModel::StickerIdRole).isValid());
    QVERIFY(!model.data(model.index(1), StickerSetModel::StickerIdRole).isValid());
    QVERIFY(!model.data(model.index(99), StickerSetModel::EmojiRole).isValid());

    StickerSetModel empty;
    QVERIFY(!empty.data(empty.index(0), StickerSetModel::StickerIdRole).isValid());
}

void StickerSetModelTest::mapsRoleNames()
{
    StickerSetModel model;
    const QHash<int, QByteArray> names = model.roleNames();
    QCOMPARE(names.size(), qsizetype(6));
    QCOMPARE(names.value(StickerSetModel::StickerIdRole), QByteArray("stickerId"));
    QCOMPARE(names.value(StickerSetModel::EmojiRole), QByteArray("emoji"));
    QCOMPARE(names.value(StickerSetModel::ThumbnailFileIdRole), QByteArray("thumbnailFileId"));
    QCOMPARE(names.value(StickerSetModel::FullFileIdRole), QByteArray("fullFileId"));
    QCOMPARE(names.value(StickerSetModel::AnimatedRole), QByteArray("isAnimated"));
    QCOMPARE(names.value(StickerSetModel::VideoRole), QByteArray("isVideo"));

    // QML resolves a role name to the same value the enum role returns.
    model.setStickers({makeSticker(7, QStringLiteral("🔥"), 8, 9, false, true)});
    const QModelIndex index = model.index(0);
    const QHash<int, QByteArray> roles = model.roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        QVERIFY(index.data(it.key()).isValid());
}

QTEST_GUILESS_MAIN(StickerSetModelTest)
#include "tst_sticker_set_model.moc"
