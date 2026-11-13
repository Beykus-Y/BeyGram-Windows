#include "AttachmentFacade.h"

#include "telegram/AttachmentOptions.h"

#include <QVector>

namespace beygram::ui {
namespace {

// One place that turns a QML string into a kind, so acceptFilter() and choose()
// cannot disagree about what counts as a known name. attachmentKindFromName()
// falls back to Photo for an unknown name; the ok flag is what distinguishes a
// real "photo" from a typo.
bool parseKind(const QString& name, telegram::AttachmentKind* kind)
{
    bool ok = false;
    const telegram::AttachmentKind parsed = telegram::attachmentKindFromName(name, &ok);
    if (!ok)
        return false;

    *kind = parsed;
    return true;
}

} // namespace

AttachmentFacade::AttachmentFacade(QObject* parent)
    : QObject(parent)
{
    const QVector<telegram::AttachmentOption> options =
        telegram::defaultAttachmentOptions();
    m_titles.reserve(options.size());
    m_kinds.reserve(options.size());
    for (const telegram::AttachmentOption& option : options) {
        m_titles.append(option.title);
        m_kinds.append(telegram::attachmentKindName(option.kind));
    }
}

QStringList AttachmentFacade::titles() const
{
    return m_titles;
}

QStringList AttachmentFacade::kinds() const
{
    return m_kinds;
}

QString AttachmentFacade::acceptFilter(const QString& kind) const
{
    telegram::AttachmentKind parsed = telegram::AttachmentKind::Photo;
    if (!parseKind(kind, &parsed))
        return {};

    return telegram::attachmentAcceptFilter(parsed);
}

void AttachmentFacade::choose(const QString& kind)
{
    telegram::AttachmentKind parsed = telegram::AttachmentKind::Photo;
    if (!parseKind(kind, &parsed))
        return;

    switch (parsed) {
    case telegram::AttachmentKind::Photo:
        emit photoRequested();
        break;
    case telegram::AttachmentKind::Video:
        emit videoRequested();
        break;
    case telegram::AttachmentKind::Document:
        emit documentRequested();
        break;
    case telegram::AttachmentKind::Audio:
        emit audioRequested();
        break;
    case telegram::AttachmentKind::Location:
        emit locationRequested();
        break;
    case telegram::AttachmentKind::Contact:
        emit contactRequested();
        break;
    case telegram::AttachmentKind::Poll:
        emit pollRequested();
        break;
    case telegram::AttachmentKind::Sticker:
        emit stickerRequested();
        break;
    }
}

} // namespace beygram::ui
