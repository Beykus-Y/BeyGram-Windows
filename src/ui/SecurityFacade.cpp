#include "SecurityFacade.h"

#include "telegram/SecretChatSafetyController.h"
#include "telegram/SpoilerRevealModel.h"

namespace beygram::ui {

namespace {

/// Parses a QML-provided chat id. Anything that is not an integer yields 0,
/// which unloads the chat. Negative ids are kept: groups and channels use them.
qint64 parseChatId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok ? id : 0;
}

/// Parses a QML-provided message id. Anything that is not a positive integer
/// yields 0, which the spoiler model treats as "ignore".
qint64 parseMessageId(const QString& value)
{
    bool ok = false;
    const qint64 id = value.trimmed().toLongLong(&ok);
    return ok && id > 0 ? id : 0;
}

// Unit boundaries for ttlText(). ttlText() and the banner's selector share
// them, so both read a timer the same way.
constexpr int kSecondsPerMinute = 60;
constexpr int kSecondsPerHour = 60 * kSecondsPerMinute;
constexpr int kSecondsPerDay = 24 * kSecondsPerHour;

} // namespace

SecurityFacade::SecurityFacade(QObject* parent)
    : QObject(parent)
    , m_safety(new telegram::SecretChatSafetyController(this))
    , m_spoilers(new telegram::SpoilerRevealModel(this))
{
    // Both sources speak through one signal, so a view can bind every property
    // to changed() and never watch two objects.
    connect(m_safety, &telegram::SecretChatSafetyController::changed,
            this, &SecurityFacade::changed);
    connect(m_spoilers, &telegram::SpoilerRevealModel::changed,
            this, &SecurityFacade::changed);
}

bool SecurityFacade::secret() const
{
    return m_safety->secret();
}

int SecurityFacade::ttlSeconds() const
{
    return m_safety->ttlSeconds();
}

QString SecurityFacade::ttlText() const
{
    const int seconds = m_safety->ttlSeconds();
    if (seconds <= 0)
        return QStringLiteral("Off");

    int value = seconds;
    QString unit = QStringLiteral("seconds");
    if (seconds >= kSecondsPerDay) {
        value = seconds / kSecondsPerDay;
        unit = value == 1 ? QStringLiteral("day") : QStringLiteral("days");
    } else if (seconds >= kSecondsPerHour) {
        value = seconds / kSecondsPerHour;
        unit = value == 1 ? QStringLiteral("hour") : QStringLiteral("hours");
    } else if (seconds >= kSecondsPerMinute) {
        value = seconds / kSecondsPerMinute;
        unit = value == 1 ? QStringLiteral("minute") : QStringLiteral("minutes");
    } else {
        unit = value == 1 ? QStringLiteral("second") : QStringLiteral("seconds");
    }

    return QStringLiteral("%1 %2").arg(value).arg(unit);
}

bool SecurityFacade::notificationsAllowed() const
{
    return m_safety->notificationsAllowed();
}

bool SecurityFacade::copyAllowed() const
{
    // Protected content denies copying even in a chat the secret-chat rules
    // would let copy.
    return m_safety->copyAllowed() && !m_protected;
}

bool SecurityFacade::screenshotWarn() const
{
    return m_safety->shouldWarnBeforeScreenshot();
}

bool SecurityFacade::protectedContent() const
{
    return m_protected;
}

void SecurityFacade::setChat(const QString& chatId, bool secret, bool protectedContent)
{
    const qint64 id = parseChatId(chatId);
    const bool chatChanged = id != m_safety->chatId();
    // setChat() on the safety controller is silent when neither the id nor the
    // secret flag moves, which is what lets a protection-only change announce
    // itself exactly once below.
    const bool safetySilent = !chatChanged && (m_safety->secret() == secret);
    // The flag belongs to a chat, so unloading drops it even when the caller
    // still passes true.
    const bool protection = id != 0 && protectedContent;
    const bool protectionChanged = protection != m_protected;

    m_protected = protection;

    // A reveal belongs to the chat that showed it; another chat must not
    // inherit it, even when it reuses a message id.
    if (chatChanged)
        m_spoilers->hideAll();

    m_safety->setChat(id, secret);

    if (protectionChanged && safetySilent)
        emit changed();
}

void SecurityFacade::setTtl(int seconds)
{
    // A timer without a chat has nowhere to live: the next load would clear it.
    if (m_safety->chatId() == 0)
        return;

    m_safety->setTtlSeconds(seconds);

    // Picking a timer is an explicit action, so it is announced even when the
    // clamped value is already in effect; the controller stays silent then and
    // changed() is not emitted.
    emit ttlRequested(QString::number(m_safety->chatId()), m_safety->ttlSeconds());
}

void SecurityFacade::setScreenshotAllowed(bool allowed)
{
    m_safety->setScreenshotAllowed(allowed);
}

void SecurityFacade::reveal(const QString& messageId)
{
    m_spoilers->reveal(parseMessageId(messageId));
}

void SecurityFacade::hideAllSpoilers()
{
    m_spoilers->hideAll();
}

bool SecurityFacade::isRevealed(const QString& messageId) const
{
    return m_spoilers->isRevealed(parseMessageId(messageId));
}

} // namespace beygram::ui
