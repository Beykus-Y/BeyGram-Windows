#pragma once

#include "telegram/EmojiCatalogModel.h"
#include "telegram/TextReplacementModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The emoji picker as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth: the
/// catalog, the recent emoji and the composer's replacement rules all live in
/// the telegram models this class owns, and every property is read straight
/// from them. The facade only does what QML cannot: it accepts the parallel
/// string lists a TDLib-shaped catalog arrives as, turns them into the model's
/// value types, and answers with the already-filtered emoji strings.
///
/// emojis() is the catalog narrowed by the selected category and the search
/// query. The two filters combine, so a query searches inside the selected
/// section instead of escaping it. An empty query matches every emoji, and
/// categoryIndex() == -1 means "no category filter", i.e. every section.
///
/// The selection is remembered by category name, not by index. Replacing the
/// catalog therefore keeps the user on the same section even when the sections
/// are reordered, and a section that disappeared falls back to "all" instead of
/// leaving the picker on a dead index.
///
/// Every value QML can read is a property with the single changed() notify
/// signal, so a binding never has to know which model moved. A mutation emits
/// changed() only when the exposed state really differs, and only once per
/// call, which keeps a repaint bound to changed() cheap.
///
/// Nothing here touches TDLib, the network, storage or logging. activate()
/// only reports the tapped emoji; inserting it into the composer is the
/// caller's job.
class EmojiFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList categories READ categories NOTIFY changed)
    Q_PROPERTY(QStringList emojis READ emojis NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(int categoryIndex READ categoryIndex NOTIFY changed)

public:
    explicit EmojiFacade(QObject* parent = nullptr);

    /// The distinct non-empty section titles, in first-seen order. Two
    /// spellings differing only by case are one section, named by the first.
    [[nodiscard]] QStringList categories() const;

    /// The emoji of the current selection and query, in catalog order.
    [[nodiscard]] QStringList emojis() const;

    /// The trimmed search text. Empty means "no search".
    [[nodiscard]] QString query() const;
    void setQuery(const QString& query);

    /// Replaces the catalog from the three parallel lists a catalog arrives as:
    /// emoji, searchable name and section title, read positionally.
    ///
    /// The emoji list defines the rows, so a name or category past its end has
    /// nothing to describe and is ignored, while a row without a name or
    /// category reads as empty. The model's normalisation applies: a row
    /// without an emoji cannot be inserted and is dropped, and a repeated emoji
    /// keeps only its first row.
    Q_INVOKABLE void setCatalog(const QStringList& emojis, const QStringList& names,
                                const QStringList& categories);

    /// Index into categories(), or -1 for "all sections". Anything that does
    /// not address an existing section, including -1, selects "all".
    [[nodiscard]] int categoryIndex() const;
    Q_INVOKABLE void selectCategory(int index);

    /// Called by the picker when an emoji is tapped: the emoji becomes the
    /// newest recent entry and emojiActivated() asks the owner to insert it.
    /// An empty emoji is ignored and emits nothing.
    Q_INVOKABLE void activate(const QString& emoji);

    /// Appends one literal find-and-replace rule, e.g. "brb" -> "be right
    /// back". An empty needle cannot match and is ignored, and because the
    /// model keeps the first rule per needle, re-adding a needle leaves the
    /// existing rule in place. An empty replacement deletes the needle.
    Q_INVOKABLE void addReplacement(const QString& from, const QString& to);

    /// Rewrites outgoing text with every rule, longest needle first, in one
    /// left-to-right pass. Text with no needle comes back unchanged.
    Q_INVOKABLE QString applyReplacements(const QString& text) const;

signals:
    void changed();
    void emojiActivated(const QString& emoji);

private:
    /// Everything a QML binding can read. Mutators snapshot it, run, and emit
    /// changed() only when the snapshot moved, so a no-op call stays silent.
    struct State {
        QStringList categories;
        QStringList emojis;
        QString query;
        int categoryIndex = -1;

        bool operator==(const State&) const = default;
    };

    [[nodiscard]] State state() const;
    void emitIfChanged(const State& before);

    /// The canonical spelling of the selected section, or empty for "all".
    /// Resolved on every read, which is what makes the selection survive a
    /// catalog that re-spells or drops it.
    [[nodiscard]] QString selectedCategory() const;

    telegram::EmojiCatalogModel m_catalog;
    telegram::TextReplacementModel m_replacements;
    QString m_selectedCategory;
    QString m_query;
};

} // namespace beygram::ui
