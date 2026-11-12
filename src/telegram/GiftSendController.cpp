#include "GiftSendController.h"

#include "GiftRequests.h"

#include <QChar>

namespace beygram::telegram {

namespace {

// Local cap for the gift message. The schema's real limit is the
// gift_text_length_max option; the builder never truncates, so the form caps
// here before the text can reach a paid request.
constexpr int kMaxMessageLength = 255;

// Keeps at most kMaxMessageLength Unicode code points. QString is UTF-16, so a
// surrogate pair is advanced as one character instead of being cut in half.
QString truncateToCodePoints(const QString& text)
{
    int index = 0;
    int codePoints = 0;
    while (index < text.size() && codePoints < kMaxMessageLength) {
        const bool surrogatePair = text.at(index).isHighSurrogate()
            && index + 1 < text.size() && text.at(index + 1).isLowSurrogate();
        index += surrogatePair ? 2 : 1;
        ++codePoints;
    }
    return text.left(index);
}

QString cappedMessage(const QString& text)
{
    return truncateToCodePoints(text.trimmed());
}

// sendGiftRequest() takes one private chat id, so the recipient is a single
// id: an opened chat wins, and the user id covers a recipient picked before its
// chat exists.
qint64 giftRecipient(qint64 chatId, qint64 userId)
{
    return chatId > 0 ? chatId : userId;
}

} // namespace

GiftSendController::GiftSendController(QObject* parent)
    : QObject(parent)
{
}

void GiftSendController::setRecipient(qint64 chatId, qint64 userId)
{
    if (m_chatId == chatId && m_userId == userId)
        return;

    m_chatId = chatId;
    m_userId = userId;
    emit changed();
}

qint64 GiftSendController::chatId() const
{
    return m_chatId;
}

qint64 GiftSendController::userId() const
{
    return m_userId;
}

void GiftSendController::setGift(qint64 giftId, qint64 starCount)
{
    // A negative price is not a discount, it is missing input: no catalog gift
    // costs less than one star, so it normalises to "no gift".
    const qint64 stars = starCount < 0 ? 0 : starCount;

    if (m_giftId == giftId && m_stars == stars)
        return;

    m_giftId = giftId;
    m_stars = stars;
    emit changed();
}

qint64 GiftSendController::giftId() const
{
    return m_giftId;
}

qint64 GiftSendController::starCount() const
{
    return m_stars;
}

void GiftSendController::setMessage(const QString& text)
{
    const QString message = cappedMessage(text);
    if (m_message == message)
        return;

    m_message = message;
    emit changed();
}

QString GiftSendController::message() const
{
    return m_message;
}

void GiftSendController::setAnonymous(bool anonymous)
{
    if (m_anonymous == anonymous)
        return;

    m_anonymous = anonymous;
    emit changed();
}

bool GiftSendController::anonymous() const
{
    return m_anonymous;
}

bool GiftSendController::valid() const
{
    return m_giftId > 0 && m_stars > 0
        && (m_chatId > 0 || m_userId > 0);
}

QJsonObject GiftSendController::request() const
{
    if (!valid())
        return {};

    // The builder owns the schema shape; only the private/public flag is
    // overridden, because sendGiftRequest() fixes is_private to false and
    // anonymity is the one field it cannot express.
    QJsonObject request =
        sendGiftRequest(m_giftId, giftRecipient(m_chatId, m_userId), m_message);
    if (m_anonymous)
        request.insert(QStringLiteral("is_private"), true);
    return request;
}

void GiftSendController::reset()
{
    if (m_chatId == 0 && m_userId == 0 && m_giftId == 0 && m_stars == 0
        && m_message.isEmpty() && !m_anonymous) {
        return;
    }

    m_chatId = 0;
    m_userId = 0;
    m_giftId = 0;
    m_stars = 0;
    m_message.clear();
    m_anonymous = false;
    emit changed();
}

} // namespace beygram::telegram
