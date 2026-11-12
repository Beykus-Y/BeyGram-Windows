#pragma once

#include "telegram/AccessibilityModel.h"
#include "telegram/ChatBackgroundController.h"
#include "telegram/LanguagePackModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The appearance screen as one QML-facing object.
///
/// This is a thin presentation facade, not a second source of truth. The
/// accessibility switches and the text scale live in AccessibilityModel, the
/// chat background and its dimmer/blur in ChatBackgroundController, and the
/// offered language packs and the picked one in LanguagePackModel. Every getter
/// reads straight from those models and every setter forwards to them, so their
/// validation and their "changed() only on a real change" rule are what the
/// screen sees.
///
/// The facade only does what QML cannot: it turns the BackgroundKind enum into
/// the stable string QML binds to, accepts the parallel code/name lists a
/// TDLib-shaped payload arrives as, and maps the selected language to the row
/// index a list view needs.
///
/// Invariants:
///  - The defaults are the models' defaults: every accessibility switch off, the
///    text scale at 100 percent, background kind "none" with an empty value,
///    dimmer and blur at 0, no languages and no selection.
///  - textScale is clamped to 85..150 and dimmer/blur to 0..100 by the models,
///    so the getters never report an out-of-range value.
///  - backgroundKind() is one of "none", "solid", "gradient", "pattern" or
///    "wallpaper". An unknown string is ignored rather than stored, and a
///    non-"none" kind is only accepted while a non-blank background value is
///    stored, because a kind without a value cannot be painted. Setting "none"
///    clears the value; selecting another kind keeps the stored value.
///  - backgroundValue() never stores a blank value. A non-blank value written
///    while the kind is "none" selects "solid", so the two fields can be filled
///    in either order from QML. Clearing the value is only possible by selecting
///    "none".
///  - languageNames() is the stored rows' names in stored order. Codes are
///    lowercased and de-duplicated by LanguagePackModel, and a blank code is
///    dropped. A blank name falls back to the code, so a row always has a label.
///  - languageIndex() is the position of the selected code in languageNames(),
///    or -1 when nothing is selected or the selected code is no longer stored.
///  - selectLanguage() accepts an in-range index, ignores an out-of-range one and
///    clears the selection for a negative index. languageSelected() is emitted
///    only when the stored selection really moved, with the code as stored.
///  - setLanguages() keeps the selection, as LanguagePackModel specifies, so a
///    refresh that still offers the chosen pack does not undo the choice.
///  - reset() restores the accessibility defaults and the background defaults
///    and clears the language selection, but keeps the offered languages,
///    because the list describes what the client can offer, not a user choice.
///  - changed() is emitted only when one of the models really changed. Setters
///    that are rejected or land on the stored value stay silent, including the
///    matching backgroundChanged()/languageSelected().
///
/// The facade never talks to TDLib, the network or storage, performs no I/O and
/// does not log. Applying a change to the rendered UI is the caller's job.
class AppearanceFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY changed)
    Q_PROPERTY(bool highContrast READ highContrast WRITE setHighContrast NOTIFY changed)
    Q_PROPERTY(bool largeText READ largeText WRITE setLargeText NOTIFY changed)
    Q_PROPERTY(int textScale READ textScale WRITE setTextScale NOTIFY changed)
    Q_PROPERTY(QString backgroundKind READ backgroundKind WRITE setBackgroundKind NOTIFY changed)
    Q_PROPERTY(QString backgroundValue READ backgroundValue WRITE setBackgroundValue NOTIFY changed)
    Q_PROPERTY(int dimmer READ dimmer WRITE setDimmer NOTIFY changed)
    Q_PROPERTY(int blur READ blur WRITE setBlur NOTIFY changed)
    Q_PROPERTY(QStringList languageNames READ languageNames NOTIFY changed)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY changed)

public:
    explicit AppearanceFacade(QObject* parent = nullptr);

    /// Whether animations should be reduced. Off by default.
    [[nodiscard]] bool reduceMotion() const;
    void setReduceMotion(bool on);

    /// Whether the high-contrast palette should be used. Off by default.
    [[nodiscard]] bool highContrast() const;
    void setHighContrast(bool on);

    /// Whether the large-text presentation should be used. Off by default and
    /// independent of textScale(), as AccessibilityModel specifies.
    [[nodiscard]] bool largeText() const;
    void setLargeText(bool on);

    /// Text scale in percent, clamped to 85..150. 100 by default.
    [[nodiscard]] int textScale() const;
    void setTextScale(int percent);

    /// The background kind as "none", "solid", "gradient", "pattern" or
    /// "wallpaper". Unknown strings are ignored.
    [[nodiscard]] QString backgroundKind() const;
    void setBackgroundKind(const QString& kind);

    /// The background value, e.g. "#1b2836" or a wallpaper URL. Empty for
    /// "none". The value is trimmed before it is stored, a blank value is
    /// ignored, and a non-blank value written while the kind is "none" selects
    /// "solid".
    [[nodiscard]] QString backgroundValue() const;
    void setBackgroundValue(const QString& value);

    /// How much the background is darkened behind the messages, in percent,
    /// clamped to 0..100. 0 by default.
    [[nodiscard]] int dimmer() const;
    void setDimmer(int percent);

    /// How much the background is blurred, in percent, clamped to 0..100. 0 by
    /// default.
    [[nodiscard]] int blur() const;
    void setBlur(int percent);

    /// Replaces the offered language packs from parallel lists, read
    /// positionally. A code is the stable pack id ("en", "pt-br") and is
    /// trimmed; a blank code is dropped. A blank or missing name falls back to
    /// the code. The lists may be shorter than codes. LanguagePackModel then
    /// lowercases and de-duplicates the codes and keeps the current selection.
    Q_INVOKABLE void setLanguages(const QStringList& codes, const QStringList& names);

    /// The stored language names in stored order.
    [[nodiscard]] QStringList languageNames() const;

    /// The index of the selected language in languageNames(), or -1.
    [[nodiscard]] int languageIndex() const;

    /// Selects the language at `index`. A negative index clears the selection
    /// and an index that is not a stored row is ignored.
    Q_INVOKABLE void selectLanguage(int index);

    /// Restores the defaults: the accessibility switches off and the scale at
    /// 100, no background, no dimmer and no blur, and no selected language. The
    /// offered languages are kept.
    Q_INVOKABLE void reset();

signals:
    /// Emitted whenever any observable state of the facade really changed.
    void changed();

    /// Emitted by selectLanguage() when the stored selection really moved, with
    /// the code as stored (lowercased). Clearing the selection stays silent.
    void languageSelected(const QString& code);

    /// Emitted when the background kind or value really changed, with both new
    /// values, so the caller can repaint without reading them back.
    void backgroundChanged(const QString& kind, const QString& value);

private:
    telegram::AccessibilityModel m_accessibility;
    telegram::ChatBackgroundController m_background;
    telegram::LanguagePackModel m_languages;
};

} // namespace beygram::ui
