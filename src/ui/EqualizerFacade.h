#pragma once

#include "telegram/music/EqualizerModel.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// QML-facing facade over EqualizerModel: the band labels and gains a panel
/// draws, the named presets, and the on/off switch the model does not keep.
///
/// Every string is pre-formatted because QML only displays it:
///  - bandLabels() names the frequencies the model currently has, "60 Hz"
///    below a kilohertz and "3.6 kHz" above it;
///  - gains() is one "x.y dB" value per band, always with one decimal and the
///    unit, so a label can show it as it comes and a slider can parse the
///    number back out;
///  - preset() is the model's current preset, empty while the curve is custom.
///
/// The band index setGain() takes is the position in the model's bands(), which
/// is ascending frequency, so it matches the index of the label and of the
/// gain. An index outside that range and a NaN are ignored, and a value that
/// clamps to what is already stored changes nothing.
///
/// The enable flag lives here rather than in the model: EqualizerModel is a
/// curve only, and a playback backend reads enabled() to decide whether to
/// attach its effect at all. It starts off, because attaching an effect changes
/// the sound of every track. reset() restores the default bands and a flat
/// curve but leaves the switch alone: clearing the curve is not the same as
/// switching the equalizer on or off.
///
/// changed() is forwarded from the model and is also emitted when the switch
/// flips, so it always covers the whole observable state and one call never
/// emits it twice.
class EqualizerFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList bandLabels READ bandLabels NOTIFY changed)
    Q_PROPERTY(QStringList gains READ gains NOTIFY changed)
    Q_PROPERTY(QString preset READ preset NOTIFY changed)
    Q_PROPERTY(QStringList presets READ presets NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)

public:
    explicit EqualizerFacade(QObject* parent = nullptr);

    /// The band names, ascending: "60 Hz", "230 Hz", "910 Hz", "3.6 kHz" and
    /// "14 kHz" for the default layout. A copy.
    [[nodiscard]] QStringList bandLabels() const;

    /// One "x.y dB" string per band, in band order, with one decimal and the
    /// unit. A copy.
    [[nodiscard]] QStringList gains() const;

    /// The preset whose curve the bands match, empty while the curve is custom.
    [[nodiscard]] QString preset() const;

    /// The preset names, in this order: Flat, Bass, Vocal, Rock, Electronic,
    /// Night. A copy.
    [[nodiscard]] QStringList presets() const;

    /// Whether the playback backend should apply the curve. Off by default.
    [[nodiscard]] bool enabled() const;

    /// Sets the gain of the band at index, which the model clamps to -12..12 dB.
    /// An index outside the band list and a NaN are ignored.
    Q_INVOKABLE void setGain(int index, double gainDb);

    /// Fills every band with the named preset's curve. Unknown names are
    /// ignored, as is applying the preset that is already in effect.
    Q_INVOKABLE void applyPreset(const QString& name);

    /// Restores the default bands and a flat curve, leaving enabled() alone.
    Q_INVOKABLE void reset();

    /// Flips the switch. Emits changed() only when the value differs.
    Q_INVOKABLE void setEnabled(bool enabled);

signals:
    /// Emitted whenever any observable value of the facade changed.
    void changed();

private:
    telegram::EqualizerModel m_model;
    bool m_enabled = false;
};

} // namespace beygram::ui
