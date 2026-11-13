#include "AttachmentOptions.h"

#include <QChar>
#include <QLatin1StringView>
#include <cmath>
#include <iterator>

namespace beygram::telegram {
namespace {

/// Everything the sheet and the name parser need to agree on, in one table, so
/// display order, names, titles and glyphs cannot drift apart.
struct KindDescriptor {
    AttachmentKind kind;
    const char* name;
    const char* title;
    char16_t glyph;
};

/// Glyphs are written as code points instead of literals so the source file
/// stays ASCII and does not depend on the compiler's execution charset:
///   U+25A3 white square containing black small square  (photo)
///   U+25B6 black right-pointing triangle               (video)
///   U+25A4 square with horizontal fill                 (document)
///   U+266A eighth note                                 (audio)
///   U+2316 position indicator                          (location)
///   U+260E black telephone                             (contact)
///   U+2263 strictly equivalent to                      (poll)
///   U+263A white smiling face                          (sticker)
constexpr KindDescriptor kKindDescriptors[] = {
    {AttachmentKind::Photo, "photo", "Photo", 0x25A3},
    {AttachmentKind::Video, "video", "Video", 0x25B6},
    {AttachmentKind::Document, "document", "Document", 0x25A4},
    {AttachmentKind::Audio, "audio", "Audio", 0x266A},
    {AttachmentKind::Location, "location", "Location", 0x2316},
    {AttachmentKind::Contact, "contact", "Contact", 0x260E},
    {AttachmentKind::Poll, "poll", "Poll", 0x2263},
    {AttachmentKind::Sticker, "sticker", "Sticker", 0x263A},
};

/// The descriptor for a valid enum value, or null for a value outside it.
const KindDescriptor* descriptorFor(AttachmentKind kind)
{
    for (const KindDescriptor& descriptor : kKindDescriptors) {
        if (descriptor.kind == kind)
            return &descriptor;
    }
    return nullptr;
}

} // namespace

QVector<AttachmentOption> defaultAttachmentOptions()
{
    QVector<AttachmentOption> options;
    options.reserve(static_cast<qsizetype>(std::size(kKindDescriptors)));

    for (const KindDescriptor& descriptor : kKindDescriptors) {
        AttachmentOption option;
        option.kind = descriptor.kind;
        option.title = QString::fromLatin1(descriptor.title);
        option.glyph = QString(QChar(descriptor.glyph));
        option.enabled = true;
        options.append(option);
    }
    return options;
}

QString attachmentKindName(AttachmentKind kind)
{
    const KindDescriptor* descriptor = descriptorFor(kind);
    return descriptor ? QString::fromLatin1(descriptor->name) : QString();
}

AttachmentKind attachmentKindFromName(const QString& name, bool* ok)
{
    const QString trimmed = name.trimmed();
    for (const KindDescriptor& descriptor : kKindDescriptors) {
        if (trimmed.compare(QLatin1StringView(descriptor.name), Qt::CaseInsensitive) == 0) {
            if (ok)
                *ok = true;
            return descriptor.kind;
        }
    }

    if (ok)
        *ok = false;
    return AttachmentKind::Photo;
}

QString attachmentAcceptFilter(AttachmentKind kind)
{
    switch (kind) {
    case AttachmentKind::Photo:
        return QStringLiteral("*.jpg *.jpeg *.png *.webp");
    case AttachmentKind::Video:
        return QStringLiteral("*.mp4 *.mov *.mkv");
    case AttachmentKind::Document:
        return QStringLiteral("*");
    case AttachmentKind::Audio:
        return QStringLiteral("*.mp3 *.m4a *.ogg *.flac");
    case AttachmentKind::Location:
    case AttachmentKind::Contact:
    case AttachmentKind::Poll:
    case AttachmentKind::Sticker:
        // These kinds are not chosen from a file dialog.
        return QString();
    }
    return QString();
}

QJsonObject locationMessageRequest(double latitude, double longitude)
{
    // std::isfinite rejects NaN, which every range comparison would let
    // through, and infinities, which would serialize as invalid JSON.
    if (!std::isfinite(latitude) || !std::isfinite(longitude)
        || latitude < -90.0 || latitude > 90.0
        || longitude < -180.0 || longitude > 180.0) {
        return {};
    }

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageLocation")},
        {QStringLiteral("location"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("location")},
             {QStringLiteral("latitude"), latitude},
             {QStringLiteral("longitude"), longitude},
             {QStringLiteral("horizontal_accuracy"), 0.0},
         }},
    };
}

QJsonObject contactMessageRequest(qint64 userId)
{
    if (userId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("inputMessageContact")},
        {QStringLiteral("contact"),
         QJsonObject{
             {QStringLiteral("@type"), QStringLiteral("contact")},
             // Only the id is known here; the caller can fill the text
             // fields from its cached user before sending.
             {QStringLiteral("phone_number"), QStringLiteral("")},
             {QStringLiteral("first_name"), QStringLiteral("")},
             {QStringLiteral("last_name"), QStringLiteral("")},
             {QStringLiteral("vcard"), QStringLiteral("")},
             {QStringLiteral("user_id"), userId},
         }},
    };
}

} // namespace beygram::telegram
