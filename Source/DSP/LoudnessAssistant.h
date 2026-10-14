#pragma once

#include "DSP/LoudnessMeter.h"

#include <algorithm>
#include <cmath>

namespace finale::dsp
{

// Assistente de loudness: corrige devagar o ganho de saída para que a loudness medida
// chegue perto do alvo. Só lógica, sem áudio; quem chama aplica a correção com suavização.
//
// Laço: correção += (alvo − medido) * dt / τ. A medida tem ~1.5 s de atraso (short-term),
// por isso τ é longo: um τ curto oscilaria.
class LoudnessAssistant
{
public:
    enum class Mode
    {
        off = 0,
        analyze = 1,
        autoAdapt = 2,
        freeze = 3
    }; // mesma ordem do parâmetro

    // Zera a correção e a análise. Mantém o modo e o alvo.
    void prepare (double newSampleRate)
    {
        sampleRate = (std::isfinite (newSampleRate) && newSampleRate > 0.0) ? newSampleRate : 48000.0;
        correctionDb = 0.0;
        analysisTimeSec = 0.0;
        analysisDone = false;
    }

    void setMode (Mode newMode)
    {
        // Só recomeça a análise ao entrar nela; chamar com o mesmo modo a cada bloco não a reinicia.
        if (newMode == Mode::analyze && mode != Mode::analyze)
        {
            analysisTimeSec = 0.0;
            analysisDone = false;
        }

        mode = newMode;
    }

    void setTargetLufs (float lufs)
    {
        if (std::isfinite (lufs))
            targetLufs = static_cast<double> (lufs);
    }

    void update (float measuredLufs, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const double dt = static_cast<double> (numSamples) / sampleRate;

        // Silêncio (piso do medidor) e valores não finitos não entram na correção.
        const bool valid = std::isfinite (measuredLufs) && measuredLufs > LoudnessMeter::floorLufs;

        switch (mode)
        {
        case Mode::off:
            correctionDb = 0.0;
            break;

        case Mode::freeze:
            break;

        case Mode::analyze:
            if (analysisDone || !valid)
                break;

            correctionDb += (targetLufs - static_cast<double> (measuredLufs)) * dt / analyzeTauSec;
            analysisTimeSec += dt;
            if (analysisTimeSec >= analysisSec)
                analysisDone = true;
            break;

        case Mode::autoAdapt:
            if (valid)
                correctionDb += (targetLufs - static_cast<double> (measuredLufs)) * dt / autoTauSec;
            break;
        }

        correctionDb = std::clamp (correctionDb, -maxCorrectionDb, maxCorrectionDb);
    }

    float getCorrectionDb() const { return static_cast<float> (correctionDb); }

private:
    // shortcut: τ fixo de 20 s no modo auto, sem ajuste por gênero.
    static constexpr double autoTauSec = 20.0;
    // shortcut: τ fixo de 5 s na análise, mais rápido para medir logo.
    static constexpr double analyzeTauSec = 5.0;
    // shortcut: a análise dura 20 s de sinal válido e depois fixa a correção.
    static constexpr double analysisSec = 20.0;
    // shortcut: limite fixo de ±12 dB.
    static constexpr double maxCorrectionDb = 12.0;

    double sampleRate = 48000.0;
    Mode mode = Mode::off;
    double targetLufs = -9.0;
    double correctionDb = 0.0;
    double analysisTimeSec = 0.0;
    bool analysisDone = false;
};

} // namespace finale::dsp
