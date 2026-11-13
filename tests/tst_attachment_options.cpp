#include "telegram/AttachmentOptions.h"

#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

QVector<AttachmentKind> allKinds()
{
    return {
        AttachmentKind::Photo, AttachmentKind::Video, AttachmentKind::Document,
        AttachmentKind::Audio, AttachmentKind::Location, AttachmentKind::Contact,
        AttachmentKind::Poll, AttachmentKind::Sticker,
    };
}

} // namespace

class AttachmentOptionsTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsCoverEveryKindInOrder();
    void defaultsAreEnabledWithTitlesAndGlyphs();
    void defaultKindsTitlesAndGlyphsAreUnique();
    void defaultsReturnIndependentCopies();

    void namesRoundTrip();
    void namesAreStableLowercase();
    void fromNameIgnoresCaseAndWhitespace();
    void fromNameRejectsUnknownNames();
    void invalidKindHasNoName();

    void acceptFiltersMatchTheDialogExtensions();
    void acceptFilterIsEmptyForNonFileKinds();

    void locationRequestBuildsCoordinates();
    void locationRequestAcceptsTheBounds();
    void locationRequestRejectsOutOfRange();
    void locationRequestRejectsNonFinite();

    void contactRequestBuildsUserCard();
    void contactRequestRejectsNonPositiveIds();
};

void AttachmentOptionsTest::defaultsCoverEveryKindInOrder()
{
    const QVector<AttachmentKind> expected = allKinds();
    const QVector<AttachmentOption> options = defaultAttachmentOptions();

    QCOMPARE(options.size(), expected.size());
    for (qsizetype i = 0; i < expected.size(); ++i)
        QCOMPARE(options.at(i).kind, expected.at(i));
}

void AttachmentOptionsTest::defaultsAreEnabledWithTitlesAndGlyphs()
{
    const QVector<AttachmentOption> options = defaultAttachmentOptions();

    for (const AttachmentOption& option : options) {
        QVERIFY(option.enabled);
        QVERIFY(!option.title.isEmpty());
        QVERIFY(!option.glyph.isEmpty());
        // The defaults use one BMP code unit per glyph; the UI may replace it.
        QCOMPARE(option.glyph.size(), qsizetype(1));
        QCOMPARE(option.title, option.title.trimmed());
    }
}

void AttachmentOptionsTest::defaultKindsTitlesAndGlyphsAreUnique()
{
    const QVector<AttachmentOption> options = defaultAttachmentOptions();
    QSet<int> kinds;
    QSet<QString> titles;
    QSet<QString> glyphs;

    for (const AttachmentOption& option : options) {
        kinds.insert(static_cast<int>(option.kind));
        titles.insert(option.title);
        glyphs.insert(option.glyph);
    }

    // A repeated kind, title or glyph would render two identical tiles.
    QCOMPARE(kinds.size(), options.size());
    QCOMPARE(titles.size(), options.size());
    QCOMPARE(glyphs.size(), options.size());
}

void AttachmentOptionsTest::defaultsReturnIndependentCopies()
{
    QVector<AttachmentOption> first = defaultAttachmentOptions();
    first.first().title = QStringLiteral("Changed");
    first.first().enabled = false;
    first.append(AttachmentOption{});

    const QVector<AttachmentOption> second = defaultAttachmentOptions();
    QCOMPARE(second.size(), allKinds().size());
    QCOMPARE(second.first().title, QStringLiteral("Photo"));
    QVERIFY(second.first().enabled);
}

void AttachmentOptionsTest::namesRoundTrip()
{
    for (const AttachmentKind kind : allKinds()) {
        const QString name = attachmentKindName(kind);
        QVERIFY(!name.isEmpty());

        bool ok = false;
        QCOMPARE(attachmentKindFromName(name, &ok), kind);
        QVERIFY(ok);
    }
}

void AttachmentOptionsTest::namesAreStableLowercase()
{
    QCOMPARE(attachmentKindName(AttachmentKind::Photo), QStringLiteral("photo"));
    QCOMPARE(attachmentKindName(AttachmentKind::Video), QStringLiteral("video"));
    QCOMPARE(attachmentKindName(AttachmentKind::Document), QStringLiteral("document"));
    QCOMPARE(attachmentKindName(AttachmentKind::Audio), QStringLiteral("audio"));
    QCOMPARE(attachmentKindName(AttachmentKind::Location), QStringLiteral("location"));
    QCOMPARE(attachmentKindName(AttachmentKind::Contact), QStringLiteral("contact"));
    QCOMPARE(attachmentKindName(AttachmentKind::Poll), QStringLiteral("poll"));
    QCOMPARE(attachmentKindName(AttachmentKind::Sticker), QStringLiteral("sticker"));
}

void AttachmentOptionsTest::fromNameIgnoresCaseAndWhitespace()
{
    bool ok = false;

    QCOMPARE(attachmentKindFromName(QStringLiteral("VIDEO"), &ok), AttachmentKind::Video);
    QVERIFY(ok);

    QCOMPARE(attachmentKindFromName(QStringLiteral("  Audio  "), &ok), AttachmentKind::Audio);
    QVERIFY(ok);

    QCOMPARE(attachmentKindFromName(QStringLiteral("\tSticker\n"), &ok),
             AttachmentKind::Sticker);
    QVERIFY(ok);
}

void AttachmentOptionsTest::fromNameRejectsUnknownNames()
{
    bool ok = true;

    QCOMPARE(attachmentKindFromName(QStringLiteral("gif"), &ok), AttachmentKind::Photo);
    QVERIFY(!ok);

    QCOMPARE(attachmentKindFromName(QString(), &ok), AttachmentKind::Photo);
    QVERIFY(!ok);

    QCOMPARE(attachmentKindFromName(QStringLiteral("   "), &ok), AttachmentKind::Photo);
    QVERIFY(!ok);

    // A prefix of a real name is not a match.
    QCOMPARE(attachmentKindFromName(QStringLiteral("photograph"), &ok), AttachmentKind::Photo);
    QVERIFY(!ok);

    // A null ok pointer is accepted when the caller only wants the fallback.
    QCOMPARE(attachmentKindFromName(QStringLiteral("poll")), AttachmentKind::Poll);
    QCOMPARE(attachmentKindFromName(QStringLiteral("nope")), AttachmentKind::Photo);
}

void AttachmentOptionsTest::invalidKindHasNoName()
{
    const auto unknown = static_cast<AttachmentKind>(255);

    QVERIFY(attachmentKindName(unknown).isEmpty());
    QVERIFY(attachmentAcceptFilter(unknown).isEmpty());
}

void AttachmentOptionsTest::acceptFiltersMatchTheDialogExtensions()
{
    QCOMPARE(attachmentAcceptFilter(AttachmentKind::Photo),
             QStringLiteral("*.jpg *.jpeg *.png *.webp"));
    QCOMPARE(attachmentAcceptFilter(AttachmentKind::Video),
             QStringLiteral("*.mp4 *.mov *.mkv"));
    QCOMPARE(attachmentAcceptFilter(AttachmentKind::Document), QStringLiteral("*"));
    QCOMPARE(attachmentAcceptFilter(AttachmentKind::Audio),
             QStringLiteral("*.mp3 *.m4a *.ogg *.flac"));
}

void AttachmentOptionsTest::acceptFilterIsEmptyForNonFileKinds()
{
    QVERIFY(attachmentAcceptFilter(AttachmentKind::Location).isEmpty());
    QVERIFY(attachmentAcceptFilter(AttachmentKind::Contact).isEmpty());
    QVERIFY(attachmentAcceptFilter(AttachmentKind::Poll).isEmpty());
    QVERIFY(attachmentAcceptFilter(AttachmentKind::Sticker).isEmpty());
}

void AttachmentOptionsTest::locationRequestBuildsCoordinates()
{
    const QJsonObject request = locationMessageRequest(55.7558, 37.6173);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageLocation"));
    // This is a content fragment; the caller wraps it in sendMessage.
    QVERIFY(!request.contains(QStringLiteral("chat_id")));

    const QJsonObject location = request.value(QStringLiteral("location")).toObject();
    QCOMPARE(location.value(QStringLiteral("@type")).toString(), QStringLiteral("location"));
    QCOMPARE(location.value(QStringLiteral("latitude")).toDouble(), 55.7558);
    QCOMPARE(location.value(QStringLiteral("longitude")).toDouble(), 37.6173);
    QCOMPARE(location.value(QStringLiteral("horizontal_accuracy")).toDouble(), 0.0);

    const QJsonObject southern = locationMessageRequest(-33.8688, 151.2093);
    const QJsonObject southernLocation =
        southern.value(QStringLiteral("location")).toObject();
    QCOMPARE(southernLocation.value(QStringLiteral("latitude")).toDouble(), -33.8688);
    QCOMPARE(southernLocation.value(QStringLiteral("longitude")).toDouble(), 151.2093);
}

void AttachmentOptionsTest::locationRequestAcceptsTheBounds()
{
    QVERIFY(!locationMessageRequest(90.0, 180.0).isEmpty());
    QVERIFY(!locationMessageRequest(-90.0, -180.0).isEmpty());
    QVERIFY(!locationMessageRequest(0.0, 0.0).isEmpty());
}

void AttachmentOptionsTest::locationRequestRejectsOutOfRange()
{
    QVERIFY(locationMessageRequest(90.0001, 0.0).isEmpty());
    QVERIFY(locationMessageRequest(-90.0001, 0.0).isEmpty());
    QVERIFY(locationMessageRequest(0.0, 180.0001).isEmpty());
    QVERIFY(locationMessageRequest(0.0, -180.0001).isEmpty());
    QVERIFY(locationMessageRequest(91.0, 181.0).isEmpty());
    QVERIFY(locationMessageRequest(-91.0, -181.0).isEmpty());
}

void AttachmentOptionsTest::locationRequestRejectsNonFinite()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    // NaN passes every range comparison, so it needs an explicit check.
    QVERIFY(locationMessageRequest(nan, 0.0).isEmpty());
    QVERIFY(locationMessageRequest(0.0, nan).isEmpty());
    QVERIFY(locationMessageRequest(infinity, 0.0).isEmpty());
    QVERIFY(locationMessageRequest(-infinity, 0.0).isEmpty());
    QVERIFY(locationMessageRequest(0.0, infinity).isEmpty());
    QVERIFY(locationMessageRequest(0.0, -infinity).isEmpty());
}

void AttachmentOptionsTest::contactRequestBuildsUserCard()
{
    const qint64 userId = 1234567890123LL;
    const QJsonObject request = contactMessageRequest(userId);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageContact"));
    // This is a content fragment; the caller wraps it in sendMessage.
    QVERIFY(!request.contains(QStringLiteral("chat_id")));

    const QJsonObject contact = request.value(QStringLiteral("contact")).toObject();
    QCOMPARE(contact.value(QStringLiteral("@type")).toString(), QStringLiteral("contact"));
    QCOMPARE(contact.value(QStringLiteral("user_id")).toVariant().toLongLong(), userId);

    // Empty strings, not null: the schema requires the text fields.
    QVERIFY(contact.value(QStringLiteral("phone_number")).isString());
    QVERIFY(contact.value(QStringLiteral("first_name")).isString());
    QVERIFY(contact.value(QStringLiteral("last_name")).isString());
    QVERIFY(contact.value(QStringLiteral("vcard")).isString());
    QVERIFY(contact.value(QStringLiteral("phone_number")).toString().isEmpty());
    QVERIFY(contact.value(QStringLiteral("first_name")).toString().isEmpty());
    QVERIFY(contact.value(QStringLiteral("last_name")).toString().isEmpty());
    QVERIFY(contact.value(QStringLiteral("vcard")).toString().isEmpty());
}

void AttachmentOptionsTest::contactRequestRejectsNonPositiveIds()
{
    QVERIFY(contactMessageRequest(0).isEmpty());
    QVERIFY(contactMessageRequest(-1).isEmpty());
    QVERIFY(contactMessageRequest(std::numeric_limits<qint64>::min()).isEmpty());
}

QTEST_GUILESS_MAIN(AttachmentOptionsTest)
#include "tst_attachment_options.moc"
