#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace finale::dsp
{

// Detector de true peak: interpolação 4x inspirada na ITU-R BS.1770-4 (coeficientes
// gerados, não a tabela da norma). FIR de 48 taps com janela de Blackman sobre sinc,
// e cada fase normalizada para somar 1 para que DC passe intacto.
//
// Sem alocação em push(): o histórico é alocado só em prepare().
class TruePeakDetector
{
public:
    // Atraso do ponto interpolado em relação à amostra de entrada.
    static constexpr int latency = 6;

    TruePeakDetector()
    {
        constexpr double pi = 3.14159265358979323846;

        std::array<double, numTaps> h { };
        for (int k = 0; k < numTaps; ++k)
        {
            const double x = (k - 24) / 4.0;
            const double sinc = x == 0.0 ? 1.0 : std::sin (pi * x) / (pi * x);
            const double blackman
                = 0.42 - 0.5 * std::cos (2.0 * pi * k / numTaps) + 0.08 * std::cos (4.0 * pi * k / numTaps);
            h[static_cast<std::size_t> (k)] = sinc * blackman;
        }

        for (int p = 0; p < numPhases; ++p)
        {
            double sum = 0.0;
            for (int j = 0; j < historyLength; ++j)
                sum += h[static_cast<std::size_t> (4 * j + p)];

            for (int j = 0; j < historyLength; ++j)
                coefficients[static_cast<std::size_t> (4 * j + p)]
                    = static_cast<float> (h[static_cast<std::size_t> (4 * j + p)] / sum);
        }
    }

    void prepare (int numChannels)
    {
        history.assign (static_cast<std::size_t> (numChannels) * historyLength, 0.0f);
        writeIndex.assign (static_cast<std::size_t> (numChannels), 0);
    }

    // Recebe uma amostra e devolve o maior |valor| entre os 4 pontos interpolados
    // do intervalo que começa `latency` amostras atrás.
    float push (int channel, float sample)
    {
        float* const ring = history.data() + static_cast<std::size_t> (channel) * historyLength;
        int& w = writeIndex[static_cast<std::size_t> (channel)];

        // NaN/infinito viram silêncio antes de entrar no histórico: senão
        // contaminariam a saída por 12 amostras. O limiter usa a mesma regra.
        w = (w + 1) % historyLength;
        ring[w] = std::isfinite (sample) ? sample : 0.0f;

        float peak = 0.0f;
        for (int p = 0; p < numPhases; ++p)
        {
            float acc = 0.0f;
            for (int j = 0; j < historyLength; ++j)
            {
                int idx = w - j;
                if (idx < 0)
                    idx += historyLength;

                acc += ring[idx] * coefficients[static_cast<std::size_t> (4 * j + p)];
            }

            peak = std::max (peak, std::abs (acc));
        }

        return peak;
    }

private:
    static constexpr int numPhases = 4;
    static constexpr int historyLength = 12;
    static constexpr int numTaps = numPhases * historyLength; // 48

    std::array<float, numTaps> coefficients { };
    std::vector<float> history; // historyLength amostras por canal, em anel
    std::vector<int> writeIndex; // posição mais recente de cada canal
};

} // namespace finale::dsp
