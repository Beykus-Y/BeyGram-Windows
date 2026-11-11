#include "NotificationSettingsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>

#include <utility>

namespace beygram::ui {
namespace {

constexpr int kVersion = 1;

bool settingsEqual(const NotificationSettings& left, const NotificationSettings& right)
{
    return left.enabled == right.enabled
        && left.showPreview == right.showPreview
        && left.sound == right.sound
        && left.notifyMutedChats == right.notifyMutedChats;
}

/// Assigns `target` from `root[key]` only when the stored value is a real JSON
/// boolean. Strings such as `"true"` and numbers such as `1` are rejected, so a
/// damaged file keeps the default instead of silently flipping a switch.
void readBool(const QJsonObject& root, const QString& key, bool& target)
{
    const QJsonValue value = root.value(key);
    if (value.isBool())
        target = value.toBool();
}

/// Reads the file into a fresh settings value. Anything unreadable, unparsable
/// or wrongly typed keeps the default for that single field.
NotificationSettings readSettings(const QString& filePath)
{
    NotificationSettings settings;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return settings;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return settings;

    const QJsonObject root = document.object();

    readBool(root, QStringLiteral("enabled"), settings.enabled);
    readBool(root, QStringLiteral("showPreview"), settings.showPreview);
    readBool(root, QStringLiteral("sound"), settings.sound);
    readBool(root, QStringLiteral("notifyMutedChats"), settings.notifyMutedChats);

    return settings;
}

} // namespace

NotificationSettingsStore::NotificationSettingsStore(QString filePath, QObject* parent)
    : QObject(parent), m_filePath(std::move(filePath))
{
}

void NotificationSettingsStore::load()
{
    const NotificationSettings loaded = readSettings(m_filePath);
    if (settingsEqual(loaded, m_settings))
        return;
    m_settings = loaded;
    emit changed();
}

NotificationSettings NotificationSettingsStore::settings() const
{
    return m_settings;
}

QString NotificationSettingsStore::filePath() const
{
    return m_filePath;
}

void NotificationSettingsStore::setEnabled(bool value)
{
    if (value == m_settings.enabled)
        return;
    m_settings.enabled = value;
    persist();
    emit changed();
}

void NotificationSettingsStore::setShowPreview(bool value)
{
    if (value == m_settings.showPreview)
        return;
    m_settings.showPreview = value;
    persist();
    emit changed();
}

void NotificationSettingsStore::setSound(bool value)
{
    if (value == m_settings.sound)
        return;
    m_settings.sound = value;
    persist();
    emit changed();
}

void NotificationSettingsStore::setNotifyMutedChats(bool value)
{
    if (value == m_settings.notifyMutedChats)
        return;
    m_settings.notifyMutedChats = value;
    persist();
    emit changed();
}

void NotificationSettingsStore::persist()
{
    if (m_filePath.isEmpty())
        return;

    const QDir parent = QFileInfo(m_filePath).absoluteDir();
    if (!parent.exists() && !QDir().mkpath(parent.absolutePath()))
        return;

    const QJsonObject root{
        {QStringLiteral("version"), kVersion},
        {QStringLiteral("enabled"), m_settings.enabled},
        {QStringLiteral("showPreview"), m_settings.showPreview},
        {QStringLiteral("sound"), m_settings.sound},
        {QStringLiteral("notifyMutedChats"), m_settings.notifyMutedChats},
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
