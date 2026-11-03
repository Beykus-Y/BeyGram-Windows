#include "ThemeController.h"

#include "core/ColorContrast.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace beygram::ui {
namespace {

struct Palette {
    QColor bg;
    QColor surface;
    QColor surface2;
    QColor line;
    QColor text;
    QColor muted;
    QColor accent;
    QColor accentSoft;
    QColor accentInk;
    QColor onAccent;
    QColor bubbleOut;
    QColor ok;
    QColor okInk;
    QColor warn;
    QColor warnInk;
    QColor danger;
};

Palette warmLight()
{
    return {
        QColor(0xFFFAF8F5), QColor(0xFFFFFFFF), QColor(0xFFF3EFEA), QColor(0xFFECE7E0),
        QColor(0xFF1E1B18), QColor(0xFF67625B),
        QColor(0xFF6A4FDC), QColor(0xFFEEE9FE), QColor(0xFF4B33B5), QColor(0xFFFFFFFF),
        QColor(0xFFE6DEFD),
        QColor(0xFFDDF5EC), QColor(0xFF0F6B57), QColor(0xFFFFF1D6), QColor(0xFF7A4B00),
        QColor(0xFFB42318),
    };
}

Palette warmDark()
{
    return {
        QColor(0xFF161417), QColor(0xFF1F1C21), QColor(0xFF2A262C), QColor(0xFF2E2A30),
        QColor(0xFFF3EFEA), QColor(0xFFA39C95),
        QColor(0xFFA894FF), QColor(0xFF2D2750), QColor(0xFFCFC4FF), QColor(0xFF16131F),
        QColor(0xFF2E2952),
        QColor(0xFF15332B), QColor(0xFF8BE0CF), QColor(0xFF3A2D16), QColor(0xFFFFD28A),
        QColor(0xFFF97066),
    };
}

Palette graphiteLight()
{
    return {
        QColor(0xFFF5F6F8), QColor(0xFFFFFFFF), QColor(0xFFECEEF2), QColor(0xFFDFE2E8),
        QColor(0xFF1B1D22), QColor(0xFF596170),
        QColor(0xFF3B5BDB), QColor(0xFFE3E9FB), QColor(0xFF2B45A8), QColor(0xFFFFFFFF),
        QColor(0xFFDCE4F8),
        QColor(0xFFDDF3EA), QColor(0xFF0F6B57), QColor(0xFFFFF1D6), QColor(0xFF7A4B00),
        QColor(0xFFB42318),
    };
}

Palette graphiteDark()
{
    return {
        QColor(0xFF16181C), QColor(0xFF1F2227), QColor(0xFF2A2E35), QColor(0xFF2C3038),
        QColor(0xFFEEF0F3), QColor(0xFF9AA1AC),
        QColor(0xFF8FA6FF), QColor(0xFF26305A), QColor(0xFFC5D1FF), QColor(0xFF111527),
        QColor(0xFF2A3360),
        QColor(0xFF15332B), QColor(0xFF8BE0CF), QColor(0xFF3A2D16), QColor(0xFFFFD28A),
        QColor(0xFFF97066),
    };
}

Palette amoled(Palette palette)
{
    palette.bg = QColor(0xFF000000);
    palette.surface = QColor(0xFF121214);
    palette.surface2 = QColor(0xFF1C1C20);
    palette.line = QColor(0xFF26262B);
    return palette;
}

} // namespace

ThemeController::ThemeController(QObject* parent) : QObject(parent)
{
    if (QGuiApplication::instance() != nullptr) {
        m_styleHints = QGuiApplication::styleHints();
        if (m_styleHints) {
            connect(m_styleHints, &QStyleHints::colorSchemeChanged, this,
                    [this] { apply(); });
        }
    }
    apply();
}

ThemeController::~ThemeController() = default;

bool ThemeController::systemIsDark() const
{
    if (!m_styleHints)
        return false;
    return m_styleHints->colorScheme() == Qt::ColorScheme::Dark;
}

void ThemeController::apply()
{
    const bool dark = m_mode == Mode::Dark || m_mode == Mode::Amoled
        || (m_mode == Mode::FollowSystem && systemIsDark());

    Palette palette;
    switch (m_preset) {
    case Preset::Warm:
        palette = dark ? warmDark() : warmLight();
        break;
    case Preset::Graphite:
        palette = dark ? graphiteDark() : graphiteLight();
        break;
    }
    if (m_mode == Mode::Amoled)
        palette = amoled(palette);

    m_dark = dark;
    m_bg = palette.bg;
    m_surface = palette.surface;
    m_surface2 = palette.surface2;
    m_line = palette.line;
    m_text = palette.text;
    m_muted = palette.muted;
    m_accent = palette.accent;
    m_accentSoft = palette.accentSoft;
    m_accentInk = palette.accentInk;
    m_onAccent = palette.onAccent;
    m_bubbleOut = palette.bubbleOut;
    m_ok = palette.ok;
    m_okInk = palette.okInk;
    m_warn = palette.warn;
    m_warnInk = palette.warnInk;
    m_danger = palette.danger;

    // Keep the Theme Engine invariant: message text must read on both bubbles.
    if (!color::isReadableText(m_text, m_surface))
        m_text = color::readableOn(m_surface, m_text, dark ? QColor(Qt::white) : QColor(Qt::black));
    if (!color::isReadableText(m_text, m_bubbleOut))
        m_text = color::readableOn(m_bubbleOut, m_text, dark ? QColor(Qt::white) : QColor(Qt::black));

    emit themeChanged();
}

QString ThemeController::modeName() const
{
    switch (m_mode) {
    case Mode::FollowSystem: return QStringLiteral("FollowSystem");
    case Mode::Light: return QStringLiteral("Light");
    case Mode::Dark: return QStringLiteral("Dark");
    case Mode::Amoled: return QStringLiteral("Amoled");
    }
    return QStringLiteral("FollowSystem");
}

void ThemeController::setModeName(const QString& name)
{
    if (name == QLatin1String("Light"))
        m_mode = Mode::Light;
    else if (name == QLatin1String("Dark"))
        m_mode = Mode::Dark;
    else if (name == QLatin1String("Amoled"))
        m_mode = Mode::Amoled;
    else
        m_mode = Mode::FollowSystem;
    apply();
}

QString ThemeController::presetName() const
{
    return m_preset == Preset::Warm ? QStringLiteral("Warm") : QStringLiteral("Graphite");
}

void ThemeController::setPresetName(const QString& name)
{
    m_preset = name == QLatin1String("Graphite") ? Preset::Graphite : Preset::Warm;
    apply();
}

void ThemeController::toggleDarkMode()
{
    // A manual toggle leaves FollowSystem and becomes explicit, matching the
    // Android editor: the user's choice must win over the system setting.
    setModeName(m_dark ? QStringLiteral("Light") : QStringLiteral("Dark"));
}

} // namespace beygram::ui
