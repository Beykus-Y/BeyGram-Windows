#include "WebAppSessionController.h"

namespace beygram::telegram {
namespace {

// WebAppInfo carries no comparison operator, so equality is spelled out field
// by field; that is also what keeps changed() from firing on an identical
// description.
bool sameInfo(const WebAppInfo& left, const WebAppInfo& right)
{
    return left.botUserId == right.botUserId && left.url == right.url
           && left.title == right.title
           && left.description == right.description
           && left.isSecure == right.isSecure;
}

} // namespace

WebAppSessionController::WebAppSessionController(QObject* parent)
    : QObject(parent)
{
}

void WebAppSessionController::open(qint64 chatId, qint64 botUserId,
                                   const WebAppInfo& info)
{
    // A Web App cannot be launched without both ids; a malformed button must
    // not tear down the session that is already on screen.
    if (chatId <= 0 || botUserId <= 0)
        return;

    // The loading flag and the error described the previous session, so a new
    // open starts from a clean slate even when the ids repeat.
    const bool dirty = m_chatId != chatId || m_botUserId != botUserId
                       || !sameInfo(m_info, info) || m_loading
                       || !m_error.isEmpty();

    m_chatId = chatId;
    m_botUserId = botUserId;
    m_info = info;
    m_loading = false;
    m_error.clear();

    if (dirty)
        emit changed();
}

void WebAppSessionController::close()
{
    if (!active())
        return;
    clear();
}

bool WebAppSessionController::active() const
{
    return m_chatId > 0 && m_botUserId > 0;
}

void WebAppSessionController::setLoading(bool loading)
{
    if (!active() || m_loading == loading)
        return;
    m_loading = loading;
    emit changed();
}

void WebAppSessionController::setError(const QString& error)
{
    if (!active() || m_error == error)
        return;
    m_error = error;
    emit changed();
}

void WebAppSessionController::clear()
{
    if (m_chatId == 0 && m_botUserId == 0 && !m_loading && m_error.isEmpty()
        && sameInfo(m_info, WebAppInfo())) {
        return;
    }

    m_chatId = 0;
    m_botUserId = 0;
    m_info = WebAppInfo();
    m_loading = false;
    m_error.clear();
    emit changed();
}

} // namespace beygram::telegram
