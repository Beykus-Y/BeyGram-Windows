#include "ui/NotificationScopesFacade.h"

#include <QLatin1String>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <utility>

namespace beygram::ui {
namespace {

/// A scope id from the string list. Negative ids name no scope, and text that
/// is not an integer names nothing either, so both are rejected exactly as
/// notificationScopesFromResponse rejects them.
bool parseScopeId(const QString& text, qint64* id)
{
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    if (!ok || value < 0)
        return false;

    *id = value;
    return true;
}

/// A flag from the string lists. Only an explicit yes or no counts; an absent
/// or unrecognised value keeps the NotificationScope default instead of
/// silently muting a scope or turning its sound off.
bool parseFlag(const QString& text, bool fallback)
{
    const QString token = text.trimmed().toLower();

    if (token == QLatin1String("true") || token == QLatin1String("1")
        || token == QLatin1String("yes") || token == QLatin1String("on")) {
        return true;
    }
    if (token == QLatin1String("false") || token == QLatin1String("0")
        || token == QLatin1String("no") || token == QLatin1String("off")) {
        return false;
    }

    return fallback;
}

/// Same rows, same order, same stored flags. The struct has no comparison of
/// its own, and this keeps the facade's no-op rule explicit.
bool sameScopes(const QVector<telegram::NotificationScope>& left,
                const QVector<telegram::NotificationScope>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype index = 0; index < left.size(); ++index) {
        const telegram::NotificationScope& a = left.at(index);
        const telegram::NotificationScope& b = right.at(index);
        if (a.scopeId != b.scopeId || a.title != b.title || a.muted != b.muted
            || a.showPreview != b.showPreview || a.sound != b.sound) {
            return false;
        }
    }

    return true;
}

} // namespace

NotificationScopesFacade::NotificationScopesFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList NotificationScopesFacade::titles() const
{
    QStringList labels;
    labels.reserve(m_scopes.size());

    for (const telegram::NotificationScope& scope : m_scopes)
        labels.append(scope.title);

    return labels;
}

QStringList NotificationScopesFacade::ids() const
{
    QStringList values;
    values.reserve(m_scopes.size());

    for (const telegram::NotificationScope& scope : m_scopes)
        values.append(QString::number(scope.scopeId));

    return values;
}

int NotificationScopesFacade::selectedIndex() const
{
    return m_selectedIndex;
}

bool NotificationScopesFacade::muted() const
{
    return m_draft.muted;
}

bool NotificationScopesFacade::preview() const
{
    return m_draft.preview;
}

bool NotificationScopesFacade::sound() const
{
    return m_draft.sound;
}

void NotificationScopesFacade::setScopes(const QStringList& ids, const QStringList& titles,
                                         const QStringList& muted, const QStringList& preview,
                                         const QStringList& sound)
{
    QVector<telegram::NotificationScope> next;
    next.reserve(ids.size());

    QSet<qint64> seen;
    for (qsizetype index = 0; index < ids.size(); ++index) {
        qint64 id = 0;
        if (!parseScopeId(ids.at(index), &id) || seen.contains(id))
            continue;
        seen.insert(id);

        telegram::NotificationScope scope;
        scope.scopeId = id;
        // The lists are read positionally, so a title or flag always belongs
        // to the id at the same index. A shorter list reads as absent and
        // keeps the struct default.
        scope.title = titles.value(index).trimmed();
        scope.muted = parseFlag(muted.value(index), false);
        scope.showPreview = parseFlag(preview.value(index), true);
        scope.sound = parseFlag(sound.value(index), true);
        next.append(scope);
    }

    const bool rowsChanged = !sameScopes(next, m_scopes);
    const int previousIndex = m_selectedIndex;
    const Draft previousDraft = m_draft;

    m_scopes = std::move(next);

    // Keep a selection the new list still has. A selection the list can no
    // longer address falls back to the first row, or to no row when empty.
    if (m_selectedIndex < 0 || m_selectedIndex >= m_scopes.size())
        m_selectedIndex = m_scopes.isEmpty() ? -1 : 0;

    // A reload is authoritative: it replaces the stored flags and therefore
    // the draft, even when the user had an unsaved edit on this row.
    m_draft = draftFor(m_selectedIndex);

    if (rowsChanged || previousIndex != m_selectedIndex || !(previousDraft == m_draft))
        emit changed();
}

void NotificationScopesFacade::select(int index)
{
    if (index < -1 || index >= m_scopes.size())
        return;
    if (index == m_selectedIndex)
        return;

    m_selectedIndex = index;
    m_draft = draftFor(index);
    emit changed();
}

void NotificationScopesFacade::setMuted(bool muted)
{
    if (m_selectedIndex < 0 || m_draft.muted == muted)
        return;

    m_draft.muted = muted;
    emit changed();
}

void NotificationScopesFacade::setPreview(bool preview)
{
    if (m_selectedIndex < 0 || m_draft.preview == preview)
        return;

    m_draft.preview = preview;
    emit changed();
}

void NotificationScopesFacade::setSound(bool sound)
{
    if (m_selectedIndex < 0 || m_draft.sound == sound)
        return;

    m_draft.sound = sound;
    emit changed();
}

bool NotificationScopesFacade::save()
{
    if (m_selectedIndex < 0 || m_selectedIndex >= m_scopes.size())
        return false;
    if (draftMatches(m_selectedIndex))
        return false;

    telegram::NotificationScope& scope = m_scopes[m_selectedIndex];
    const QString scopeId = QString::number(scope.scopeId);
    const bool muted = m_draft.muted;
    const bool preview = m_draft.preview;
    const bool sound = m_draft.sound;

    // The draft becomes the stored baseline before the signal: a handler that
    // saves again synchronously must not announce the same edit twice.
    scope.muted = muted;
    scope.showPreview = preview;
    scope.sound = sound;

    // The exposed draft does not move, so the save is announced without a
    // changed(): only the hidden baseline moved, and a second click is already
    // guarded by the comparison above.
    emit saveRequested(scopeId, muted, preview, sound);
    return true;
}

NotificationScopesFacade::Draft NotificationScopesFacade::draftFor(int index) const
{
    if (index < 0 || index >= m_scopes.size())
        return {};

    const telegram::NotificationScope& scope = m_scopes.at(index);
    return Draft{scope.muted, scope.showPreview, scope.sound};
}

bool NotificationScopesFacade::draftMatches(int index) const
{
    if (index < 0 || index >= m_scopes.size())
        return false;

    const telegram::NotificationScope& scope = m_scopes.at(index);
    return scope.muted == m_draft.muted && scope.showPreview == m_draft.preview
        && scope.sound == m_draft.sound;
}

} // namespace beygram::ui
