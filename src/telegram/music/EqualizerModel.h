#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One equalizer band: the center frequency the platform equalizer reports and
/// the gain applied there.
struct EqualizerBand {
    int frequencyHz = 0;
    double gainDb = 0;
};

/// The equalizer curve of the music player: a list of bands with one gain each,
/// plus the named presets that fill them in.
///
/// This is a model only. It never attaches an audio effect and never touches a
/// device: a playback backend reads bands() and applies them to whatever
/// equalizer it has, and this class keeps the shape of the curve, the selected
/// preset and the user's edits.
///
/// Bands are identified by frequency, not by position, so a curve can be
/// resolved against any band layout. setBands() keeps the gain of every
/// frequency that survives the layout change and starts new bands at 0 dB.
/// Gains are clamped to -12..12 dB, which also leaves the backend headroom to
/// avoid clipping.
///
/// The presets are curves anchored at the octave frequencies 31 Hz..16 kHz and
/// interpolated in log-frequency space at whatever frequencies the bands
/// actually have, so a five-band layout gets five meaningful gains rather than
/// a truncated ten-band table. Gains are rounded to 0.1 dB.
class EqualizerModel final : public QObject {
    Q_OBJECT

public:
    explicit EqualizerModel(QObject* parent = nullptr);

    /// Replaces the band layout with frequencies, keeping the gain of every
    /// frequency that was already present and starting the new ones at 0 dB.
    /// Non-positive frequencies are dropped, duplicates keep their first
    /// occurrence, and the result is sorted ascending because bands run from
    /// low to high. An input with no usable frequency is ignored as a whole, so
    /// the model never loses its bands.
    ///
    /// The default layout is 60, 230, 910, 3600 and 14000 Hz.
    void setBands(const QVector<int>& frequencies);

    /// The bands in ascending frequency order. A copy, so editing it does not
    /// reach the model.
    [[nodiscard]] QVector<EqualizerBand> bands() const;

    /// Sets the gain of the band at frequencyHz, clamped to -12..12 dB. Returns
    /// false when no band has that frequency or when gainDb is NaN, in which
    /// case nothing changes. A value that clamps to the stored gain is accepted
    /// but emits nothing.
    bool setGain(int frequencyHz, double gainDb);

    /// Fills every band with the named preset's curve. Unknown names are
    /// ignored, as is applying the preset that is already in effect.
    void applyPreset(const QString& name);

    /// The preset names, in this order: Flat, Bass, Vocal, Rock, Electronic,
    /// Night.
    [[nodiscard]] QStringList presets() const;

    /// The preset whose curve the bands currently match, or an empty string
    /// when the curve is custom (any manual setGain() edit that no longer
    /// matches a preset). Flat matches an all-zero curve at any layout.
    [[nodiscard]] QString currentPreset() const;

    /// Restores the default layout and a flat curve. Emits changed() only when
    /// something differed.
    void reset();

signals:
    /// Emitted whenever the band layout or a gain actually changed.
    void changed();

private:
    QVector<EqualizerBand> m_bands;
};

} // namespace beygram::telegram
