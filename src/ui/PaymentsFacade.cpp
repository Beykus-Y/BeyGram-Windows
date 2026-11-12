#include "PaymentsFacade.h"

#include <QVector>

#include <utility>

namespace beygram::ui {
namespace {

// QML hands ids over as strings while TDLib and the checkout model use int53.
// Text that is not an integer becomes 0, which the model treats as "not an
// invoice" and clears itself, so a malformed id can never leave a stale invoice
// on screen.
qint64 parseId(const QString& text)
{
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    return ok ? value : 0;
}

} // namespace

PaymentsFacade::PaymentsFacade(QObject* parent) : QObject(parent)
{
    connect(&m_checkout, &telegram::PaymentCheckoutModel::changed, this,
            &PaymentsFacade::handleModelChanged);
    connect(&m_methods, &telegram::PaymentMethodModel::changed, this,
            &PaymentsFacade::handleModelChanged);
}

void PaymentsFacade::setInvoice(const QString& chatId, const QString& messageId,
                                const QString& title, qint64 totalAmount,
                                const QString& currency)
{
    m_checkout.setInvoice(parseId(chatId), parseId(messageId), title, totalAmount,
                          currency);
}

bool PaymentsFacade::active() const
{
    return m_checkout.active();
}

QString PaymentsFacade::title() const
{
    return m_checkout.title();
}

QString PaymentsFacade::totalText() const
{
    return m_checkout.totalText();
}

QString PaymentsFacade::name() const
{
    return m_checkout.name();
}

void PaymentsFacade::setName(const QString& name)
{
    m_checkout.setName(name);
}

QString PaymentsFacade::email() const
{
    return m_checkout.email();
}

void PaymentsFacade::setEmail(const QString& email)
{
    m_checkout.setEmail(email);
}

QString PaymentsFacade::phone() const
{
    return m_checkout.phone();
}

void PaymentsFacade::setPhone(const QString& phone)
{
    m_checkout.setPhone(phone);
}

void PaymentsFacade::setMethods(const QStringList& ids, const QStringList& titles)
{
    const qsizetype count = qMin(ids.size(), titles.size());
    QVector<telegram::PaymentMethodRow> rows;
    rows.reserve(count);
    for (qsizetype i = 0; i < count; ++i) {
        telegram::PaymentMethodRow row;
        row.id = ids.at(i);
        row.title = titles.at(i);
        rows.append(row);
    }
    // Normalization (empty ids dropped, duplicates collapsed, order kept) is the
    // method model's rule, so the facade does not repeat it.
    m_methods.setMethods(std::move(rows));
}

QStringList PaymentsFacade::methodTitles() const
{
    const QVector<telegram::PaymentMethodRow> rows = m_methods.methods();
    QStringList titles;
    titles.reserve(rows.size());
    for (const telegram::PaymentMethodRow& row : rows)
        titles.append(row.title.isEmpty() ? row.id : row.title);
    return titles;
}

int PaymentsFacade::methodIndex() const
{
    const QString selected = m_methods.selectedId();
    if (selected.isEmpty())
        return -1;

    const QVector<telegram::PaymentMethodRow> rows = m_methods.methods();
    for (int i = 0; i < rows.size(); ++i) {
        if (rows.at(i).id == selected)
            return i;
    }
    return -1;
}

void PaymentsFacade::selectMethod(int index)
{
    if (index == -1) {
        // The method model clears the selection for an empty id; -1 is this
        // facade's "nothing selected" index, so it maps to that clear.
        m_methods.select(QString());
        return;
    }

    const QVector<telegram::PaymentMethodRow> rows = m_methods.methods();
    if (index < 0 || index >= rows.size())
        return;
    m_methods.select(rows.at(index).id);
}

bool PaymentsFacade::canPay() const
{
    if (!m_checkout.valid())
        return false;
    // An offered but unselected list is a real gap: the buyer was asked to pick.
    // No list at all means the invoice pays through its own method.
    return m_methods.count() == 0 || !m_methods.selectedId().isEmpty();
}

bool PaymentsFacade::pay()
{
    if (!canPay())
        return false;

    emit payRequested(QString::number(m_checkout.chatId()),
                      QString::number(m_checkout.messageId()), m_checkout.name(),
                      m_checkout.email(), m_checkout.phone(), m_methods.selectedId());
    return true;
}

void PaymentsFacade::reset()
{
    m_silent = true;
    m_checkout.reset();
    m_methods.clear();
    m_silent = false;

    if (m_pending) {
        m_pending = false;
        emit changed();
    }
}

void PaymentsFacade::handleModelChanged()
{
    if (m_silent) {
        m_pending = true;
        return;
    }
    emit changed();
}

} // namespace beygram::ui
