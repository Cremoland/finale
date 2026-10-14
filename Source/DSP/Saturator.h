#pragma once

#include "Oversampler.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace finale::dsp
{

// Saturador tanh com oversampling 2x. A curva tanh(g·x)/g é waveshaping estático: a
// não-linearidade gera harmônicos que, sem oversampling, rebatem como aliasing. Rodar
// a curva na taxa 2x e filtrar na volta empurra esse aliasing para fora da banda.
// Fonte: Zölzer, "DAFX" (2ª ed.), cap. 4, "Nonlinear Processing" (tanh como soft clipper).
// A divisão por g faz sinal pequeno passar com ganho unitário; o drive só muda a dureza.
// Com 2x, drive alto (DOOM) em agudos acima de ~12 kHz ainda dobra harmônicos para dentro da banda; 4x resolveria.
class Saturator
{
public:
    static constexpr int latency = HalfbandOversampler::latency;

    void prepare (double sampleRate, int numChannels)
    {
        preparedChannels = std::max (0, numChannels);
        oversampler.prepare (preparedChannels);

        // Suavizadores de 20 ms. Começam nos alvos padrão (drive 0 dB, mix 0), não nos valores do
        // plugin: o plugin seta os seus no primeiro bloco, então há uma rampa de ~100 ms ao carregar (sem estalo).
        smoothing = std::exp (-1.0 / (0.02 * sampleRate));
        driveDb = targetDriveDb;
        mix = targetMix;

        // Linha de atraso do seco: `latency` amostras por canal, uma posição de leitura comum.
        delayLine.assign (static_cast<std::size_t> (preparedChannels) * latency, 0.0);
        delayIndex = 0;
    }

    void setDriveDecibels (float db)
    {
        // shortcut: teto de 48 dB de drive; o DOOM usa 30.
        targetDriveDb = std::isfinite (db) ? std::clamp (static_cast<double> (db), 0.0, 48.0) : 0.0;
    }

    void setMix (float value)
    {
        targetMix = std::isfinite (value) ? std::clamp (static_cast<double> (value), 0.0, 1.0) : 0.0;
    }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        const int active = std::min (numChannels, preparedChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            driveDb = targetDriveDb + smoothing * (driveDb - targetDriveDb);
            mix = targetMix + smoothing * (mix - targetMix);
            const double gain = std::pow (10.0, driveDb / 20.0);

            for (int ch = 0; ch < active; ++ch)
            {
                const auto c = static_cast<std::size_t> (ch);
                float& sample = channels[ch][i];

                // Seco: sai a amostra de `latency` passos atrás; a nova entra no lugar dela.
                // NaN/infinito viram silêncio também aqui, para o seco não contaminar a saída.
                const double input = std::isfinite (sample) ? static_cast<double> (sample) : 0.0;
                double& slot = delayLine[c * latency + static_cast<std::size_t> (delayIndex)];
                const double dry = slot;
                slot = input;

                double u0 = 0.0;
                double u1 = 0.0;
                oversampler.upsample (ch, input, u0, u1);
                const double wet
                    = oversampler.downsample (ch, std::tanh (gain * u0) / gain, std::tanh (gain * u1) / gain);

                sample = static_cast<float> (dry + mix * (wet - dry));
            }

            delayIndex = (delayIndex + 1) % latency;
        }
    }

private:
    HalfbandOversampler oversampler;
    std::vector<double> delayLine;
    int delayIndex = 0;
    int preparedChannels = 0;

    double smoothing = 0.0;
    double targetDriveDb = 0.0;
    double targetMix = 0.0;
    double driveDb = 0.0;
    double mix = 0.0;
};

} // namespace finale::dsp
