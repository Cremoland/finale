#pragma once

#include "DSP/LoudnessAssistant.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace finale::dsp
{

// Assistente tonal: compara a forma espectral do original (4 bandas do BandAnalyzer, dB em relação à média)
// com a forma neutra de música comercial e faz a correção de cada banda seguir devagar
// clamp (neutra − medida, ±maxCorrectionDb). Só lógica, sem áudio: quem chama aplica a correção no EQ tonal.
// A medida é feita antes do EQ, então a correção não realimenta a medida (malha aberta, não oscila).
class TonalAssistant
{
public:
    using Mode = LoudnessAssistant::Mode;
    static constexpr int numBands = 4;

    // Forma neutra (energia por oitava em dB, relativa à média das 4 bandas de 90 Hz, 300 Hz, 3 kHz e 10 kHz):
    // espectro médio de pop dos anos 1990, Adrian & Bitzer, "Erstellung und Untersuchung von Langzeitspektren…",
    // DAGA 2014, fig. 2 (cai ~6 dB/oitava em densidade entre 100 Hz e 10 kHz; ruído rosa cai 3).
    // shortcut: uma forma só para todos os gêneros, lida na figura (±2 dB); trocar pela forma medida das
    // referências de cada gênero (finale_curvegen) quando houver.
    static constexpr std::array<float, numBands> neutralShapeDb { 5.0f, 1.5f, -1.5f, -5.0f };
    // shortcut: ±6 dB por banda antes da escala de amount e EQ; quando mudar: correções que soem fortes demais.
    static constexpr float maxCorrectionDb = 6.0f;
    // shortcut: abaixo de −50 LUFS de short-term na entrada a forma não é medida (silêncio, fade).
    static constexpr float inputGateLufs = -50.0f;
    // shortcut: o analisador do plugin esquece com constante de tempo de 10 s.
    static constexpr double analyzerMemorySeconds = 10.0;

    // Zera a análise. Mantém a correção (o projeto a guarda e o host pode preparar de novo) e o modo.
    void prepare (double newSampleRate)
    {
        sampleRate = (std::isfinite (newSampleRate) && newSampleRate > 0.0) ? newSampleRate : 48000.0;
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

    // Correção vinda de fora (projeto salvo). Não-finito é ignorado; fora de ±maxCorrectionDb é limitado.
    void setCorrectionDb (const std::array<float, numBands>& db)
    {
        for (std::size_t b = 0; b < numBands; ++b)
            if (std::isfinite (db[b]))
                correctionDb[b] = std::clamp (static_cast<double> (db[b]), -limit, limit);
    }

    // valid: há música de verdade na entrada (acima do gate). Medida com algum valor não-finito é ignorada.
    void update (const std::array<float, numBands>& measuredShapeDb, bool valid, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const double dt = static_cast<double> (numSamples) / sampleRate;
        bool usable = valid;
        for (const float v : measuredShapeDb)
            usable = usable && std::isfinite (v);

        switch (mode)
        {
        case Mode::off:
            correctionDb.fill (0.0);
            break;

        case Mode::freeze:
            break;

        case Mode::analyze:
            if (analysisDone || !usable)
                break;
            approach (measuredShapeDb, dt / analyzeTauSec);
            analysisTimeSec += dt;
            if (analysisTimeSec >= analysisSec)
                analysisDone = true;
            break;

        case Mode::autoAdapt:
            if (usable)
                approach (measuredShapeDb, dt / autoTauSec);
            break;
        }
    }

    std::array<float, numBands> getCorrectionDb() const
    {
        std::array<float, numBands> out { };
        for (std::size_t b = 0; b < numBands; ++b)
            out[b] = static_cast<float> (correctionDb[b]);
        return out;
    }

private:
    // shortcut: τ de 5 s no auto (a medida já tem memória de 10 s); quando mudar: correção lenta ou nervosa demais.
    static constexpr double autoTauSec = 5.0;
    // shortcut: τ de 2 s na análise, para chegar no alvo dentro dos 20 s.
    static constexpr double analyzeTauSec = 2.0;
    // shortcut: a análise dura 20 s de sinal válido e depois fixa a correção (igual ao assistente de loudness).
    static constexpr double analysisSec = 20.0;
    static constexpr double limit = static_cast<double> (maxCorrectionDb);

    // Passo de primeira ordem em direção ao desejado; o passo é limitado a 1, então nunca passa do alvo.
    void approach (const std::array<float, numBands>& measured, double step)
    {
        const double k = std::min (1.0, step);
        for (std::size_t b = 0; b < numBands; ++b)
        {
            const double desired = std::clamp (
                static_cast<double> (neutralShapeDb[b]) - static_cast<double> (measured[b]), -limit, limit);
            correctionDb[b] += (desired - correctionDb[b]) * k;
        }
    }

    double sampleRate = 48000.0;
    Mode mode = Mode::off;
    std::array<double, numBands> correctionDb { };
    double analysisTimeSec = 0.0;
    bool analysisDone = false;
};

} // namespace finale::dsp
