#include "GiftsFacade.h"

#include <QDate>
#include <QDateTime>
#include <QScopedValueRollback>
#include <QStringList>
#include <QTime>
#include <QVector>

#include <utility>

namespace beygram::ui {

namespace {

/// The Telegram star glyph (U+2B50), written as a code point so the source
/// stays ASCII and the glyph does not depend on the compiler's charset.
constexpr char16_t kStarGlyph = 0x2B50;

/// A stand-in peer id for history rows. GiftHistoryModel only checks whether
/// senderUserId/receiverUserId is non-zero to split sent from received; the
/// actual account id is not known to this facade.
constexpr qint64 kLocalUserId = 1;

/// The separator between the segments of a history row: a middle dot (U+00B7)
/// with one space on each side.
QString separator()
{
    return QStringLiteral(" ") + QChar(0x00B7) + QStringLiteral(" ");
}

/// "50" plus the star glyph, the price as the picker prints it.
QString starText(qint64 stars)
{
    return QString::number(stars) + QLatin1Char(' ') + QChar(kStarGlyph);
}

/// A decimal integer from the string lists, or 0 when the text is not a
/// number. The models treat 0 as "no value" for every id field this facade
/// parses, so a malformed entry is dropped instead of being guessed at.
qint64 toInt64(const QString& text)
{
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    return ok ? value : 0;
}

/// A star amount from the string lists. The value may arrive as a plain
/// number, with the star glyph copied from a price, or grouped with spaces;
/// the noise is removed first. A negative value is kept so the model can apply
/// its own clamp/drop rule instead of the facade hiding it.
qint64 toStarAmount(const QString& text)
{
    QString cleaned = text.trimmed();
    cleaned.remove(QChar(kStarGlyph));
    cleaned.remove(QLatin1Char(' '));
    cleaned.remove(QLatin1Char(','));
    return toInt64(cleaned);
}

/// A history date as seconds since the Unix epoch for ordering only. The
/// display text is kept verbatim; an ISO date or a common dotted date is
/// understood, anything else is undated (0). The conversion is local time, so
/// it is only ever compared with other rows parsed the same way.
qint64 toEpochSeconds(const QString& text)
{
    if (text.isEmpty())
        return 0;

    const QDate date = QDate::fromString(text, Qt::ISODate);
    if (date.isValid())
        return QDateTime(date, QTime(0, 0)).toSecsSinceEpoch();

    QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
    if (dateTime.isValid())
        return dateTime.toSecsSinceEpoch();

    static const QStringList formats{
        QStringLiteral("yyyy-MM-dd HH:mm"),
        QStringLiteral("dd.MM.yyyy HH:mm"),
        QStringLiteral("dd.MM.yyyy"),
    };

    for (const QString& format : formats) {
        dateTime = QDateTime::fromString(text, format);
        if (dateTime.isValid())
            return dateTime.toSecsSinceEpoch();
    }

    return 0;
}

/// One history row: direction, title, price and date joined by a middle dot.
/// Empty title or date segments are omitted rather than left dangling.
QString historyText(const QString& title, const QString& date, qint64 stars,
                    bool incoming)
{
    QStringList parts;
    parts.reserve(4);

    parts.append(incoming ? QStringLiteral("Received") : QStringLiteral("Sent"));

    if (!title.isEmpty())
        parts.append(title);

    parts.append(starText(stars));

    if (!date.isEmpty())
        parts.append(date);

    return parts.join(separator());
}

} // namespace

GiftsFacade::GiftsFacade(QObject* parent) : QObject(parent)
{
    // Each model already suppresses no-op changes. The catalog connection also
    // re-resolves the selection, because replacing the catalog can remove or
    // move the selected gift; that controller update is folded into the one
    // change notification the catalog emits.
    connect(&m_catalog, &telegram::GiftCatalogModel::changed, this, [this] {
        reconcileSelection();
        emit changed();
    });
    connect(&m_send, &telegram::GiftSendController::changed, this, [this] {
        if (!m_reconciling)
            emit changed();
    });
    connect(&m_history, &telegram::GiftHistoryModel::changed, this,
            &GiftsFacade::changed);
}

QStringList GiftsFacade::titles() const
{
    const QVector<telegram::GiftRow> gifts = m_catalog.gifts();

    QStringList result;
    result.reserve(gifts.size());

    for (const telegram::GiftRow& gift : gifts)
        result.append(gift.title);

    return result;
}

QStringList GiftsFacade::prices() const
{
    const QVector<telegram::GiftRow> gifts = m_catalog.gifts();

    QStringList result;
    result.reserve(gifts.size());

    for (const telegram::GiftRow& gift : gifts)
        result.append(starText(gift.starCount));

    return result;
}

void GiftsFacade::setCatalog(const QStringList& ids, const QStringList& titles,
                             const QStringList& stars)
{
    QVector<telegram::GiftRow> rows;
    rows.reserve(ids.size());

    for (qsizetype i = 0; i < ids.size(); ++i) {
        telegram::GiftRow row;
        row.giftId = toInt64(ids.at(i));
        row.title = i < titles.size() ? titles.at(i) : QString();
        row.starCount = i < stars.size() ? toStarAmount(stars.at(i)) : 0;
        rows.append(row);
    }

    // The model owns the normalization rules (drop, de-duplicate), so the
    // facade does not repeat them. Its changed() signal drives the selection
    // reconciliation above.
    m_catalog.setGifts(std::move(rows));
}

int GiftsFacade::selectedIndex() const
{
    return m_selectedIndex;
}

void GiftsFacade::select(int index)
{
    // -1 is the explicit "no selection" value; any other index must address a
    // stored gift, so an out-of-range one is ignored like a stale tab.
    if (index < -1 || index >= m_catalog.count())
        return;

    if (index == m_selectedIndex)
        return;

    if (index == -1) {
        m_selectedIndex = -1;
        m_selectedGiftId = 0;
        m_send.setGift(0, 0);
        return;
    }

    const QVector<telegram::GiftRow> gifts = m_catalog.gifts();
    const telegram::GiftRow row = gifts.at(index);
    m_selectedIndex = index;
    m_selectedGiftId = row.giftId;
    m_send.setGift(row.giftId, row.starCount);
}

QString GiftsFacade::message() const
{
    return m_send.message();
}

void GiftsFacade::setMessage(const QString& text)
{
    m_send.setMessage(text);
}

bool GiftsFacade::anonymous() const
{
    return m_send.anonymous();
}

void GiftsFacade::setAnonymous(bool anonymous)
{
    m_send.setAnonymous(anonymous);
}

bool GiftsFacade::canSend() const
{
    return m_send.valid();
}

void GiftsFacade::setRecipient(const QString& chatId, const QString& userId)
{
    m_send.setRecipient(toInt64(chatId), toInt64(userId));
}

bool GiftsFacade::send()
{
    if (!canSend())
        return false;

    emit sendRequested(QString::number(m_send.giftId()),
                       QString::number(m_send.chatId()),
                       QString::number(m_send.userId()),
                       m_send.message(),
                       m_send.anonymous());
    return true;
}

QStringList GiftsFacade::history() const
{
    const QVector<telegram::GiftHistoryRow> rows = m_history.rows();

    QStringList result;
    result.reserve(rows.size());

    for (const telegram::GiftHistoryRow& row : rows) {
        const auto stored = m_historyEntries.constFind(row.giftId);

        if (stored != m_historyEntries.cend()) {
            const HistoryEntry& entry = stored.value();
            result.append(historyText(entry.title, entry.date, row.starCount,
                                      entry.incoming));
            continue;
        }

        // Defensive fallback: the model can only hold rows this facade added,
        // but a row without its display fields still formats from the row.
        const QString date = row.date > 0
            ? QDateTime::fromSecsSinceEpoch(row.date)
                  .toUTC()
                  .toString(QStringLiteral("yyyy-MM-dd"))
            : QString();
        result.append(historyText(QString(), date, row.starCount,
                                  row.receiverUserId != 0));
    }

    return result;
}

void GiftsFacade::addHistory(const QString& title, const QString& date,
                             const QString& stars, bool incoming)
{
    // GiftHistoryModel identifies a row by a positive gift id, so the facade
    // assigns a synthetic one. The direction lives in the peer fields; the
    // title and display date are kept next to the row because the model has no
    // field for them.
    const qint64 giftId = m_nextHistoryId++;

    HistoryEntry entry;
    entry.title = title.trimmed();
    entry.date = date.trimmed();
    entry.incoming = incoming;
    m_historyEntries.insert(giftId, entry);

    telegram::GiftHistoryRow row;
    row.giftId = giftId;
    row.date = toEpochSeconds(entry.date);
    row.starCount = toStarAmount(stars);

    if (incoming)
        row.receiverUserId = kLocalUserId;
    else
        row.senderUserId = kLocalUserId;

    m_history.append(QVector<telegram::GiftHistoryRow>{row});
}

void GiftsFacade::reset()
{
    m_selectedIndex = -1;
    m_selectedGiftId = 0;
    m_send.reset();
}

void GiftsFacade::reconcileSelection()
{
    // The send controller's update is part of this catalog change, not a
    // second one, so its changed() signal is suppressed while it happens.
    QScopedValueRollback<bool> guard(m_reconciling, true);

    const QVector<telegram::GiftRow> gifts = m_catalog.gifts();

    int index = -1;
    if (m_selectedGiftId > 0) {
        for (qsizetype i = 0; i < gifts.size(); ++i) {
            if (gifts.at(i).giftId == m_selectedGiftId) {
                index = static_cast<int>(i);
                break;
            }
        }
    }

    m_selectedIndex = index;

    if (index >= 0) {
        const telegram::GiftRow row = gifts.at(index);
        m_send.setGift(row.giftId, row.starCount);
    } else {
        m_selectedGiftId = 0;
        m_send.setGift(0, 0);
    }
}

} // namespace beygram::ui
