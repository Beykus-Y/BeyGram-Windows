#include "telegram/ProfilePhotoModel.h"

#include <QSignalSpy>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

ProfilePhotoRow row(qint64 id, int thumbnailFileId = 0, int fullFileId = 0)
{
    return ProfilePhotoRow{id, thumbnailFileId, fullFileId};
}

QVector<qint64> idsOf(const QVector<ProfilePhotoRow>& photos)
{
    QVector<qint64> ids;
    ids.reserve(photos.size());
    for (const ProfilePhotoRow& photo : photos)
        ids.append(photo.id);
    return ids;
}

} // namespace

class ProfilePhotoModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setPhotosDropsNonPositiveIds();
    void setPhotosDedupesKeepingFirst();
    void setPhotosKeepsIncomingOrder();
    void setPhotosReplacesPreviousPhotos();
    void setPhotosEmitsChangedOnce();
    void setPhotosIdenticalDoesNotEmit();
    void setPhotosFileIdChangeEmits();
    void setPhotosReorderedEmits();
    void setPhotosEmptyClearsPhotos();
    void photosReturnsIndependentCopy();

    void appendAddsOnlyNewIds();
    void appendToEmptyAddsPhotos();
    void appendKeepsExistingRowsUnchanged();
    void appendDedupesWithinBatch();
    void appendAllDuplicatesIsNoOp();
    void appendEmptyIsNoOp();

    void removeExistingPhoto();
    void removeAbsentPhotoIsNoOp();
    void removeNonPositiveIdIsNoOp();

    void clearDropsPhotos();
    void clearOnEmptyIsNoOp();

    void countMatchesPhotos();
};

void ProfilePhotoModelTest::startsEmpty()
{
    ProfilePhotoModel model;

    QCOMPARE(model.count(), 0);
    QVERIFY(model.photos().isEmpty());
}

void ProfilePhotoModelTest::setPhotosDropsNonPositiveIds()
{
    ProfilePhotoModel model;
    model.setPhotos({row(0, 10, 100), row(-4, 20, 200), row(7, 70, 700)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.photos().at(0).id, qint64(7));
    QCOMPARE(model.photos().at(0).thumbnailFileId, 70);
    QCOMPARE(model.photos().at(0).fullFileId, 700);
}

void ProfilePhotoModelTest::setPhotosDedupesKeepingFirst()
{
    ProfilePhotoModel model;
    model.setPhotos({row(5, 10, 100), row(5, 20, 200)});

    QCOMPARE(model.count(), 1);
    QCOMPARE(model.photos().at(0).id, qint64(5));
    QCOMPARE(model.photos().at(0).thumbnailFileId, 10);
    QCOMPARE(model.photos().at(0).fullFileId, 100);
}

void ProfilePhotoModelTest::setPhotosKeepsIncomingOrder()
{
    ProfilePhotoModel model;
    model.setPhotos({row(9), row(2), row(30)});

    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{9, 2, 30}));
}

void ProfilePhotoModelTest::setPhotosReplacesPreviousPhotos()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2)});
    model.setPhotos({row(3)});

    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{3}));
}

void ProfilePhotoModelTest::setPhotosEmitsChangedOnce()
{
    ProfilePhotoModel model;
    QSignalSpy spy(&model, &ProfilePhotoModel::changed);

    model.setPhotos({row(1), row(2)});

    QCOMPARE(spy.count(), 1);
}

void ProfilePhotoModelTest::setPhotosIdenticalDoesNotEmit()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1, 10, 100), row(2, 20, 200)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.setPhotos({row(1, 10, 100), row(2, 20, 200)});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 2);
}

void ProfilePhotoModelTest::setPhotosFileIdChangeEmits()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1, 10, 100)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.setPhotos({row(1, 11, 101)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.photos().at(0).thumbnailFileId, 11);
    QCOMPARE(model.photos().at(0).fullFileId, 101);
}

void ProfilePhotoModelTest::setPhotosReorderedEmits()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.setPhotos({row(2), row(1)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{2, 1}));
}

void ProfilePhotoModelTest::setPhotosEmptyClearsPhotos()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.setPhotos({});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
}

void ProfilePhotoModelTest::photosReturnsIndependentCopy()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1)});

    QVector<ProfilePhotoRow> copy = model.photos();
    copy[0].id = 99;

    QCOMPARE(model.photos().at(0).id, qint64(1));
}

void ProfilePhotoModelTest::appendAddsOnlyNewIds()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.append({row(2, 20, 200), row(3, 30, 300), row(4, 40, 400)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{1, 2, 3, 4}));
    QCOMPARE(model.photos().at(2).thumbnailFileId, 30);
    QCOMPARE(model.photos().at(2).fullFileId, 300);
}

void ProfilePhotoModelTest::appendToEmptyAddsPhotos()
{
    ProfilePhotoModel model;

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.append({row(1), row(2)});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{1, 2}));
}

void ProfilePhotoModelTest::appendKeepsExistingRowsUnchanged()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1, 10, 100)});

    model.append({row(1, 20, 200), row(2, 30, 300)});

    QCOMPARE(model.photos().at(0).thumbnailFileId, 10);
    QCOMPARE(model.photos().at(0).fullFileId, 100);
    QCOMPARE(model.photos().at(1).id, qint64(2));
    QCOMPARE(model.photos().at(1).thumbnailFileId, 30);
}

void ProfilePhotoModelTest::appendDedupesWithinBatch()
{
    ProfilePhotoModel model;
    model.append({row(7, 10, 100), row(7, 20, 200), row(8)});

    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{7, 8}));
    QCOMPARE(model.photos().at(0).thumbnailFileId, 10);
    QCOMPARE(model.photos().at(0).fullFileId, 100);
}

void ProfilePhotoModelTest::appendAllDuplicatesIsNoOp()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.append({row(1), row(2), row(0), row(-3)});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{1, 2}));
}

void ProfilePhotoModelTest::appendEmptyIsNoOp()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.append({});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void ProfilePhotoModelTest::removeExistingPhoto()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2), row(3)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    QVERIFY(model.remove(2));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(idsOf(model.photos()), (QVector<qint64>{1, 3}));
    QVERIFY(!model.remove(2));
    QCOMPARE(spy.count(), 1);
}

void ProfilePhotoModelTest::removeAbsentPhotoIsNoOp()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    QVERIFY(!model.remove(99));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void ProfilePhotoModelTest::removeNonPositiveIdIsNoOp()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    QVERIFY(!model.remove(0));
    QVERIFY(!model.remove(-5));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.count(), 1);
}

void ProfilePhotoModelTest::clearDropsPhotos()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2)});

    QSignalSpy spy(&model, &ProfilePhotoModel::changed);
    model.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.photos().isEmpty());
}

void ProfilePhotoModelTest::clearOnEmptyIsNoOp()
{
    ProfilePhotoModel model;
    QSignalSpy spy(&model, &ProfilePhotoModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 0);
}

void ProfilePhotoModelTest::countMatchesPhotos()
{
    ProfilePhotoModel model;
    model.setPhotos({row(1), row(2), row(3)});

    QCOMPARE(model.count(), 3);
    QCOMPARE(model.photos().size(), 3);

    model.remove(2);

    QCOMPARE(model.count(), 2);
    QCOMPARE(model.photos().size(), 2);
}

QTEST_GUILESS_MAIN(ProfilePhotoModelTest)
#include "tst_profile_photo_model.moc"
