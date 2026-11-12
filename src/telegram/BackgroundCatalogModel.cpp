#include "BackgroundCatalogModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool storesBackground(const QVector<BackgroundInfo>& backgrounds, qint64 id)
{
    return std::any_of(backgrounds.cbegin(), backgrounds.cend(),
                       [id](const BackgroundInfo& background) {
                           return background.id == id;
                       });
}

// BackgroundInfo carries no operator==, so the change check compares the five
// fields explicitly. Keeps setBackgrounds() from notifying on a no-op update.
bool sameBackgrounds(const QVector<BackgroundInfo>& lhs,
                     const QVector<BackgroundInfo>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                      [](const BackgroundInfo& left, const BackgroundInfo& right) {
                          return left.id == right.id
                              && left.name == right.name
                              && left.isDefault == right.isDefault
                              && left.isDark == right.isDark
                              && left.fileId == right.fileId;
                      });
}

} // namespace

BackgroundCatalogModel::BackgroundCatalogModel(QObject* parent)
    : QObject(parent)
{
}

void BackgroundCatalogModel::setBackgrounds(QVector<BackgroundInfo> backgrounds)
{
    QVector<BackgroundInfo> next;
    next.reserve(backgrounds.size());

    for (BackgroundInfo& background : backgrounds) {
        // backgroundFromJson() reports a malformed entry as id 0; such a row
        // cannot be selected, so it is dropped instead of stored.
        if (background.id <= 0)
            continue;
        // Telegram background ids are unique. The first row wins over later
        // repeats and the incoming order is otherwise kept.
        if (storesBackground(next, background.id))
            continue;

        background.name = background.name.trimmed();
        next.append(std::move(background));
    }

    if (sameBackgrounds(next, m_backgrounds))
        return;

    m_backgrounds = std::move(next);
    emit changed();
}

QVector<BackgroundInfo> BackgroundCatalogModel::backgrounds() const
{
    return m_backgrounds;
}

int BackgroundCatalogModel::count() const
{
    return static_cast<int>(m_backgrounds.size());
}

BackgroundInfo BackgroundCatalogModel::defaultBackground() const
{
    for (const BackgroundInfo& background : m_backgrounds) {
        if (background.isDefault)
            return background;
    }
    return BackgroundInfo{};
}

BackgroundInfo BackgroundCatalogModel::find(qint64 backgroundId) const
{
    const auto it = std::find_if(m_backgrounds.cbegin(), m_backgrounds.cend(),
                                 [backgroundId](const BackgroundInfo& background) {
                                     return background.id == backgroundId;
                                 });

    if (it == m_backgrounds.cend())
        return BackgroundInfo{};

    return *it;
}

void BackgroundCatalogModel::clear()
{
    if (m_backgrounds.isEmpty())
        return;

    m_backgrounds.clear();
    emit changed();
}

} // namespace beygram::telegram
