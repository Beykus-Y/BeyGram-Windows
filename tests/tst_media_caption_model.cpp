#include "telegram/MediaCaptionModel.h"

#include <QChar>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// U+1F642, a surrogate pair in QString and therefore two UTF-16 units.
QString smiley()
{
    return QString::fromUtf8("\xF0\x9F\x99\x82");
}

} // namespace

class MediaCaptionModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactiveAndEmpty();
    void beginActivatesWithOriginal();
    void beginTrimsOriginal();
    void beginAllowsEmptyOriginal();
    void beginEmitsChanged();
    void beginSameCaptionWhileActiveIsNoOp();
    void beginDiscardsEditsAndRestoresOriginal();
    void beginReplacesPreviousSession();
    void setTextIsIgnoredWhileInactive();
    void setTextUpdatesTextAndEmits();
    void setTextSameTextDoesNotEmit();
    void setTextDoesNotTrim();
    void setTextCapsAtLimit();
    void setTextKeepsSurrogatePairThatFits();
    void setTextDoesNotSplitSurrogatePair();
    void lengthAndRemainingTrackText();
    void remainingGoesNegativeForLongOriginal();
    void dirtyTracksBaseline();
    void dirtyIsFalseWhenTextIsRestored();
    void canSaveRequiresEditWithinLimit();
    void canSaveIsFalseWhileInactive();
    void markSavedAcceptsTextAndKeepsActive();
    void markSavedWhenCleanIsNoOp();
    void markSavedIsIgnoredWhileInactive();
    void cancelDeactivatesAndClears();
    void cancelWhileInactiveIsNoOp();
    void cancelDiscardsEdits();
};

void MediaCaptionModelTest::startsInactiveAndEmpty()
{
    MediaCaptionModel model;

    QVERIFY(!model.active());
    QVERIFY(model.text().isEmpty());
    QVERIFY(!model.dirty());
    QCOMPARE(model.length(), 0);
    QCOMPARE(model.remaining(), 1024);
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::beginActivatesWithOriginal()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("holiday snap"));

    QVERIFY(model.active());
    QCOMPARE(model.text(), QStringLiteral("holiday snap"));
    QVERIFY(!model.dirty());
    QCOMPARE(model.length(), 12);
    QCOMPARE(model.remaining(), 1012);
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::beginTrimsOriginal()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("  spaced out \n"));

    QCOMPARE(model.text(), QStringLiteral("spaced out"));
    QCOMPARE(model.length(), 10);
}

void MediaCaptionModelTest::beginAllowsEmptyOriginal()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("   "));

    QVERIFY(model.active());
    QVERIFY(model.text().isEmpty());
    QVERIFY(!model.dirty());
    QCOMPARE(model.remaining(), 1024);
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::beginEmitsChanged()
{
    MediaCaptionModel model;
    QSignalSpy spy(&model, &MediaCaptionModel::changed);

    model.begin(QStringLiteral("first"));

    QCOMPARE(spy.count(), 1);
}

void MediaCaptionModelTest::beginSameCaptionWhileActiveIsNoOp()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("same"));

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.begin(QStringLiteral("  same  "));

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.active());
    QCOMPARE(model.text(), QStringLiteral("same"));
}

void MediaCaptionModelTest::beginDiscardsEditsAndRestoresOriginal()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("original"));
    model.setText(QStringLiteral("edited"));

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.begin(QStringLiteral("original"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.text(), QStringLiteral("original"));
    QVERIFY(!model.dirty());
}

void MediaCaptionModelTest::beginReplacesPreviousSession()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("first"));
    model.setText(QStringLiteral("edited"));

    model.begin(QStringLiteral("second"));

    QCOMPARE(model.text(), QStringLiteral("second"));
    QVERIFY(!model.dirty());
}

void MediaCaptionModelTest::setTextIsIgnoredWhileInactive()
{
    MediaCaptionModel model;
    QSignalSpy spy(&model, &MediaCaptionModel::changed);

    model.setText(QStringLiteral("ignored"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.text().isEmpty());
    QVERIFY(!model.active());
}

void MediaCaptionModelTest::setTextUpdatesTextAndEmits()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("old"));

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.setText(QStringLiteral("new caption"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.text(), QStringLiteral("new caption"));
    QCOMPARE(model.length(), 11);
    QVERIFY(model.dirty());
}

void MediaCaptionModelTest::setTextSameTextDoesNotEmit()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("stable"));

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.setText(QStringLiteral("stable"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.dirty());
}

void MediaCaptionModelTest::setTextDoesNotTrim()
{
    MediaCaptionModel model;
    model.begin(QString());

    model.setText(QStringLiteral("  keep spaces  "));

    QCOMPARE(model.text(), QStringLiteral("  keep spaces  "));
    QCOMPARE(model.length(), 15);
}

void MediaCaptionModelTest::setTextCapsAtLimit()
{
    MediaCaptionModel model;
    model.begin(QString());

    model.setText(QString(1500, QLatin1Char('a')));

    QCOMPARE(model.length(), 1024);
    QCOMPARE(model.remaining(), 0);
    QCOMPARE(model.text(), QString(1024, QLatin1Char('a')));
    QVERIFY(model.canSave());
}

void MediaCaptionModelTest::setTextKeepsSurrogatePairThatFits()
{
    MediaCaptionModel model;
    model.begin(QString());

    // 1022 ASCII units plus the two-unit smiley is exactly the limit.
    const QString text = QString(1022, QLatin1Char('a')) + smiley();
    QCOMPARE(text.size(), 1024);
    model.setText(text);

    QCOMPARE(model.length(), 1024);
    QCOMPARE(model.remaining(), 0);
    QVERIFY(model.text().at(model.length() - 1).isLowSurrogate());
    QCOMPARE(model.text(), text);
}

void MediaCaptionModelTest::setTextDoesNotSplitSurrogatePair()
{
    MediaCaptionModel model;
    model.begin(QString());

    // The smiley would straddle the limit, so the cap drops it whole.
    const QString text = QString(1023, QLatin1Char('a')) + smiley();
    model.setText(text);

    QCOMPARE(model.length(), 1023);
    QCOMPARE(model.remaining(), 1);
    QVERIFY(!model.text().at(model.length() - 1).isHighSurrogate());
    QCOMPARE(model.text(), QString(1023, QLatin1Char('a')));
}

void MediaCaptionModelTest::lengthAndRemainingTrackText()
{
    MediaCaptionModel model;
    model.begin(QString());

    model.setText(QStringLiteral("abc"));
    QCOMPARE(model.length(), 3);
    QCOMPARE(model.remaining(), 1021);

    model.setText(QStringLiteral("abc") + smiley());
    QCOMPARE(model.length(), 5);
    QCOMPARE(model.remaining(), 1019);

    model.setText(QString());
    QCOMPARE(model.length(), 0);
    QCOMPARE(model.remaining(), 1024);
}

void MediaCaptionModelTest::remainingGoesNegativeForLongOriginal()
{
    MediaCaptionModel model;
    model.begin(QString(1200, QLatin1Char('a')));

    QCOMPARE(model.length(), 1200);
    QCOMPARE(model.remaining(), -176);
    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::dirtyTracksBaseline()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("base"));
    QVERIFY(!model.dirty());

    model.setText(QStringLiteral("base plus"));
    QVERIFY(model.dirty());

    model.setText(QStringLiteral("base"));
    QVERIFY(!model.dirty());
}

void MediaCaptionModelTest::dirtyIsFalseWhenTextIsRestored()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("keep"));
    model.setText(QString());
    QVERIFY(model.dirty());

    model.setText(QStringLiteral("keep"));
    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::canSaveRequiresEditWithinLimit()
{
    MediaCaptionModel model;
    QVERIFY(!model.canSave());

    model.begin(QStringLiteral("base"));
    QVERIFY(!model.canSave());

    model.setText(QStringLiteral("base edited"));
    QVERIFY(model.canSave());

    model.setText(QString(1024, QLatin1Char('b')));
    QVERIFY(model.canSave());
}

void MediaCaptionModelTest::canSaveIsFalseWhileInactive()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("base"));
    model.setText(QStringLiteral("edited"));
    model.cancel();

    QVERIFY(!model.active());
    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::markSavedAcceptsTextAndKeepsActive()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("before"));
    model.setText(QStringLiteral("after"));

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.markSaved();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.active());
    QCOMPARE(model.text(), QStringLiteral("after"));
    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::markSavedWhenCleanIsNoOp()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("clean"));

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.markSaved();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.active());
    QVERIFY(!model.dirty());
}

void MediaCaptionModelTest::markSavedIsIgnoredWhileInactive()
{
    MediaCaptionModel model;

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.markSaved();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.active());
    QVERIFY(model.text().isEmpty());
}

void MediaCaptionModelTest::cancelDeactivatesAndClears()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("base"));
    model.setText(QStringLiteral("edited"));

    QSignalSpy spy(&model, &MediaCaptionModel::changed);
    model.cancel();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.active());
    QVERIFY(model.text().isEmpty());
    QCOMPARE(model.length(), 0);
    QCOMPARE(model.remaining(), 1024);
    QVERIFY(!model.dirty());
    QVERIFY(!model.canSave());
}

void MediaCaptionModelTest::cancelWhileInactiveIsNoOp()
{
    MediaCaptionModel model;
    QSignalSpy spy(&model, &MediaCaptionModel::changed);

    model.cancel();
    model.cancel();

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.active());
}

void MediaCaptionModelTest::cancelDiscardsEdits()
{
    MediaCaptionModel model;
    model.begin(QStringLiteral("base"));
    model.setText(QStringLiteral("edited"));
    model.cancel();

    model.begin(QStringLiteral("base"));

    QCOMPARE(model.text(), QStringLiteral("base"));
    QVERIFY(!model.dirty());
}

QTEST_GUILESS_MAIN(MediaCaptionModelTest)
#include "tst_media_caption_model.moc"
