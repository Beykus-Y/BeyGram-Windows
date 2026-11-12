#include "VoiceComposerFacade.h"

#include "telegram/VoiceRecordingController.h"
#include "telegram/VoiceWaveformModel.h"

#include <utility>

namespace beygram::ui {

namespace {

/// Decimal places used when the normalized envelope is handed to QML.
constexpr int kWaveformDecimals = 3;

} // namespace

VoiceComposerFacade::VoiceComposerFacade(QObject* parent)
    : QObject(parent)
    , m_controller(new telegram::VoiceRecordingController(this))
    , m_waveform(new telegram::VoiceWaveformModel(this))
{
    // Both backing objects already suppress no-op updates, so a plain relay is
    // enough to keep `changed` honest.
    connect(m_controller, &telegram::VoiceRecordingController::changed,
            this, &VoiceComposerFacade::changed);
    connect(m_waveform, &telegram::VoiceWaveformModel::changed,
            this, &VoiceComposerFacade::changed);
}

bool VoiceComposerFacade::recording() const
{
    return m_controller->recording();
}

bool VoiceComposerFacade::paused() const
{
    return m_controller->paused();
}

bool VoiceComposerFacade::locked() const
{
    return m_controller->locked();
}

int VoiceComposerFacade::elapsed() const
{
    return m_controller->elapsedSeconds();
}

QString VoiceComposerFacade::elapsedText() const
{
    const int total = m_controller->elapsedSeconds();
    const int minutes = total / 60;
    const int seconds = total % 60;

    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

QStringList VoiceComposerFacade::waveform() const
{
    const QVector<double> bars = m_waveform->normalized();

    QStringList result;
    result.reserve(bars.size());

    for (const double bar : bars)
        result.append(QString::number(bar, 'f', kWaveformDecimals));

    return result;
}

bool VoiceComposerFacade::canSend() const
{
    return m_controller->recording() && !m_controller->tooShort()
           && !m_controller->tooLong();
}

void VoiceComposerFacade::start()
{
    // Starting while already recording must not reset the elapsed timer or wipe
    // the envelope the user is watching.
    if (m_controller->recording())
        return;

    m_waveform->clear();
    m_controller->start();
}

void VoiceComposerFacade::cancel()
{
    if (!m_controller->recording())
        return;

    m_controller->cancel();
    m_waveform->clear();
    emit cancelRequested();
}

void VoiceComposerFacade::finish()
{
    if (!canSend())
        return;

    const int duration = m_controller->elapsedSeconds();
    m_controller->finish();

    // Emitted before the strip is cleared so a handler can still read the final
    // envelope when it builds the outgoing message. A handler that starts a new
    // recording right away keeps its own strip.
    emit sendRequested(duration);

    if (!m_controller->recording())
        m_waveform->clear();
}

void VoiceComposerFacade::setPaused(bool paused)
{
    m_controller->setPaused(paused);
}

void VoiceComposerFacade::setLocked(bool locked)
{
    m_controller->setLocked(locked);
}

void VoiceComposerFacade::tick(int elapsedSeconds)
{
    m_controller->setElapsedSeconds(elapsedSeconds);
}

void VoiceComposerFacade::setWaveform(const QVariantList& samples)
{
    QVector<int> amplitudes;
    amplitudes.reserve(samples.size());

    for (const QVariant& sample : samples) {
        bool ok = false;
        const int amplitude = sample.toInt(&ok);
        if (!ok)
            continue;

        amplitudes.append(amplitude);
    }

    m_waveform->setSamples(std::move(amplitudes));
}

} // namespace beygram::ui
