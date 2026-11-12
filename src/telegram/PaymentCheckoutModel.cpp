#include "PaymentCheckoutModel.h"

#include <QLocale>

namespace beygram::telegram {

namespace {

// A phone number only has to look like one to be worth validating: TDLib owns
// the real check, and a number typed with spaces, dashes, parentheses or a
// leading '+' must still pass. Five digits is the shortest national number the
// client accepts.
constexpr int kMinPhoneDigits = 5;

// Loose on purpose: the schema takes email_address as a plain string and the
// server decides whether it is deliverable. All this rejects is text that
// cannot be an address at all - no '@', or no dot in the domain part.
bool looksLikeEmail(const QString& email)
{
    const int at = email.indexOf(QLatin1Char('@'));
    if (at < 0)
        return false;

    return email.indexOf(QLatin1Char('.'), at + 1) >= 0;
}

int digitCount(const QString& text)
{
    int digits = 0;
    for (const QChar character : text) {
        if (character.isDigit())
            ++digits;
    }
    return digits;
}

} // namespace

PaymentCheckoutModel::PaymentCheckoutModel(QObject* parent) : QObject(parent)
{
}

void PaymentCheckoutModel::setInvoice(qint64 chatId, qint64 messageId, const QString& title,
                                      qint64 totalAmount, const QString& currency)
{
    // TDLib addresses an invoice by a positive chat id and message id, and
    // total_amount is never negative. Anything else would leave an active
    // checkout that no request can use, so it clears the checkout instead.
    if (chatId <= 0 || messageId <= 0 || totalAmount < 0) {
        reset();
        return;
    }

    const QString normalizedTitle = title.trimmed();
    const QString normalizedCurrency = currency.trimmed().toUpper();
    if (m_chatId == chatId && m_messageId == messageId && m_amount == totalAmount
        && m_title == normalizedTitle && m_currency == normalizedCurrency) {
        return;
    }

    m_chatId = chatId;
    m_messageId = messageId;
    m_title = normalizedTitle;
    m_amount = totalAmount;
    m_currency = normalizedCurrency;
    emit changed();
}

bool PaymentCheckoutModel::active() const
{
    return m_chatId > 0 && m_messageId > 0 && m_amount >= 0;
}

qint64 PaymentCheckoutModel::chatId() const
{
    return m_chatId;
}

qint64 PaymentCheckoutModel::messageId() const
{
    return m_messageId;
}

QString PaymentCheckoutModel::title() const
{
    return m_title;
}

qint64 PaymentCheckoutModel::totalAmount() const
{
    return m_amount;
}

QString PaymentCheckoutModel::currency() const
{
    return m_currency;
}

void PaymentCheckoutModel::setName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (m_name == trimmed)
        return;

    m_name = trimmed;
    emit changed();
}

QString PaymentCheckoutModel::name() const
{
    return m_name;
}

void PaymentCheckoutModel::setEmail(const QString& email)
{
    const QString trimmed = email.trimmed();
    if (m_email == trimmed)
        return;

    m_email = trimmed;
    emit changed();
}

QString PaymentCheckoutModel::email() const
{
    return m_email;
}

void PaymentCheckoutModel::setPhone(const QString& phone)
{
    const QString trimmed = phone.trimmed();
    if (m_phone == trimmed)
        return;

    m_phone = trimmed;
    emit changed();
}

QString PaymentCheckoutModel::phone() const
{
    return m_phone;
}

void PaymentCheckoutModel::setSaveCredentials(bool save)
{
    if (m_save == save)
        return;

    m_save = save;
    emit changed();
}

bool PaymentCheckoutModel::saveCredentials() const
{
    return m_save;
}

bool PaymentCheckoutModel::valid() const
{
    if (!active() || m_amount < 0)
        return false;

    // The name is stored trimmed, so an empty check is enough; a name of only
    // spaces never reaches the model.
    if (m_name.isEmpty())
        return false;

    return looksLikeEmail(m_email) || digitCount(m_phone) >= kMinPhoneDigits;
}

QString PaymentCheckoutModel::totalText() const
{
    const QString amount =
        QLocale::c().toString(static_cast<double>(m_amount) / 100.0, 'f', 2);
    if (m_currency.isEmpty())
        return amount;

    return amount + QLatin1Char(' ') + m_currency;
}

void PaymentCheckoutModel::reset()
{
    // "Already empty" means nothing to clear: no active invoice, no text and no
    // consent. reset() then stays silent so closing an untouched checkout does
    // not notify the view.
    const bool empty = !active() && m_chatId == 0 && m_messageId == 0 && m_amount == 0
        && m_title.isEmpty() && m_currency.isEmpty() && m_name.isEmpty() && m_email.isEmpty()
        && m_phone.isEmpty() && !m_save;
    if (empty)
        return;

    m_chatId = 0;
    m_messageId = 0;
    m_amount = 0;
    m_title.clear();
    m_currency.clear();
    m_name.clear();
    m_email.clear();
    m_phone.clear();
    m_save = false;
    emit changed();
}

} // namespace beygram::telegram
