#include "EqualizerFacade.h"

#include <QVector>

namespace beygram::ui {

namespace {

/// "60 Hz" below a kilohertz, "3.6 kHz" above it. A trailing ".0" is dropped so
/// 14 kHz does not read as "14.0 kHz".
QString formatFrequency(int frequencyHz)
{
    if (frequencyHz < 1000)
        return QStringLiteral("%1 Hz").arg(frequencyHz);

    QString number = QString::number(frequencyHz / 1000.0, 'f', 1);
    if (number.endsWith(QLatin1String(".0")))
        number.chop(2);
    return number + QStringLiteral(" kHz");
}

/// One decimal and the unit, which is the shape a band label shows as-is.
QString formatGain(double gainDb)
{
    // A stored -0.0 would print as "-0.0", which reads like a cut when the band
    // is actually flat.
    const double gain = gainDb == 0.0 ? 0.0 : gainDb;
    return QStringLiteral("%1 dB").arg(QString::number(gain, 'f', 1));
}

} // namespace

EqualizerFacade::EqualizerFacade(QObject* parent) : QObject(parent)
{
    // The model already emits only when the layout or a gain actually changed,
    // so forwarding its signal keeps changed() exact without a second
    // comparison of the formatted strings.
    connect(&m_model, &telegram::EqualizerModel::changed, this, &EqualizerFacade::changed);
}

QStringList EqualizerFacade::bandLabels() const
{
    const QVector<telegram::EqualizerBand> bands = m_model.bands();

    QStringList labels;
    labels.reserve(bands.size());
    for (const telegram::EqualizerBand& band : bands)
        labels.append(formatFrequency(band.frequencyHz));
    return labels;
}

QStringList EqualizerFacade::gains() const
{
    const QVector<telegram::EqualizerBand> bands = m_model.bands();

    QStringList values;
    values.reserve(bands.size());
    for (const telegram::EqualizerBand& band : bands)
        values.append(formatGain(band.gainDb));
    return values;
}

QString EqualizerFacade::preset() const
{
    return m_model.currentPreset();
}

QStringList EqualizerFacade::presets() const
{
    return m_model.presets();
}

bool EqualizerFacade::enabled() const
{
    return m_enabled;
}

void EqualizerFacade::setGain(int index, double gainDb)
{
    const QVector<telegram::EqualizerBand> bands = m_model.bands();
    const int count = static_cast<int>(bands.size());
    if (index < 0 || index >= count)
        return;

    // The model addresses a band by frequency, so the position QML has (which
    // matches the label and gain lists) is translated here.
    m_model.setGain(bands.at(index).frequencyHz, gainDb);
}

void EqualizerFacade::applyPreset(const QString& name)
{
    m_model.applyPreset(name);
}

void EqualizerFacade::reset()
{
    m_model.reset();
}

void EqualizerFacade::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    emit changed();
}

} // namespace beygram::ui
