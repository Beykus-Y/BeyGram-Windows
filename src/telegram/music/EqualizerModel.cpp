#include "EqualizerModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace beygram::telegram {

namespace {

// Positive gain is capped well below what most platform equalizers allow, so
// several boosted bands together cannot clip the output.
constexpr double kMinGainDb = -12.0;
constexpr double kMaxGainDb = 12.0;

// Preset gains are stored with this resolution, which is finer than any slider
// a user could move.
constexpr double kGainStepDb = 0.1;
constexpr double kGainEpsilon = 1e-9;

constexpr std::array<int, 10> kAnchorFrequenciesHz = {
    31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000,
};

constexpr std::array<int, 5> kDefaultFrequenciesHz = {60, 230, 910, 3600, 14000};

struct PresetCurve {
    const char* name;
    std::array<double, 10> gainsDb;
};

// The same curves the Android player uses, described at the anchor
// frequencies and resolved against the real bands below.
const std::array<PresetCurve, 6>& presetCurves()
{
    static const std::array<PresetCurve, 6> curves = {{
        {"Flat", {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {"Bass", {6.0, 5.0, 3.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {"Vocal", {-2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0, 2.0, 1.0, 0.0}},
        {"Rock", {4.0, 3.0, 2.0, 1.0, -1.0, -1.0, 0.0, 1.0, 2.0, 3.0}},
        {"Electronic", {5.0, 4.0, 2.0, 0.0, -1.0, 1.0, 2.0, 3.0, 4.0, 4.0}},
        {"Night", {-4.0, -3.0, -2.0, -1.0, 0.0, 0.0, -1.0, -2.0, -3.0, -4.0}},
    }};
    return curves;
}

double roundedGain(double gainDb)
{
    return std::round(gainDb / kGainStepDb) * kGainStepDb;
}

// Interpolates the curve at frequencyHz in log-frequency space, where octave
// spaced anchors and bands line up. Frequencies outside the anchors keep the
// nearest end value.
double gainAt(const std::array<double, 10>& curve, int frequencyHz)
{
    const double x = std::log10(static_cast<double>(std::max(frequencyHz, 1)));
    const double firstX = std::log10(static_cast<double>(kAnchorFrequenciesHz.front()));
    if (x <= firstX)
        return curve.front();

    for (std::size_t index = 0; index + 1 < kAnchorFrequenciesHz.size(); ++index) {
        const double rightX = std::log10(static_cast<double>(kAnchorFrequenciesHz[index + 1]));
        if (x > rightX)
            continue;

        const double leftX = std::log10(static_cast<double>(kAnchorFrequenciesHz[index]));
        const double fraction = (x - leftX) / (rightX - leftX);
        return curve[index] + (curve[index + 1] - curve[index]) * fraction;
    }
    return curve.back();
}

// Empty when the name is unknown; the caller treats that as "leave as is".
QVector<double> presetGainsDb(const QString& name, const QVector<int>& frequenciesHz)
{
    const PresetCurve* curve = nullptr;
    for (const PresetCurve& candidate : presetCurves()) {
        if (name == QLatin1String(candidate.name)) {
            curve = &candidate;
            break;
        }
    }
    if (curve == nullptr)
        return {};

    QVector<double> gains;
    gains.reserve(frequenciesHz.size());
    for (const int frequency : frequenciesHz) {
        const double gain = std::clamp(roundedGain(gainAt(curve->gainsDb, frequency)),
                                       kMinGainDb, kMaxGainDb);
        gains.append(gain);
    }
    return gains;
}

QVector<int> frequenciesOf(const QVector<EqualizerBand>& bands)
{
    QVector<int> frequencies;
    frequencies.reserve(bands.size());
    for (const EqualizerBand& band : bands)
        frequencies.append(band.frequencyHz);
    return frequencies;
}

bool sameBands(const QVector<EqualizerBand>& lhs, const QVector<EqualizerBand>& rhs)
{
    if (lhs.size() != rhs.size())
        return false;

    for (int index = 0; index < lhs.size(); ++index) {
        const EqualizerBand& left = lhs.at(index);
        const EqualizerBand& right = rhs.at(index);
        if (left.frequencyHz != right.frequencyHz || left.gainDb != right.gainDb)
            return false;
    }
    return true;
}

} // namespace

EqualizerModel::EqualizerModel(QObject* parent) : QObject(parent)
{
    m_bands.reserve(static_cast<int>(kDefaultFrequenciesHz.size()));
    for (const int frequency : kDefaultFrequenciesHz)
        m_bands.append(EqualizerBand{frequency, 0.0});
}

void EqualizerModel::setBands(const QVector<int>& frequencies)
{
    QVector<int> cleaned;
    cleaned.reserve(frequencies.size());
    for (const int frequency : frequencies) {
        if (frequency <= 0 || cleaned.contains(frequency))
            continue;
        cleaned.append(frequency);
    }
    if (cleaned.isEmpty())
        return;
    std::sort(cleaned.begin(), cleaned.end());

    QVector<EqualizerBand> bands;
    bands.reserve(cleaned.size());
    for (const int frequency : cleaned) {
        double gainDb = 0.0;
        for (const EqualizerBand& existing : m_bands) {
            if (existing.frequencyHz == frequency) {
                gainDb = existing.gainDb;
                break;
            }
        }
        bands.append(EqualizerBand{frequency, gainDb});
    }

    if (sameBands(bands, m_bands))
        return;

    m_bands = bands;
    emit changed();
}

QVector<EqualizerBand> EqualizerModel::bands() const
{
    return m_bands;
}

bool EqualizerModel::setGain(int frequencyHz, double gainDb)
{
    // A NaN has no meaningful clamp and would poison every later comparison.
    if (std::isnan(gainDb))
        return false;

    const auto band = std::find_if(m_bands.begin(), m_bands.end(),
                                   [frequencyHz](const EqualizerBand& candidate) {
                                       return candidate.frequencyHz == frequencyHz;
                                   });
    if (band == m_bands.end())
        return false;

    const double clamped = std::clamp(gainDb, kMinGainDb, kMaxGainDb);
    if (band->gainDb == clamped)
        return true;

    band->gainDb = clamped;
    emit changed();
    return true;
}

void EqualizerModel::applyPreset(const QString& name)
{
    const QVector<double> gains = presetGainsDb(name, frequenciesOf(m_bands));
    if (gains.isEmpty())
        return;

    bool modified = false;
    for (int index = 0; index < m_bands.size(); ++index) {
        if (m_bands.at(index).gainDb == gains.at(index))
            continue;
        m_bands[index].gainDb = gains.at(index);
        modified = true;
    }
    if (modified)
        emit changed();
}

QStringList EqualizerModel::presets() const
{
    QStringList names;
    for (const PresetCurve& preset : presetCurves())
        names.append(QString::fromLatin1(preset.name));
    return names;
}

QString EqualizerModel::currentPreset() const
{
    const QVector<int> frequencies = frequenciesOf(m_bands);
    for (const PresetCurve& preset : presetCurves()) {
        const QVector<double> gains = presetGainsDb(QString::fromLatin1(preset.name), frequencies);
        if (gains.size() != m_bands.size())
            continue;

        bool matches = true;
        for (int index = 0; matches && index < m_bands.size(); ++index)
            matches = std::abs(m_bands.at(index).gainDb - gains.at(index)) < kGainEpsilon;
        if (matches)
            return QString::fromLatin1(preset.name);
    }
    return QString();
}

void EqualizerModel::reset()
{
    QVector<EqualizerBand> defaults;
    defaults.reserve(static_cast<int>(kDefaultFrequenciesHz.size()));
    for (const int frequency : kDefaultFrequenciesHz)
        defaults.append(EqualizerBand{frequency, 0.0});

    if (sameBands(defaults, m_bands))
        return;

    m_bands = defaults;
    emit changed();
}

} // namespace beygram::telegram
