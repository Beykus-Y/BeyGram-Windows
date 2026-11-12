#include "ui/NotificationsFacade.h"

#include <QChar>
#include <QLatin1Char>
#include <QString>
#include <QVector>

#include <utility>

namespace beygram::ui {
namespace {

// The separator of the quiet-hours window: a spaced en dash, written as a code
// point so the source stays ASCII and the literal does not depend on the
// compiler's source charset, matching the labels in the rest of this tree.
constexpr char16_t kEnDash = 0x2013;

// Minutes of the day as the settings screen prints them: "22:00", "08:05".
// The model clamps every stored bound to 0..1439, so the value always names a
// real minute of the day.
QString clockText(int minutesOfDay)
{
    const int hour = minutesOfDay / 60;
    const int minute = minutesOfDay % 60;

    return QString::number(hour).rightJustified(2, QLatin1Char('0'))
        + QLatin1Char(':')
        + QString::number(minute).rightJustified(2, QLatin1Char('0'));
}

// A TDLib-shaped id from the string lists. An id that is not a number reads as
// 0, which the models treat as "no value" for every field this facade parses
// (no sound, no chat), so a malformed entry is dropped instead of guessed at.
qint64 toInt64(const QString& text)
{
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    return ok ? value : 0;
}

} // namespace

NotificationsFacade::NotificationsFacade(QObject* parent)
    : QObject(parent)
    , m_settings(this)
    , m_sounds(this)
    , m_chat(this)
{
    // The facade holds no state of its own: every property is derived from one
    // of the three collaborators, so their changes are the facade's changes.
    // Each one suppresses no-op writes, so redundant calls stay silent.
    connect(&m_settings, &telegram::NotificationSettingsModel::changed,
            this, &NotificationsFacade::changed);
    connect(&m_sounds, &telegram::NotificationSoundModel::changed,
            this, &NotificationsFacade::changed);
    connect(&m_chat, &telegram::ChatNotificationController::changed,
            this, &NotificationsFacade::changed);
}

bool NotificationsFacade::enabled() const
{
    return m_settings.enabled();
}

void NotificationsFacade::setEnabled(bool on)
{
    m_settings.setEnabled(on);
}

bool NotificationsFacade::sound() const
{
    return m_settings.soundEnabled();
}

void NotificationsFacade::setSound(bool on)
{
    m_settings.setSoundEnabled(on);
}

bool NotificationsFacade::preview() const
{
    return m_settings.previewEnabled();
}

void NotificationsFacade::setPreview(bool on)
{
    m_settings.setPreviewEnabled(on);
}

bool NotificationsFacade::dnd() const
{
    return m_settings.dnd();
}

void NotificationsFacade::setDnd(bool on)
{
    m_settings.setDnd(on);
}

int NotificationsFacade::quietStart() const
{
    return m_settings.quietStartMinutes();
}

int NotificationsFacade::quietEnd() const
{
    return m_settings.quietEndMinutes();
}

QString NotificationsFacade::quietText() const
{
    // Equal bounds are the model's "quiet hours off"; there is no window to
    // print, so the screen shows the off state instead of a fake "22:00 - 22:00".
    if (!m_settings.quietHoursActive())
        return QStringLiteral("Off");

    return clockText(m_settings.quietStartMinutes()) + QLatin1Char(' ')
        + QChar(kEnDash) + QLatin1Char(' ')
        + clockText(m_settings.quietEndMinutes());
}

void NotificationsFacade::setQuietHours(int startMinutes, int endMinutes)
{
    // The model owns clamping and the start-equals-end rule, so the facade does
    // not repeat them and a clamped write stays silent exactly as it does there.
    m_settings.setQuietHours(startMinutes, endMinutes);
}

void NotificationsFacade::setSounds(const QStringList& ids, const QStringList& titles)
{
    QVector<telegram::SoundOption> options;
    options.reserve(ids.size());

    for (qsizetype i = 0; i < ids.size(); ++i) {
        telegram::SoundOption option;
        option.soundId = toInt64(ids.at(i));
        // A missing title is empty, not a guessed label; the model keeps the
        // entry so the id and its position still line up with the payload.
        option.title = titles.value(i);
        options.append(option);
    }

    // The model owns the normalization rules (drop non-positive ids, drop later
    // duplicates, clamp durations), so the facade does not repeat them.
    m_sounds.setOptions(std::move(options));
}

QStringList NotificationsFacade::soundTitles() const
{
    const QVector<telegram::SoundOption> options = m_sounds.options();

    QStringList titles;
    titles.reserve(options.size());

    for (const telegram::SoundOption& option : options)
        titles.append(option.title);

    return titles;
}

int NotificationsFacade::soundIndex() const
{
    const qint64 selected = m_sounds.selectedId();

    // 0 is the model's silent selection and is not a row. A selection the list
    // no longer stores is not a row either; the id is kept by the model, so a
    // refreshed list brings the same row back.
    if (selected == 0)
        return -1;

    const QVector<telegram::SoundOption> options = m_sounds.options();
    for (int index = 0; index < options.size(); ++index) {
        if (options.at(index).soundId == selected)
            return index;
    }

    return -1;
}

void NotificationsFacade::selectSound(int index)
{
    if (index < 0) {
        // The silent choice, like the model's own id 0.
        m_sounds.select(0);
        return;
    }

    const QVector<telegram::SoundOption> options = m_sounds.options();
    if (index >= options.size())
        return; // A stale row cannot select a sound that is not offered.

    m_sounds.select(options.at(index).soundId);
}

void NotificationsFacade::loadChat(const QString& chatId, bool muted, bool sound, bool preview)
{
    const QString text = chatId.trimmed();
    const qint64 parsed = toInt64(text);

    // Telegram never assigns chat 0, so neither 0 nor a malformed id names a
    // chat. Loading one would leave overrides on screen that cannot be saved,
    // so the facade clears them instead and keeps the setters inert.
    if (parsed == 0) {
        m_chatId.clear();
        m_chat.setChat(0, false, true, true);
        return;
    }

    m_chatId = text;
    m_chat.setChat(parsed, muted, sound, preview);
}

bool NotificationsFacade::chatMuted() const
{
    return m_chat.muted();
}

bool NotificationsFacade::chatSound() const
{
    return m_chat.sound();
}

bool NotificationsFacade::chatPreview() const
{
    return m_chat.preview();
}

void NotificationsFacade::setChatMuted(bool muted)
{
    m_chat.setMuted(muted);
}

void NotificationsFacade::setChatSound(bool on)
{
    m_chat.setSound(on);
}

void NotificationsFacade::setChatPreview(bool on)
{
    m_chat.setPreview(on);
}

void NotificationsFacade::saveChat()
{
    if (!m_chat.dirty())
        return;

    // Read the payload before marking saved: the signal describes the edit the
    // user made, and markSaved() only moves the baseline.
    const QString chatId = m_chatId;
    const bool muted = m_chat.muted();
    const bool sound = m_chat.sound();
    const bool preview = m_chat.preview();

    emit chatSettingsRequested(chatId, muted, sound, preview);

    // The request is on its way, so the same flags are now the saved ones; a
    // second click cannot send the same request again.
    m_chat.markSaved();
}

} // namespace beygram::ui
