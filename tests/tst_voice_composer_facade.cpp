#include "ui/VoiceComposerFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QVariantList>
#include <QtTest>

using beygram::ui::VoiceComposerFacade;

class VoiceComposerFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsIdle();
    void startBeginsRecording();
    void startIsIgnoredWhileRecording();
    void startClearsThePreviousWaveform();
    void tickSetsElapsedTime();
    void tickIsIgnoredWhileIdle();
    void elapsedTextFormatsMinutesAndSeconds();
    void setPausedTogglesOnlyWhileRecording();
    void setLockedTogglesOnlyWhileRecording();
    void canSendRequiresOneToFiftyNineSeconds();
    void finishEmitsSendRequestedWithDuration();
    void finishKeepsRecordingWhenTooShort();
    void finishKeepsRecordingWhenTooLong();
    void finishIsIgnoredWhileIdle();
    void cancelEmitsCancelRequested();
    void cancelIsIgnoredWhileIdle();
    void cancelClearsTheWaveform();
    void waveformNormalizesSamples();
    void waveformClampsNegativeSamples();
    void waveformHandlesEmptyAndZeroEnvelopes();
    void waveformDropsNonNumericSamples();
    void sendRequestedStillSeesTheFinalWaveform();
    void changedTracksRealChanges();
    void exposesQmlPropertyContract();
};

void VoiceComposerFacadeTest::startsIdle()
{
    VoiceComposerFacade facade;
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    QVERIFY(!facade.recording());
    QVERIFY(!facade.paused());
    QVERIFY(!facade.locked());
    QCOMPARE(facade.elapsed(), 0);
    QCOMPARE(facade.elapsedText(), QStringLiteral("0:00"));
    QVERIFY(facade.waveform().isEmpty());
    QVERIFY(!facade.canSend());
    QCOMPARE(changedSpy.count(), 0);
}

void VoiceComposerFacadeTest::startBeginsRecording()
{
    VoiceComposerFacade facade;
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    facade.start();

    QVERIFY(facade.recording());
    QVERIFY(!facade.paused());
    QVERIFY(!facade.locked());
    QCOMPARE(facade.elapsed(), 0);
    QCOMPARE(changedSpy.count(), 1);
}

void VoiceComposerFacadeTest::startIsIgnoredWhileRecording()
{
    VoiceComposerFacade facade;
    facade.start();
    facade.tick(7);
    facade.setPaused(true);

    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);
    facade.start();

    // A second press must not restart the timer or resume a paused recording.
    QCOMPARE(facade.elapsed(), 7);
    QVERIFY(facade.paused());
    QCOMPARE(changedSpy.count(), 0);
}

void VoiceComposerFacadeTest::startClearsThePreviousWaveform()
{
    VoiceComposerFacade facade;
    facade.setWaveform(QVariantList{4, 8, 16});
    QCOMPARE(facade.waveform().size(), 3);

    facade.start();

    QVERIFY(facade.waveform().isEmpty());
    QVERIFY(facade.recording());
}

void VoiceComposerFacadeTest::tickSetsElapsedTime()
{
    VoiceComposerFacade facade;
    facade.start();

    facade.tick(12);
    QCOMPARE(facade.elapsed(), 12);

    facade.tick(13);
    QCOMPARE(facade.elapsed(), 13);

    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);
    facade.tick(13);
    QCOMPARE(changedSpy.count(), 0);
}

void VoiceComposerFacadeTest::tickIsIgnoredWhileIdle()
{
    VoiceComposerFacade facade;
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    facade.tick(30);

    QCOMPARE(facade.elapsed(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void VoiceComposerFacadeTest::elapsedTextFormatsMinutesAndSeconds()
{
    VoiceComposerFacade facade;
    QCOMPARE(facade.elapsedText(), QStringLiteral("0:00"));

    facade.start();

    facade.tick(5);
    QCOMPARE(facade.elapsedText(), QStringLiteral("0:05"));

    facade.tick(59);
    QCOMPARE(facade.elapsedText(), QStringLiteral("0:59"));

    facade.tick(65);
    QCOMPARE(facade.elapsedText(), QStringLiteral("1:05"));

    facade.tick(600);
    QCOMPARE(facade.elapsedText(), QStringLiteral("10:00"));

    // Negative values are clamped to zero by the controller.
    facade.tick(-1);
    QCOMPARE(facade.elapsedText(), QStringLiteral("0:00"));

    // The controller caps reported time at one hour.
    facade.tick(5000);
    QCOMPARE(facade.elapsed(), 3600);
    QCOMPARE(facade.elapsedText(), QStringLiteral("60:00"));
}

void VoiceComposerFacadeTest::setPausedTogglesOnlyWhileRecording()
{
    VoiceComposerFacade facade;

    facade.setPaused(true);
    QVERIFY(!facade.paused());

    facade.start();
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    facade.setPaused(true);
    QVERIFY(facade.paused());
    QCOMPARE(changedSpy.count(), 1);

    facade.setPaused(true);
    QVERIFY(facade.paused());
    QCOMPARE(changedSpy.count(), 1);

    facade.setPaused(false);
    QVERIFY(!facade.paused());
    QCOMPARE(changedSpy.count(), 2);
}

void VoiceComposerFacadeTest::setLockedTogglesOnlyWhileRecording()
{
    VoiceComposerFacade facade;

    facade.setLocked(true);
    QVERIFY(!facade.locked());

    facade.start();
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    facade.setLocked(true);
    QVERIFY(facade.locked());
    QCOMPARE(changedSpy.count(), 1);

    facade.setLocked(true);
    QVERIFY(facade.locked());
    QCOMPARE(changedSpy.count(), 1);

    // Lock survives a pause: the clip stays hands-free.
    facade.setPaused(true);
    QVERIFY(facade.locked());
    QVERIFY(facade.paused());

    facade.setLocked(false);
    QVERIFY(!facade.locked());
}

void VoiceComposerFacadeTest::canSendRequiresOneToFiftyNineSeconds()
{
    VoiceComposerFacade facade;
    QVERIFY(!facade.canSend());

    facade.start();
    QVERIFY(!facade.canSend());

    facade.tick(1);
    QVERIFY(facade.canSend());

    facade.tick(59);
    QVERIFY(facade.canSend());

    facade.tick(60);
    QVERIFY(!facade.canSend());

    facade.tick(3600);
    QVERIFY(!facade.canSend());

    facade.tick(30);
    QVERIFY(facade.canSend());

    // A paused or locked clip is still sendable.
    facade.setPaused(true);
    QVERIFY(facade.canSend());
    facade.setLocked(true);
    QVERIFY(facade.canSend());
}

void VoiceComposerFacadeTest::finishEmitsSendRequestedWithDuration()
{
    VoiceComposerFacade facade;
    facade.start();
    facade.tick(12);

    QSignalSpy sendSpy(&facade, &VoiceComposerFacade::sendRequested);
    facade.finish();

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.first().at(0).toInt(), 12);

    QVERIFY(!facade.recording());
    QCOMPARE(facade.elapsed(), 0);
    QCOMPARE(facade.elapsedText(), QStringLiteral("0:00"));
    QVERIFY(!facade.canSend());
}

void VoiceComposerFacadeTest::finishKeepsRecordingWhenTooShort()
{
    VoiceComposerFacade facade;
    facade.start();

    QSignalSpy sendSpy(&facade, &VoiceComposerFacade::sendRequested);
    facade.finish();

    QCOMPARE(sendSpy.count(), 0);
    QVERIFY(facade.recording());

    facade.tick(1);
    facade.finish();

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.first().at(0).toInt(), 1);
    QVERIFY(!facade.recording());
}

void VoiceComposerFacadeTest::finishKeepsRecordingWhenTooLong()
{
    VoiceComposerFacade facade;
    facade.start();
    facade.tick(60);

    QSignalSpy sendSpy(&facade, &VoiceComposerFacade::sendRequested);
    facade.finish();

    QCOMPARE(sendSpy.count(), 0);
    QVERIFY(facade.recording());
    QCOMPARE(facade.elapsed(), 60);

    // Shortening the clip (or the recorder stopping its own timer) allows it
    // again, so the bar does not get stuck.
    facade.tick(59);
    facade.finish();

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.first().at(0).toInt(), 59);
    QVERIFY(!facade.recording());
}

void VoiceComposerFacadeTest::finishIsIgnoredWhileIdle()
{
    VoiceComposerFacade facade;
    QSignalSpy sendSpy(&facade, &VoiceComposerFacade::sendRequested);
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    facade.finish();

    QCOMPARE(sendSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
    QVERIFY(!facade.recording());
}

void VoiceComposerFacadeTest::cancelEmitsCancelRequested()
{
    VoiceComposerFacade facade;
    facade.start();
    facade.tick(9);
    facade.setPaused(true);

    QSignalSpy cancelSpy(&facade, &VoiceComposerFacade::cancelRequested);
    facade.cancel();

    QCOMPARE(cancelSpy.count(), 1);
    QVERIFY(!facade.recording());
    QVERIFY(!facade.paused());
    QCOMPARE(facade.elapsed(), 0);
}

void VoiceComposerFacadeTest::cancelIsIgnoredWhileIdle()
{
    VoiceComposerFacade facade;
    QSignalSpy cancelSpy(&facade, &VoiceComposerFacade::cancelRequested);
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    facade.cancel();

    QCOMPARE(cancelSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void VoiceComposerFacadeTest::cancelClearsTheWaveform()
{
    VoiceComposerFacade facade;
    facade.start();
    facade.setWaveform(QVariantList{5, 10});
    QCOMPARE(facade.waveform().size(), 2);

    facade.cancel();

    QVERIFY(facade.waveform().isEmpty());
}

void VoiceComposerFacadeTest::waveformNormalizesSamples()
{
    VoiceComposerFacade facade;

    facade.setWaveform(QVariantList{0, 50, 100});

    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("0.000"),
                                           QStringLiteral("0.500"),
                                           QStringLiteral("1.000")}));
}

void VoiceComposerFacadeTest::waveformClampsNegativeSamples()
{
    VoiceComposerFacade facade;

    facade.setWaveform(QVariantList{-5, 10, 20});

    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("0.000"),
                                           QStringLiteral("0.500"),
                                           QStringLiteral("1.000")}));
}

void VoiceComposerFacadeTest::waveformHandlesEmptyAndZeroEnvelopes()
{
    VoiceComposerFacade facade;

    facade.setWaveform(QVariantList{});
    QVERIFY(facade.waveform().isEmpty());

    facade.setWaveform(QVariantList{0, 0, 0});
    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("0.000"),
                                           QStringLiteral("0.000"),
                                           QStringLiteral("0.000")}));

    // Duplicates are kept in order: the envelope is not a set.
    facade.setWaveform(QVariantList{3, 3, 6});
    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("0.500"),
                                           QStringLiteral("0.500"),
                                           QStringLiteral("1.000")}));
}

void VoiceComposerFacadeTest::waveformDropsNonNumericSamples()
{
    VoiceComposerFacade facade;

    facade.setWaveform(QVariantList{QVariant(), QVariant(QStringLiteral("loud")), 8});

    QCOMPARE(facade.waveform(), (QStringList{QStringLiteral("1.000")}));
}

void VoiceComposerFacadeTest::sendRequestedStillSeesTheFinalWaveform()
{
    VoiceComposerFacade facade;
    facade.start();
    facade.setWaveform(QVariantList{1, 2});
    facade.tick(4);

    int barsAtSend = -1;
    connect(&facade, &VoiceComposerFacade::sendRequested, this, [&facade, &barsAtSend](int) {
        barsAtSend = facade.waveform().size();
    });

    QSignalSpy sendSpy(&facade, &VoiceComposerFacade::sendRequested);
    facade.finish();

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(barsAtSend, 2);
    QVERIFY(facade.waveform().isEmpty());
}

void VoiceComposerFacadeTest::changedTracksRealChanges()
{
    VoiceComposerFacade facade;
    QSignalSpy changedSpy(&facade, &VoiceComposerFacade::changed);

    facade.start();                          // 1
    facade.start();                          // ignored, already recording
    facade.setPaused(true);                  // 2
    facade.setPaused(true);                  // no-op
    facade.tick(3);                          // 3
    facade.tick(3);                          // no-op
    facade.setLocked(true);                  // 4
    facade.setWaveform(QVariantList{1, 2, 3}); // 5
    facade.setWaveform(QVariantList{1, 2, 3}); // no-op
    facade.cancel();                         // 6 (state) + 7 (envelope cleared)
    facade.cancel();                         // ignored, idle
    facade.setPaused(true);                  // ignored, idle
    facade.finish();                         // ignored, idle
    facade.start();                          // 8

    QCOMPARE(changedSpy.count(), 8);
    QVERIFY(facade.recording());
    QVERIFY(!facade.paused());
    QVERIFY(!facade.locked());
    QCOMPARE(facade.elapsed(), 0);
}

void VoiceComposerFacadeTest::exposesQmlPropertyContract()
{
    VoiceComposerFacade facade;
    const QMetaObject* meta = facade.metaObject();

    const QStringList names = {
        QStringLiteral("recording"),
        QStringLiteral("paused"),
        QStringLiteral("locked"),
        QStringLiteral("elapsed"),
        QStringLiteral("elapsedText"),
        QStringLiteral("waveform"),
        QStringLiteral("canSend"),
    };

    for (const QString& name : names) {
        const int index = meta->indexOfProperty(name.toUtf8().constData());
        QVERIFY2(index >= 0, qPrintable(name));

        const QMetaProperty property = meta->property(index);
        QVERIFY2(property.isReadable(), qPrintable(name));
        QCOMPARE(QByteArray(property.notifySignal().name()), QByteArray("changed"));
    }

    const QMetaProperty elapsed =
        meta->property(meta->indexOfProperty("elapsed"));
    QCOMPARE(QByteArray(elapsed.typeName()), QByteArray("int"));

    const QMetaProperty waveform =
        meta->property(meta->indexOfProperty("waveform"));
    QCOMPARE(QByteArray(waveform.typeName()), QByteArray("QStringList"));

    QVERIFY(meta->indexOfSignal("sendRequested(int)") >= 0);
    QVERIFY(meta->indexOfSignal("cancelRequested()") >= 0);
}

QTEST_GUILESS_MAIN(VoiceComposerFacadeTest)
#include "tst_voice_composer_facade.moc"
