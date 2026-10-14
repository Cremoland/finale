#pragma once

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace finale::dsp
{

// Alargador estéreo em mid/side. A codificação m = (l+r)/2 e s = (l−r)/2 é invertível
// exatamente: l = m + s, r = m − s (técnica M/S atribuída a Blumlein, patente de 1931). Como o mid
// passa intacto, o centro (voz, bumbo, baixo) nunca muda; só o side é reescalado.
// O side é dividido em grave e agudo por um passa-alta de 120 Hz (RBJ Audio EQ Cookbook,
// Q = 1/√2, Butterworth). O agudo recebe o ganho w; o grave sobe até 1 e cai a zero em
// w = 2, ficando mono. Assim o grave nunca alarga a imagem, que é o que os masters
// costumam precisar.
class StereoWidth
{
public:
    void setWidth (float width)
    {
        targetWidth = std::isfinite (width) ? std::clamp (static_cast<double> (width), 0.0, 2.0) : 1.0;
    }

    void prepare (double sampleRate)
    {
        // shortcut: corte fixo em 120 Hz, sem parâmetro para o usuário.
        highPass = makeHighPass (sampleRate, 120.0, std::numbers::sqrt2 / 2.0);
        highPassState = { };

        // Suavização de 20 ms, começando no alvo atual para não haver rampa ao carregar.
        smoothing = std::exp (-1.0 / (0.02 * sampleRate));
        smoothedWidth = targetWidth;
    }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        // Mono ou sem canais: nada a fazer, o sinal passa como está.
        if (numChannels < 2)
            return;

        float* left = channels[0];
        float* right = channels[1];

        for (int i = 0; i < numSamples; ++i)
        {
            smoothedWidth = targetWidth + smoothing * (smoothedWidth - targetWidth);

            // Um NaN ou infinito na entrada vira silêncio antes de entrar no filtro, senão
            // contamina o estado do passa-alta para sempre.
            const double l = std::isfinite (left[i]) ? static_cast<double> (left[i]) : 0.0;
            const double r = std::isfinite (right[i]) ? static_cast<double> (right[i]) : 0.0;

            const double m = (l + r) / 2.0;
            const double s = (l - r) / 2.0;

            const double sHigh = highPassState.process (highPass, s);
            const double sLow = s - sHigh;

            // shortcut: mapeamento linear do ganho do grave (sobe até 1 e cai a 0). Sem
            // curva de loudness, então a largura percebida não é perfeitamente uniforme.
            const double lowGain = smoothedWidth <= 1.0 ? smoothedWidth : 2.0 - smoothedWidth;
            const double highGain = smoothedWidth;

            const double s2 = lowGain * sLow + highGain * sHigh;

            left[i] = static_cast<float> (m + s2);
            right[i] = static_cast<float> (m - s2);
        }
    }

private:
    double targetWidth = 1.0;
    double smoothedWidth = 1.0;
    double smoothing = 0.0;
    BiquadCoefficients highPass { };
    BiquadState highPassState { };
};

} // namespace finale::dsp
