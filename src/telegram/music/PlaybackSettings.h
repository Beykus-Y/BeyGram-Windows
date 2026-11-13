#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Device-local preferences for music playback: the speed, the transition
/// between one track and the next, and the loudness helpers.
///
/// This is a model only. It never starts a player and never touches a device:
/// a playback backend reads the values and applies them to whatever it has, and
/// this class keeps the user's choices and their bounds.
///
/// The named speed stops are 0.5, 0.75, 1, 1.25, 1.5, 1.75 and 2, and a fine
/// control moves in 0.05 steps. Every stored value is clamped to 0.5..2.0 and
/// rounded to two decimals, so a slider that reports 1.237 stores 1.24. A NaN
/// is ignored because it would poison every later comparison.
///
/// The crossfade is a length, not a switch: 0 means the tracks are cut, and
/// anything above 0 overlaps the end of one track with the start of the next.
/// It is clamped to 0..12000 ms and defaults to 4000 ms, matching the 4 s
/// overlap and the 12 s ceiling of the Android TrackTransitionSettings.
class PlaybackSettings final : public QObject {
    Q_OBJECT

public:
    explicit PlaybackSettings(QObject* parent = nullptr);

    /// Sets the speed, clamped to 0.5..2.0 and rounded to two decimals. A NaN
    /// is ignored. A value that ends up equal to the stored one emits nothing.
    void setSpeed(double speed);
    [[nodiscard]] double speed() const;

    /// The named speed stops, in this order: 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2.
    /// A copy, so editing it does not reach the model.
    [[nodiscard]] QVector<double> speedOptions() const;

    /// Sets the overlap in milliseconds, clamped to 0..12000. 0 disables the
    /// overlap, so there is no separate crossfade switch.
    void setCrossfadeMs(int ms);
    [[nodiscard]] int crossfadeMs() const;

    /// Plays the next track without the gap a player would otherwise leave
    /// between two decodes. Off by default.
    void setGapless(bool on);
    [[nodiscard]] bool gapless() const;

    /// Evens out the level between tracks that were mastered at different
    /// loudness. Off by default.
    void setNormalizeVolume(bool on);
    [[nodiscard]] bool normalizeVolume() const;

    /// Skips the silence at the end of a track and at the start of the next
    /// one. Off by default on the desktop, where the crossfade covers the same
    /// case.
    void setSkipSilence(bool on);
    [[nodiscard]] bool skipSilence() const;

    /// Restores 1x speed, a 4000 ms overlap and every switch off. Emits
    /// changed() only when something differed.
    void reset();

signals:
    /// Emitted whenever any stored value actually changed.
    void changed();

private:
    double m_speed = 1.0;
    int m_crossfadeMs = 4000;
    bool m_gapless = false;
    bool m_normalizeVolume = false;
    bool m_skipSilence = false;
};

} // namespace beygram::telegram
