#include "PaymentMethodModel.h"

#include <utility>

namespace beygram::telegram {
namespace {

// PaymentMethodRow carries no operator== (it is a plain view value), so the
// model compares the fields it holds. Every field is compared: a reload that
// only flips saved or rewrites the subtitle still changes what the row renders.
bool sameMethod(const PaymentMethodRow& left, const PaymentMethodRow& right)
{
    return left.id == right.id
        && left.title == right.title
        && left.subtitle == right.subtitle
        && left.kind == right.kind
        && left.saved == right.saved;
}

bool sameMethods(const QVector<PaymentMethodRow>& left,
                 const QVector<PaymentMethodRow>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype i = 0; i < left.size(); ++i) {
        if (!sameMethod(left.at(i), right.at(i)))
            return false;
    }
    return true;
}

// -1 when no method has that id. The list is one checkout's methods, so a
// linear scan beats keeping a second index in sync.
qsizetype indexOfMethod(const QVector<PaymentMethodRow>& methods, const QString& id)
{
    for (qsizetype i = 0; i < methods.size(); ++i) {
        if (methods.at(i).id == id)
            return i;
    }
    return -1;
}

} // namespace

PaymentMethodModel::PaymentMethodModel(QObject* parent)
    : QObject(parent)
{
}

void PaymentMethodModel::setMethods(QVector<PaymentMethodRow> methods)
{
    // Keep the first row of each id: methods arrive in the order the caller
    // merged its answers, and the first answer is the one already on screen, so
    // it wins over a later duplicate instead of making the visible row depend on
    // merge order. A row without an id could never be selected, so it is
    // dropped.
    QVector<PaymentMethodRow> unique;
    unique.reserve(methods.size());
    for (PaymentMethodRow& method : methods) {
        if (method.id.isEmpty())
            continue;
        if (indexOfMethod(unique, method.id) >= 0)
            continue;
        unique.append(std::move(method));
    }

    // A selection must name an offered row: keeping it while its method is gone
    // would leave selectedId() pointing at something the list cannot show.
    const bool selectionDropped =
        !m_selected.isEmpty() && indexOfMethod(unique, m_selected) < 0;

    // A reload that brings the same rows is not a change: repainting the list
    // for it would also restart every animation bound to changed().
    if (!selectionDropped && sameMethods(unique, m_methods))
        return;

    m_methods = std::move(unique);
    if (selectionDropped)
        m_selected.clear();
    emit changed();
}

QVector<PaymentMethodRow> PaymentMethodModel::methods() const
{
    return m_methods;
}

int PaymentMethodModel::count() const
{
    return static_cast<int>(m_methods.size());
}

QVector<PaymentMethodRow> PaymentMethodModel::savedMethods() const
{
    QVector<PaymentMethodRow> saved;
    saved.reserve(m_methods.size());
    for (const PaymentMethodRow& method : m_methods) {
        if (method.saved)
            saved.append(method);
    }
    return saved;
}

void PaymentMethodModel::select(const QString& id)
{
    // An empty id is "no choice", not a method called "": it clears the
    // selection so the caller does not have to know whether one was set.
    if (id.isEmpty()) {
        if (m_selected.isEmpty())
            return;
        m_selected.clear();
        emit changed();
        return;
    }

    // A tap on a row that is no longer offered must not select an invisible
    // method; the current selection stays as it was.
    if (indexOfMethod(m_methods, id) < 0)
        return;

    if (m_selected == id)
        return;

    m_selected = id;
    emit changed();
}

QString PaymentMethodModel::selectedId() const
{
    return m_selected;
}

PaymentMethodRow PaymentMethodModel::selectedMethod() const
{
    const qsizetype index = indexOfMethod(m_methods, m_selected);
    if (index < 0)
        return {};
    return m_methods.at(index);
}

void PaymentMethodModel::clear()
{
    if (m_methods.isEmpty() && m_selected.isEmpty())
        return;

    m_methods.clear();
    m_selected.clear();
    emit changed();
}

} // namespace beygram::telegram
