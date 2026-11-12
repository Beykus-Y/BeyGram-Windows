#include "PremiumFacade.h"

#include <QDateTime>

#include <utility>

namespace beygram::ui {

namespace {

/// Builds one display row per title/description pair.
///
/// The lists are read positionally and may differ in length: a title without a
/// description renders as the title alone, and a pair without a title is
/// skipped because there is no label to put the description under. Trimming
/// both sides keeps a padded payload from producing a row with a dangling
/// separator or a blank line.
QStringList buildCatalog(const QStringList& titles, const QStringList& descriptions)
{
    QStringList rows;
    rows.reserve(titles.size());

    for (qsizetype i = 0; i < titles.size(); ++i) {
        const QString title = titles.at(i).trimmed();
        if (title.isEmpty())
            continue;

        const QString description =
            i < descriptions.size() ? descriptions.at(i).trimmed() : QString();

        rows.append(description.isEmpty()
                        ? title
                        : QStringLiteral("%1 — %2").arg(title, description));
    }

    return rows;
}

} // namespace

PremiumFacade::PremiumFacade(QObject* parent) : QObject(parent)
{
    // The model suppresses no-op updates itself, but its signal is still the
    // only path through which a status change can reach QML.
    connect(&m_status, &telegram::PremiumStatusModel::changed,
            this, &PremiumFacade::syncChanged);

    // Seed the diff so the first real change is reported, while the empty
    // state the card starts in stays silent.
    m_visible = visibleState();
}

bool PremiumFacade::premium() const
{
    return m_status.premium();
}

QString PremiumFacade::badge() const
{
    return m_status.badgeText();
}

QString PremiumFacade::expiryText() const
{
    if (!m_status.premium())
        return QString();

    // 0 is "no known expiry" (a lifetime subscription, or a status TDLib sent
    // without one), never the Unix epoch.
    if (m_status.expiresAt() <= 0)
        return QStringLiteral("No expiry");

    // daysLeft() rounds up, so any part of a day still counts as a whole day
    // left, and an elapsed expiry reports 0 rather than a negative count. The
    // card cannot tell "ends in a few hours" from "ended a minute ago" here;
    // the caller refreshes the status and stores the new flag for that.
    const int days = m_status.daysLeft(QDateTime::currentSecsSinceEpoch());
    if (days <= 0)
        return QStringLiteral("Expires today");

    return QStringLiteral("%1 days left").arg(days);
}

QStringList PremiumFacade::features() const
{
    // The catalog is the human-readable rendering of the same list, so it wins
    // while it is set; the keys are the fallback that keeps the card populated
    // before the titles arrive.
    return m_catalog.isEmpty() ? m_status.features() : m_catalog;
}

bool PremiumFacade::loading() const
{
    return m_loading;
}

void PremiumFacade::setStatus(bool premium, qint64 expiresAt)
{
    // The model emits changed() only for a real difference, and syncChanged()
    // turns that into this facade's signal.
    m_status.setPremium(premium, expiresAt);
    syncChanged();
}

void PremiumFacade::setFeatures(const QStringList& features)
{
    // Drop the old display text first: otherwise a model change would be
    // announced while the previous catalog still shadowed the new keys, and the
    // card would repaint twice for one call.
    m_catalog.clear();
    m_status.setFeatures(features);
    syncChanged();
}

void PremiumFacade::setLoading(bool loading)
{
    if (m_loading == loading)
        return;

    m_loading = loading;
    syncChanged();
}

void PremiumFacade::setFeatureCatalog(const QStringList& titles,
                                      const QStringList& descriptions)
{
    QStringList rows = buildCatalog(titles, descriptions);
    if (rows == m_catalog)
        return;

    m_catalog = std::move(rows);
    syncChanged();
}

void PremiumFacade::clear()
{
    // Clear the facade-local state before the model so the model's own signal
    // already sees the fully cleared snapshot and the card repaints once.
    m_catalog.clear();
    m_loading = false;
    m_status.clear();
    syncChanged();
}

void PremiumFacade::upgrade()
{
    emit upgradeRequested();
}

PremiumFacade::VisibleState PremiumFacade::visibleState() const
{
    return VisibleState{m_status.premium(), m_status.expiresAt(), features(), m_loading};
}

void PremiumFacade::syncChanged()
{
    VisibleState next = visibleState();
    if (next == m_visible)
        return;

    m_visible = std::move(next);
    emit changed();
}

} // namespace beygram::ui
