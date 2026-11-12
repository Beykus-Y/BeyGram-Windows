#include "AppearanceFacade.h"

#include <QVector>

#include <utility>

namespace beygram::ui {

namespace {

constexpr int kDefaultPercentTranslated = 100;

QString kindToString(telegram::BackgroundKind kind)
{
    switch (kind) {
    case telegram::BackgroundKind::None:
        return QStringLiteral("none");
    case telegram::BackgroundKind::Solid:
        return QStringLiteral("solid");
    case telegram::BackgroundKind::Gradient:
        return QStringLiteral("gradient");
    case telegram::BackgroundKind::Pattern:
        return QStringLiteral("pattern");
    case telegram::BackgroundKind::Wallpaper:
        return QStringLiteral("wallpaper");
    }
    return QStringLiteral("none");
}

/// Parses the string QML binds to. Unknown spellings yield false so the caller
/// can ignore them instead of silently falling back to "none", which would look
/// like the user cleared the background.
bool kindFromString(const QString& text, telegram::BackgroundKind* kind)
{
    const QString normalized = text.trimmed().toLower();
    if (normalized == QLatin1String("none")) {
        *kind = telegram::BackgroundKind::None;
        return true;
    }
    if (normalized == QLatin1String("solid")) {
        *kind = telegram::BackgroundKind::Solid;
        return true;
    }
    if (normalized == QLatin1String("gradient")) {
        *kind = telegram::BackgroundKind::Gradient;
        return true;
    }
    if (normalized == QLatin1String("pattern")) {
        *kind = telegram::BackgroundKind::Pattern;
        return true;
    }
    if (normalized == QLatin1String("wallpaper")) {
        *kind = telegram::BackgroundKind::Wallpaper;
        return true;
    }
    return false;
}

} // namespace

AppearanceFacade::AppearanceFacade(QObject* parent)
    : QObject(parent)
{
    connect(&m_accessibility, &telegram::AccessibilityModel::changed,
            this, &AppearanceFacade::changed);
    connect(&m_background, &telegram::ChatBackgroundController::changed,
            this, &AppearanceFacade::changed);
    connect(&m_languages, &telegram::LanguagePackModel::changed,
            this, &AppearanceFacade::changed);
}

bool AppearanceFacade::reduceMotion() const
{
    return m_accessibility.reduceMotion();
}

void AppearanceFacade::setReduceMotion(bool on)
{
    m_accessibility.setReduceMotion(on);
}

bool AppearanceFacade::highContrast() const
{
    return m_accessibility.highContrast();
}

void AppearanceFacade::setHighContrast(bool on)
{
    m_accessibility.setHighContrast(on);
}

bool AppearanceFacade::largeText() const
{
    return m_accessibility.largeText();
}

void AppearanceFacade::setLargeText(bool on)
{
    m_accessibility.setLargeText(on);
}

int AppearanceFacade::textScale() const
{
    return m_accessibility.textScale();
}

void AppearanceFacade::setTextScale(int percent)
{
    m_accessibility.setTextScale(percent);
}

QString AppearanceFacade::backgroundKind() const
{
    return kindToString(m_background.kind());
}

void AppearanceFacade::setBackgroundKind(const QString& kind)
{
    telegram::BackgroundKind parsed = telegram::BackgroundKind::None;
    if (!kindFromString(kind, &parsed))
        return;

    const QString previousKind = backgroundKind();
    const QString previousValue = backgroundValue();

    // None drops the value; every other kind keeps the stored one and is
    // dropped by the controller while that value is still blank.
    m_background.setBackground(parsed, parsed == telegram::BackgroundKind::None
                                               ? QString()
                                               : m_background.value());

    if (backgroundKind() != previousKind || backgroundValue() != previousValue)
        emit backgroundChanged(backgroundKind(), backgroundValue());
}

QString AppearanceFacade::backgroundValue() const
{
    return m_background.value();
}

void AppearanceFacade::setBackgroundValue(const QString& value)
{
    const QString trimmed = value.trimmed();
    // A background without a value cannot be painted, so a blank write never
    // replaces the stored value. Selecting None is the way to clear it.
    if (trimmed.isEmpty())
        return;

    const QString previousKind = backgroundKind();
    const QString previousValue = backgroundValue();

    // A value with no kind yet describes a solid background, so QML can fill the
    // value field before the kind field and still end up with a renderable
    // background.
    const telegram::BackgroundKind kind =
            m_background.kind() == telegram::BackgroundKind::None
            ? telegram::BackgroundKind::Solid
            : m_background.kind();
    m_background.setBackground(kind, trimmed);

    if (backgroundKind() != previousKind || backgroundValue() != previousValue)
        emit backgroundChanged(backgroundKind(), backgroundValue());
}

int AppearanceFacade::dimmer() const
{
    return m_background.dimmer();
}

void AppearanceFacade::setDimmer(int percent)
{
    m_background.setDimmer(percent);
}

int AppearanceFacade::blur() const
{
    return m_background.blur();
}

void AppearanceFacade::setBlur(int percent)
{
    m_background.setBlur(percent);
}

void AppearanceFacade::setLanguages(const QStringList& codes, const QStringList& names)
{
    QVector<telegram::LanguageRow> rows;
    rows.reserve(codes.size());

    for (int i = 0; i < codes.size(); ++i) {
        const QString code = codes.at(i).trimmed();
        if (code.isEmpty())
            continue; // the model drops these too; skip them before the lookup

        const QString name = i < names.size() ? names.at(i).trimmed() : QString();

        telegram::LanguageRow row;
        row.code = code;
        row.name = name.isEmpty() ? code : name;
        row.nativeName = row.name;
        row.beta = false;
        // The caller offers packs it already knows; a missing percentage means
        // the pack is complete rather than unknown.
        row.percentTranslated = kDefaultPercentTranslated;
        rows.append(row);
    }

    m_languages.setLanguages(std::move(rows));
}

QStringList AppearanceFacade::languageNames() const
{
    const QVector<telegram::LanguageRow> rows = m_languages.languages();
    QStringList names;
    names.reserve(rows.size());
    for (const telegram::LanguageRow& row : rows)
        names.append(row.name);
    return names;
}

int AppearanceFacade::languageIndex() const
{
    const QString selected = m_languages.selectedCode();
    if (selected.isEmpty())
        return -1;

    const QVector<telegram::LanguageRow> rows = m_languages.languages();
    for (int i = 0; i < rows.size(); ++i) {
        if (rows.at(i).code == selected)
            return i;
    }
    return -1;
}

void AppearanceFacade::selectLanguage(int index)
{
    if (index < 0) {
        // A negative index means "no language", which the model spells as an
        // empty code.
        m_languages.select(QString());
        return;
    }

    const QVector<telegram::LanguageRow> rows = m_languages.languages();
    if (index >= rows.size())
        return;

    const QString previous = m_languages.selectedCode();
    m_languages.select(rows.at(index).code);

    const QString selected = m_languages.selectedCode();
    if (selected != previous && !selected.isEmpty())
        emit languageSelected(selected);
}

void AppearanceFacade::reset()
{
    const QString previousKind = backgroundKind();
    const QString previousValue = backgroundValue();

    m_accessibility.reset();
    m_background.reset();
    // The offered packs describe what the client can offer, not a user choice,
    // so reset() clears the selection but keeps the list.
    m_languages.select(QString());

    if (backgroundKind() != previousKind || backgroundValue() != previousValue)
        emit backgroundChanged(backgroundKind(), backgroundValue());
}

} // namespace beygram::ui
