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

    // Zera a análise e o pico recente. Mantém a correção (o projeto a guarda e o host
    // pode preparar de novo no meio da sessão), o modo e o alvo.
    void prepare (double newSampleRate)
    {
        sampleRate = (std::isfinite (newSampleRate) && newSampleRate > 0.0) ? newSampleRate : 48000.0;
        analysisTimeSec = 0.0;
        analysisDone = false;
        hasRecentPeak = false;
        recentPeakLufs = 0.0;
    }

    // Correção vinda de fora (projeto salvo). Não-finito é ignorado; fora de ±12 é limitado.
    void setCorrectionDb (float db)
    {
        if (std::isfinite (db))
            correctionDb = std::clamp (static_cast<double> (db), -maxCorrectionDb, maxCorrectionDb);
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

    // limiterReductionDb: redução máxima do limiter no pedaço (dB, ≥ 0). Com ela alta,
    // a correção não sobe: o limiter já está segurando e subir só aumentaria a compressão.
    void update (float measuredLufs, int numSamples, float limiterReductionDb = 0.0f)
    {
        if (numSamples <= 0)
            return;

        const double dt = static_cast<double> (numSamples) / sampleRate;

        // Silêncio (piso do medidor) e valores não finitos não entram na correção.
        const bool valid = std::isfinite (measuredLufs) && measuredLufs > LoudnessMeter::floorLufs;
        if (valid)
            updateRecentPeak (static_cast<double> (measuredLufs), dt);

        const double error = targetLufs - static_cast<double> (measuredLufs);

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

            if (mayIntegrate (error, static_cast<double> (measuredLufs), limiterReductionDb))
                correctionDb += error * dt / analyzeTauSec;
            analysisTimeSec += dt;
            if (analysisTimeSec >= analysisSec)
                analysisDone = true;
            break;

        case Mode::autoAdapt:
            if (valid && mayIntegrate (error, static_cast<double> (measuredLufs), limiterReductionDb))
                correctionDb += error * dt / autoTauSec;
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
    // shortcut: medido 20 dB abaixo do alvo não integra (intro baixa, breakdown, fim); quando mudar: música com faixas
    // muito baixas.
    static constexpr double gateBelowTargetDb = 20.0;
    // shortcut: medido 10 dB abaixo do pico recente não integra (queda do fim da música); quando mudar: música com
    // quedas bruscas legítimas.
    static constexpr double gateBelowPeakDb = 10.0;
    // shortcut: o pico recente cai 1 dB/s quando o medido não sobe; quando mudar: música que muda de nível devagar.
    static constexpr double peakFallDbPerSec = 1.0;
    // shortcut: 6 dB de redução do limiter para de subir a correção; quando mudar: limiter mais suave ou mais
    // agressivo.
    static constexpr float limiterGateDb = 6.0f;

    // Pico recente: segue o medido quando ele sobe e cai devagar quando não. Só medidas válidas.
    void updateRecentPeak (double measured, double dt)
    {
        if (!hasRecentPeak)
        {
            recentPeakLufs = measured;
            hasRecentPeak = true;
            return;
        }

        recentPeakLufs = std::max (measured, recentPeakLufs - peakFallDbPerSec * dt);
    }

    // Diz se o erro pode entrar na correção. Medido longe do alvo ou do pico recente é
    // intro, breakdown ou fim de música: não deve puxar a correção. Subir com o limiter
    // já trabalhando forte também não entra.
    bool mayIntegrate (double error, double measured, float limiterReductionDb) const
    {
        if (measured <= targetLufs - gateBelowTargetDb)
            return false;
        if (measured <= recentPeakLufs - gateBelowPeakDb)
            return false;
        if (error > 0.0 && limiterReductionDb >= limiterGateDb)
            return false;
        return true;
    }

    double sampleRate = 48000.0;
    Mode mode = Mode::off;
    double targetLufs = -9.0;
    double correctionDb = 0.0;
    double analysisTimeSec = 0.0;
    bool analysisDone = false;
    bool hasRecentPeak = false;
    double recentPeakLufs = 0.0;
};

} // namespace finale::dsp
