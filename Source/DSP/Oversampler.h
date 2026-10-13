#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace finale::dsp
{

// Oversampler 2x com meia banda: FIR de 63 taps em fase linear, janela de Blackman
// sobre sinc, corte em fs_base/2 na taxa 2x. Os coeficientes são gerados, não
// tabelados. Ganho DC unitário nas duas direções.
// Fonte: Oppenheim & Schafer, "Discrete-Time Signal Processing", projeto de FIR por janelamento.
//
// Ida e volta: atraso puro de `latency` amostras da taxa base.
// Sem alocação em upsample/downsample: o histórico é alocado só em prepare().
class HalfbandOversampler
{
public:
    static constexpr int taps = 63;
    // (taps - 1) / 2 amostras na taxa 2x por filtro, dois filtros: 31 amostras base.
    static constexpr int latency = 31;

    HalfbandOversampler()
    {
        constexpr double pi = 3.14159265358979323846;

        std::array<double, taps> h { };
        double sum = 0.0;
        for (int k = 0; k < taps; ++k)
        {
            const double x = (k - 31) / 2.0;
            const double sinc = x == 0.0 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double blackman
                = 0.42 - 0.5 * std::cos (2.0 * pi * k / (taps - 1)) + 0.08 * std::cos (4.0 * pi * k / (taps - 1));
            h[static_cast<std::size_t> (k)] = 0.5 * sinc * blackman;
            sum += h[static_cast<std::size_t> (k)];
        }

        for (std::size_t k = 0; k < taps; ++k)
        {
            h[k] /= sum;
            downCoefficients[k] = h[k];
            // O zero inserido no upsample reduz o ganho pela metade: o 2 compensa.
            upCoefficients[k] = 2.0 * h[k];
        }
    }

    void prepare (int numChannels)
    {
        const auto channels = static_cast<std::size_t> (numChannels);
        upHistory.assign (channels * taps, 0.0);
        downHistory.assign (channels * taps, 0.0);
        upWriteIndex.assign (channels, 0);
        downWriteIndex.assign (channels, 0);
    }

    // Uma amostra base vira duas amostras 2x: x entra primeiro (y0), depois um zero (y1).
    void upsample (int channel, double x, double& y0, double& y1)
    {
        // NaN/infinito viram silêncio antes de entrar no histórico.
        const double input = std::isfinite (x) ? x : 0.0;

        push (upHistory, upWriteIndex, channel, input);
        y0 = convolve (upHistory, upWriteIndex, channel, upCoefficients);

        push (upHistory, upWriteIndex, channel, 0.0);
        y1 = convolve (upHistory, upWriteIndex, channel, upCoefficients);
    }

    // Duas amostras 2x viram uma amostra base: a saída é tirada depois de y0 (não de y1),
    // o que mantém a ida e volta em atraso inteiro de 31 amostras.
    double downsample (int channel, double y0, double y1)
    {
        push (downHistory, downWriteIndex, channel, y0);
        const double out = convolve (downHistory, downWriteIndex, channel, downCoefficients);
        push (downHistory, downWriteIndex, channel, y1);
        return out;
    }

private:
    static void push (std::vector<double>& ring, std::vector<int>& writeIndex, int channel, double sample)
    {
        const auto ch = static_cast<std::size_t> (channel);
        int& w = writeIndex[ch];
        w = (w + 1) % taps;
        ring[ch * taps + static_cast<std::size_t> (w)] = sample;
    }

    static double convolve (const std::vector<double>& ring, const std::vector<int>& writeIndex, int channel,
        const std::array<double, taps>& coefficients)
    {
        const auto ch = static_cast<std::size_t> (channel);
        const double* const history = ring.data() + ch * taps;
        const int w = writeIndex[ch];

        double acc = 0.0;
        for (int j = 0; j < taps; ++j)
        {
            int idx = w - j;
            if (idx < 0)
                idx += taps;

            acc += history[idx] * coefficients[static_cast<std::size_t> (j)];
        }
        return acc;
    }

    std::array<double, taps> upCoefficients { };
    std::array<double, taps> downCoefficients { };
    std::vector<double> upHistory; // `taps` amostras por canal, em anel
    std::vector<double> downHistory;
    std::vector<int> upWriteIndex; // posição mais recente de cada canal
    std::vector<int> downWriteIndex;
};

} // namespace finale::dsp
