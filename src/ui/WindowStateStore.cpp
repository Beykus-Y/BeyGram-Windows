#include "WindowStateStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace beygram::ui {
namespace {

constexpr int kVersion = 1;
constexpr int kMinSize = 640;
constexpr int kMaxSize = 10000;

int clampedSize(int value)
{
    return std::clamp(value, kMinSize, kMaxSize);
}

WindowState sanitized(WindowState state)
{
    state.width = clampedSize(state.width);
    state.height = clampedSize(state.height);
    return state;
}

bool statesEqual(const WindowState& left, const WindowState& right)
{
    return left.x == right.x
        && left.y == right.y
        && left.width == right.width
        && left.height == right.height
        && left.maximized == right.maximized;
}

/// Reads one integral JSON number, or `fallback` when the value is absent or not
/// a whole number. Non-numeric values and fractional doubles count as damaged,
/// so they keep the default instead of being truncated.
int readInt(const QJsonObject& root, const QString& key, int fallback)
{
    const QJsonValue value = root.value(key);
    if (!value.isDouble())
        return fallback;

    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number)
        return fallback;
    if (number < static_cast<double>(std::numeric_limits<int>::min())
        || number > static_cast<double>(std::numeric_limits<int>::max())) {
        return fallback;
    }

    return static_cast<int>(number);
}

/// Reads the file into a fresh geometry value. Anything unreadable, unparsable
/// or out of range keeps the default for that single field.
WindowState readState(const QString& filePath)
{
    WindowState state;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return state;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return state;

    const QJsonObject root = document.object();

    // Positions are kept as given; multi-monitor desktops need negative values.
    state.x = readInt(root, QStringLiteral("x"), state.x);
    state.y = readInt(root, QStringLiteral("y"), state.y);

    // A missing or non-numeric size keeps the default; a numeric one is clamped,
    // so a zero-sized or absurd window still opens at a usable geometry.
    state.width = clampedSize(readInt(root, QStringLiteral("width"), state.width));
    state.height = clampedSize(readInt(root, QStringLiteral("height"), state.height));

    const QJsonValue maximized = root.value(QStringLiteral("maximized"));
    if (maximized.isBool())
        state.maximized = maximized.toBool();

    return state;
}

} // namespace

WindowStateStore::WindowStateStore(QString filePath, QObject* parent)
    : QObject(parent), m_filePath(std::move(filePath))
{
}

void WindowStateStore::load()
{
    const WindowState loaded = readState(m_filePath);
    if (statesEqual(loaded, m_state))
        return;
    m_state = loaded;
    emit changed();
}

WindowState WindowStateStore::state() const
{
    return m_state;
}

QString WindowStateStore::filePath() const
{
    return m_filePath;
}

void WindowStateStore::save(const WindowState& state)
{
    const WindowState next = sanitized(state);
    if (statesEqual(next, m_state))
        return;
    m_state = next;
    persist();
    emit changed();
}

void WindowStateStore::persist()
{
    if (m_filePath.isEmpty())
        return;

    const QDir parent = QFileInfo(m_filePath).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return;

    const QJsonObject root{
        {QStringLiteral("version"), kVersion},
        {QStringLiteral("x"), m_state.x},
        {QStringLiteral("y"), m_state.y},
        {QStringLiteral("width"), m_state.width},
        {QStringLiteral("height"), m_state.height},
        {QStringLiteral("maximized"), m_state.maximized},
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
