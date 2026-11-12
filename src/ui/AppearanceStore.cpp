#include "AppearanceStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>
#include <utility>

namespace beygram::ui {
namespace {

constexpr int kVersion = 1;
constexpr int kMinWallpaperIntensity = 0;
constexpr int kMaxWallpaperIntensity = 45;
constexpr int kDefaultWallpaperIntensity = 24;
constexpr int kMinMessageTextSize = 12;
constexpr int kMaxMessageTextSize = 18;
constexpr int kDefaultMessageTextSize = 14;

// Stored names from Android `data/appearance/`. Keep them byte-identical: they
// are what the Android client writes and what the Windows theme engine parses.
const QStringList& themePresets()
{
    static const QStringList values{
        QStringLiteral("Warm"),
        QStringLiteral("Graphite"),
    };
    return values;
}

const QStringList& themeModes()
{
    static const QStringList values{
        QStringLiteral("FollowSystem"),
        QStringLiteral("Light"),
        QStringLiteral("Dark"),
        QStringLiteral("Amoled"),
    };
    return values;
}

// DesignStyle.kt: Default, Material2, Material3, LiquidGlass, Signature.
const QStringList& designStyles()
{
    static const QStringList values{
        QStringLiteral("Default"),
        QStringLiteral("Material2"),
        QStringLiteral("Material3"),
        QStringLiteral("LiquidGlass"),
        QStringLiteral("Signature"),
    };
    return values;
}

const QStringList& bubbleStyles()
{
    static const QStringList values{
        QStringLiteral("Rounded"),
        QStringLiteral("Soft"),
        QStringLiteral("Square"),
    };
    return values;
}

const QStringList& listDensities()
{
    static const QStringList values{
        QStringLiteral("Comfortable"),
        QStringLiteral("Compact"),
    };
    return values;
}

// `ChatWallpaper.None` plus every id in `WallpaperPresets.kt`.
const QStringList& wallpaperPresets()
{
    static const QStringList values{
        QStringLiteral("none"),
        QStringLiteral("lavender-flow"),
        QStringLiteral("arctic-blue"),
        QStringLiteral("mint-air"),
        QStringLiteral("sunset-peach"),
        QStringLiteral("soft-rose"),
        QStringLiteral("blue-notes"),
        QStringLiteral("graphite-wave"),
        QStringLiteral("forest-whisper"),
        QStringLiteral("deep-night"),
        QStringLiteral("violet-mesh"),
        QStringLiteral("ocean-current"),
        QStringLiteral("amber-glow"),
        QStringLiteral("sky-paper"),
        QStringLiteral("terracotta-dust"),
        QStringLiteral("silver-mist"),
        QStringLiteral("emerald-tide"),
    };
    return values;
}

QString normalized(const QStringList& allowed, const QString& value, const QString& fallback)
{
    return allowed.contains(value) ? value : fallback;
}

int clampedIntensity(int value)
{
    return std::clamp(value, kMinWallpaperIntensity, kMaxWallpaperIntensity);
}

int clampedTextSize(int value)
{
    return std::clamp(value, kMinMessageTextSize, kMaxMessageTextSize);
}

bool settingsEqual(const AppearanceSettings& left, const AppearanceSettings& right)
{
    return left.themePreset == right.themePreset
        && left.themeMode == right.themeMode
        && left.designStyle == right.designStyle
        && left.bubbleStyle == right.bubbleStyle
        && left.wallpaperPreset == right.wallpaperPreset
        && left.wallpaperIntensity == right.wallpaperIntensity
        && left.reduceMotion == right.reduceMotion
        && left.listDensity == right.listDensity
        && left.messageTextSize == right.messageTextSize;
}

/// Reads the file into a fresh settings value. Anything unreadable, unparsable
/// or out of range keeps the default for that single field.
AppearanceSettings readSettings(const QString& filePath)
{
    AppearanceSettings settings;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return settings;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return settings;

    const QJsonObject root = document.object();

    settings.themePreset = normalized(themePresets(),
                                      root.value(QStringLiteral("themePreset")).toString(),
                                      settings.themePreset);
    settings.themeMode = normalized(themeModes(),
                                    root.value(QStringLiteral("themeMode")).toString(),
                                    settings.themeMode);
    settings.designStyle = normalized(designStyles(),
                                      root.value(QStringLiteral("designStyle")).toString(),
                                      settings.designStyle);
    settings.bubbleStyle = normalized(bubbleStyles(),
                                      root.value(QStringLiteral("bubbleStyle")).toString(),
                                      settings.bubbleStyle);
    settings.wallpaperPreset = normalized(wallpaperPresets(),
                                          root.value(QStringLiteral("wallpaperPreset")).toString(),
                                          settings.wallpaperPreset);

    // toInt() already rejects non-numbers and non-integral values.
    settings.wallpaperIntensity = clampedIntensity(
        root.value(QStringLiteral("wallpaperIntensity")).toInt(kDefaultWallpaperIntensity));

    const QJsonValue reduceMotion = root.value(QStringLiteral("reduceMotion"));
    if (reduceMotion.isBool())
        settings.reduceMotion = reduceMotion.toBool();

    settings.listDensity = normalized(listDensities(),
                                      root.value(QStringLiteral("listDensity")).toString(),
                                      settings.listDensity);
    settings.messageTextSize = clampedTextSize(
        root.value(QStringLiteral("messageTextSize")).toInt(kDefaultMessageTextSize));

    return settings;
}

} // namespace

AppearanceStore::AppearanceStore(QString filePath, QObject* parent)
    : QObject(parent), m_filePath(std::move(filePath))
{
}

void AppearanceStore::load()
{
    const AppearanceSettings loaded = readSettings(m_filePath);
    if (settingsEqual(loaded, m_settings))
        return;
    m_settings = loaded;
    emit changed();
}

AppearanceSettings AppearanceStore::settings() const
{
    return m_settings;
}

QString AppearanceStore::filePath() const
{
    return m_filePath;
}

void AppearanceStore::setThemePreset(const QString& value)
{
    const QString next = normalized(themePresets(), value, QStringLiteral("Warm"));
    if (next == m_settings.themePreset)
        return;
    m_settings.themePreset = next;
    persist();
    emit changed();
}

void AppearanceStore::setThemeMode(const QString& value)
{
    const QString next = normalized(themeModes(), value, QStringLiteral("FollowSystem"));
    if (next == m_settings.themeMode)
        return;
    m_settings.themeMode = next;
    persist();
    emit changed();
}

void AppearanceStore::setDesignStyle(const QString& value)
{
    const QString next = normalized(designStyles(), value, QStringLiteral("Default"));
    if (next == m_settings.designStyle)
        return;
    m_settings.designStyle = next;
    persist();
    emit changed();
}

void AppearanceStore::setBubbleStyle(const QString& value)
{
    const QString next = normalized(bubbleStyles(), value, QStringLiteral("Rounded"));
    if (next == m_settings.bubbleStyle)
        return;
    m_settings.bubbleStyle = next;
    persist();
    emit changed();
}

void AppearanceStore::setWallpaperPreset(const QString& value)
{
    const QString next = normalized(wallpaperPresets(), value, QStringLiteral("none"));
    if (next == m_settings.wallpaperPreset)
        return;
    m_settings.wallpaperPreset = next;
    persist();
    emit changed();
}

void AppearanceStore::setWallpaperIntensity(int value)
{
    const int next = clampedIntensity(value);
    if (next == m_settings.wallpaperIntensity)
        return;
    m_settings.wallpaperIntensity = next;
    persist();
    emit changed();
}

void AppearanceStore::setReduceMotion(bool value)
{
    if (value == m_settings.reduceMotion)
        return;
    m_settings.reduceMotion = value;
    persist();
    emit changed();
}

void AppearanceStore::setListDensity(const QString& value)
{
    const QString next = normalized(listDensities(), value, QStringLiteral("Comfortable"));
    if (next == m_settings.listDensity)
        return;
    m_settings.listDensity = next;
    persist();
    emit changed();
}

void AppearanceStore::setMessageTextSize(int value)
{
    const int next = clampedTextSize(value);
    if (next == m_settings.messageTextSize)
        return;
    m_settings.messageTextSize = next;
    persist();
    emit changed();
}

void AppearanceStore::persist()
{
    if (m_filePath.isEmpty())
        return;

    const QDir parent = QFileInfo(m_filePath).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return;

    const QJsonObject root{
        {QStringLiteral("version"), kVersion},
        {QStringLiteral("themePreset"), m_settings.themePreset},
        {QStringLiteral("themeMode"), m_settings.themeMode},
        {QStringLiteral("designStyle"), m_settings.designStyle},
        {QStringLiteral("bubbleStyle"), m_settings.bubbleStyle},
        {QStringLiteral("wallpaperPreset"), m_settings.wallpaperPreset},
        {QStringLiteral("wallpaperIntensity"), m_settings.wallpaperIntensity},
        {QStringLiteral("reduceMotion"), m_settings.reduceMotion},
        {QStringLiteral("listDensity"), m_settings.listDensity},
        {QStringLiteral("messageTextSize"), m_settings.messageTextSize},
    };

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        file.cancelWriting();
        return;
    }
    file.commit();
}

} // namespace beygram::ui
