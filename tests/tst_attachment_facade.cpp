#include "ui/AttachmentFacade.h"

#include "telegram/AttachmentOptions.h"

#include <QByteArray>
#include <QMetaProperty>
#include <QSet>
#include <QSignalSpy>
#include <QStringList>
#include <QVector>
#include <QtTest>

#include <initializer_list>

using beygram::telegram::attachmentKindName;
using beygram::ui::AttachmentFacade;

class AttachmentFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesEveryKindInDisplayOrder();
    void titlesAndKindsAreParallelAndNonEmpty();
    void returnedListsAreCopies();
    void exposesTheQmlProperties();

    void acceptFilterMatchesTheKind();
    void acceptFilterIsEmptyForNonFileKinds();
    void acceptFilterNormalizesTheName();
    void acceptFilterRejectsUnknownNames();

    void chooseEmitsExactlyTheMatchingSignal();
    void chooseNormalizesTheName();
    void chooseRepeatsEveryTap();
    void chooseIgnoresUnknownNames();
    void chooseLeavesTheListsAlone();

    void changedIsNotEmitted();
};

void AttachmentFacadeTest::exposesEveryKindInDisplayOrder()
{
    AttachmentFacade facade;

    const QStringList expectedKinds = {
        QStringLiteral("photo"),
        QStringLiteral("video"),
        QStringLiteral("document"),
        QStringLiteral("audio"),
        QStringLiteral("location"),
        QStringLiteral("contact"),
        QStringLiteral("poll"),
        QStringLiteral("sticker"),
    };
    QCOMPARE(facade.kinds(), expectedKinds);

    const QStringList expectedTitles = {
        QStringLiteral("Photo"),
        QStringLiteral("Video"),
        QStringLiteral("Document"),
        QStringLiteral("Audio"),
        QStringLiteral("Location"),
        QStringLiteral("Contact"),
        QStringLiteral("Poll"),
        QStringLiteral("Sticker"),
    };
    QCOMPARE(facade.titles(), expectedTitles);
}

void AttachmentFacadeTest::titlesAndKindsAreParallelAndNonEmpty()
{
    AttachmentFacade facade;
    const QVector<beygram::telegram::AttachmentOption> options =
        beygram::telegram::defaultAttachmentOptions();

    QCOMPARE(facade.kinds().size(), options.size());
    QCOMPARE(facade.titles().size(), options.size());

    QSet<QString> titles;
    QSet<QString> kinds;
    for (qsizetype i = 0; i < options.size(); ++i) {
        // The facade must not reorder or rename what the C++ table declares.
        QCOMPARE(facade.kinds().at(i), attachmentKindName(options.at(i).kind));
        QCOMPARE(facade.titles().at(i), options.at(i).title);

        QVERIFY(!facade.kinds().at(i).isEmpty());
        QVERIFY(!facade.titles().at(i).isEmpty());
        QCOMPARE(facade.titles().at(i), facade.titles().at(i).trimmed());

        titles.insert(facade.titles().at(i));
        kinds.insert(facade.kinds().at(i));
    }

    // A repeated kind or title would render two indistinguishable tiles.
    QCOMPARE(titles.size(), options.size());
    QCOMPARE(kinds.size(), options.size());
}

void AttachmentFacadeTest::returnedListsAreCopies()
{
    AttachmentFacade facade;

    // A QML view may sort or splice what it receives; that must not reach the
    // facade's own lists.
    QStringList titles = facade.titles();
    titles.first() = QStringLiteral("Changed");
    titles.append(QStringLiteral("Extra"));

    QStringList kinds = facade.kinds();
    kinds.clear();

    QCOMPARE(facade.titles().size(), qsizetype(8));
    QCOMPARE(facade.titles().first(), QStringLiteral("Photo"));
    QCOMPARE(facade.kinds().size(), qsizetype(8));
    QCOMPARE(facade.kinds().first(), QStringLiteral("photo"));
}

void AttachmentFacadeTest::exposesTheQmlProperties()
{
    AttachmentFacade facade;
    const QMetaObject* meta = facade.metaObject();

    for (const char* name : {"titles", "kinds"}) {
        const int index = meta->indexOfProperty(name);
        QVERIFY2(index >= 0, name);

        const QMetaProperty property = meta->property(index);
        QVERIFY(property.isReadable());
        QVERIFY(!property.isWritable());
        QCOMPARE(property.notifySignal().name(), QByteArray("changed"));
    }

    // QML calls these two by name, so they must stay invokable.
    QVERIFY(meta->indexOfMethod("acceptFilter(QString)") >= 0);
    QVERIFY(meta->indexOfMethod("choose(QString)") >= 0);
}

void AttachmentFacadeTest::acceptFilterMatchesTheKind()
{
    AttachmentFacade facade;

    QCOMPARE(facade.acceptFilter(QStringLiteral("photo")),
             QStringLiteral("*.jpg *.jpeg *.png *.webp"));
    QCOMPARE(facade.acceptFilter(QStringLiteral("video")),
             QStringLiteral("*.mp4 *.mov *.mkv"));
    QCOMPARE(facade.acceptFilter(QStringLiteral("document")), QStringLiteral("*"));
    QCOMPARE(facade.acceptFilter(QStringLiteral("audio")),
             QStringLiteral("*.mp3 *.m4a *.ogg *.flac"));
}

void AttachmentFacadeTest::acceptFilterIsEmptyForNonFileKinds()
{
    AttachmentFacade facade;

    // These kinds are chosen in the app, not in a file dialog.
    QVERIFY(facade.acceptFilter(QStringLiteral("location")).isEmpty());
    QVERIFY(facade.acceptFilter(QStringLiteral("contact")).isEmpty());
    QVERIFY(facade.acceptFilter(QStringLiteral("poll")).isEmpty());
    QVERIFY(facade.acceptFilter(QStringLiteral("sticker")).isEmpty());
}

void AttachmentFacadeTest::acceptFilterNormalizesTheName()
{
    AttachmentFacade facade;

    // The name may come from a settings file or a user-editable config.
    QCOMPARE(facade.acceptFilter(QStringLiteral(" Photo ")),
             QStringLiteral("*.jpg *.jpeg *.png *.webp"));
    QCOMPARE(facade.acceptFilter(QStringLiteral("VIDEO")),
             QStringLiteral("*.mp4 *.mov *.mkv"));
    QCOMPARE(facade.acceptFilter(QStringLiteral("\tAudio\n")),
             QStringLiteral("*.mp3 *.m4a *.ogg *.flac"));
    QCOMPARE(facade.acceptFilter(QStringLiteral("  document  ")), QStringLiteral("*"));
    QVERIFY(facade.acceptFilter(QStringLiteral(" LOCATION ")).isEmpty());
}

void AttachmentFacadeTest::acceptFilterRejectsUnknownNames()
{
    AttachmentFacade facade;

    QVERIFY(facade.acceptFilter(QString()).isEmpty());
    QVERIFY(facade.acceptFilter(QStringLiteral("   ")).isEmpty());
    QVERIFY(facade.acceptFilter(QStringLiteral("gif")).isEmpty());
    QVERIFY(facade.acceptFilter(QStringLiteral("image")).isEmpty());
    // A prefix of a real name is not a match.
    QVERIFY(facade.acceptFilter(QStringLiteral("photograph")).isEmpty());
}

void AttachmentFacadeTest::chooseEmitsExactlyTheMatchingSignal()
{
    AttachmentFacade facade;

    QSignalSpy photo(&facade, &AttachmentFacade::photoRequested);
    QSignalSpy video(&facade, &AttachmentFacade::videoRequested);
    QSignalSpy document(&facade, &AttachmentFacade::documentRequested);
    QSignalSpy audio(&facade, &AttachmentFacade::audioRequested);
    QSignalSpy location(&facade, &AttachmentFacade::locationRequested);
    QSignalSpy contact(&facade, &AttachmentFacade::contactRequested);
    QSignalSpy poll(&facade, &AttachmentFacade::pollRequested);
    QSignalSpy sticker(&facade, &AttachmentFacade::stickerRequested);

    QSignalSpy* const spies[] = {
        &photo, &video, &document, &audio, &location, &contact, &poll, &sticker,
    };
    const QStringList kinds = facade.kinds();
    QCOMPARE(kinds.size(), qsizetype(8));

    // Choose the tiles in display order; each tap must light up its own signal
    // and no other, and earlier choices must not repeat.
    for (qsizetype step = 0; step < kinds.size(); ++step) {
        facade.choose(kinds.at(step));
        for (qsizetype i = 0; i < kinds.size(); ++i) {
            QCOMPARE(spies[i]->count(), i <= step ? 1 : 0);
        }
    }
}

void AttachmentFacadeTest::chooseNormalizesTheName()
{
    AttachmentFacade facade;
    QSignalSpy video(&facade, &AttachmentFacade::videoRequested);
    QSignalSpy audio(&facade, &AttachmentFacade::audioRequested);

    facade.choose(QStringLiteral("  VIDEO  "));
    QCOMPARE(video.count(), 1);

    facade.choose(QStringLiteral("\tAudio\n"));
    QCOMPARE(audio.count(), 1);
}

void AttachmentFacadeTest::chooseRepeatsEveryTap()
{
    AttachmentFacade facade;
    QSignalSpy photo(&facade, &AttachmentFacade::photoRequested);

    // The owner may cancel the first picker and tap the tile again, so a
    // repeated choice is a repeated request, not a no-op.
    facade.choose(QStringLiteral("photo"));
    facade.choose(QStringLiteral("photo"));

    QCOMPARE(photo.count(), 2);
}

void AttachmentFacadeTest::chooseIgnoresUnknownNames()
{
    AttachmentFacade facade;

    QSignalSpy photo(&facade, &AttachmentFacade::photoRequested);
    QSignalSpy video(&facade, &AttachmentFacade::videoRequested);
    QSignalSpy document(&facade, &AttachmentFacade::documentRequested);
    QSignalSpy audio(&facade, &AttachmentFacade::audioRequested);
    QSignalSpy location(&facade, &AttachmentFacade::locationRequested);
    QSignalSpy contact(&facade, &AttachmentFacade::contactRequested);
    QSignalSpy poll(&facade, &AttachmentFacade::pollRequested);
    QSignalSpy sticker(&facade, &AttachmentFacade::stickerRequested);

    QSignalSpy* const spies[] = {
        &photo, &video, &document, &audio, &location, &contact, &poll, &sticker,
    };

    facade.choose(QString());
    facade.choose(QStringLiteral("   "));
    facade.choose(QStringLiteral("gif"));
    facade.choose(QStringLiteral("image"));
    facade.choose(QStringLiteral("photograph"));

    // In particular, an unknown name must not fall back to photoRequested().
    for (const QSignalSpy* spy : spies)
        QCOMPARE(spy->count(), 0);
}

void AttachmentFacadeTest::chooseLeavesTheListsAlone()
{
    AttachmentFacade facade;
    const QStringList titles = facade.titles();
    const QStringList kinds = facade.kinds();

    facade.choose(QStringLiteral("photo"));
    facade.choose(QStringLiteral("sticker"));

    QCOMPARE(facade.titles(), titles);
    QCOMPARE(facade.kinds(), kinds);
}

void AttachmentFacadeTest::changedIsNotEmitted()
{
    AttachmentFacade facade;
    QSignalSpy changed(&facade, &AttachmentFacade::changed);

    QVERIFY(!facade.titles().isEmpty());
    QVERIFY(!facade.kinds().isEmpty());
    QCOMPARE(facade.acceptFilter(QStringLiteral("photo")),
             QStringLiteral("*.jpg *.jpeg *.png *.webp"));

    facade.choose(QStringLiteral("photo"));
    facade.choose(QStringLiteral("sticker"));
    facade.choose(QStringLiteral("nope"));

    // The lists are fixed at construction, so reading them and asking for a
    // picker never announces a state change.
    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(AttachmentFacadeTest)
#include "tst_attachment_facade.moc"
