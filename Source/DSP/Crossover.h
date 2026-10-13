#pragma once

#include "Biquad.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace finale::dsp
{

// Crossover Linkwitz-Riley de 4ª ordem em duas frequências (3 bandas). Cada LR4 é
// dois Butterworth de 2ª ordem (Q = 1/√2) em cascata. A banda baixa passa por um
// allpass em f2 que iguala a fase das bandas média e alta; por isso a soma das três
// bandas tem magnitude plana.
class ThreeBandCrossover
{
public:
    // Calcula os coeficientes e aloca/zera o estado. Única parte que aloca.
    void prepare (double sampleRate, int numChannels, double lowMidHz, double midHighHz)
    {
        const double q = std::numbers::sqrt2 / 2.0;

        lowMidLowPass = makeLowPass (sampleRate, lowMidHz, q);
        lowMidHighPass = makeHighPass (sampleRate, lowMidHz, q);
        midHighAllPass = makeAllPass (sampleRate, midHighHz, q);
        midHighLowPass = makeLowPass (sampleRate, midHighHz, q);
        midHighHighPass = makeHighPass (sampleRate, midHighHz, q);

        states.assign (static_cast<std::size_t> (numChannels), ChannelStates { });
    }

    // Entrada não-finita vira silêncio, para um NaN nunca entrar no estado dos filtros.
    void process (int channel, double x, double& low, double& mid, double& high)
    {
        if (!std::isfinite (x))
            x = 0.0;

        ChannelStates& s = states[static_cast<std::size_t> (channel)];

        low = s.lowAllPass.process (
            midHighAllPass, s.lowLowPassB.process (lowMidLowPass, s.lowLowPassA.process (lowMidLowPass, x)));

        const double rest = s.highPassB.process (lowMidHighPass, s.highPassA.process (lowMidHighPass, x));

        mid = s.midLowPassB.process (midHighLowPass, s.midLowPassA.process (midHighLowPass, rest));
        high = s.highHighPassB.process (midHighHighPass, s.highHighPassA.process (midHighHighPass, rest));
    }

private:
    struct ChannelStates
    {
        BiquadState lowLowPassA, lowLowPassB, lowAllPass;
        BiquadState highPassA, highPassB;
        BiquadState midLowPassA, midLowPassB;
        BiquadState highHighPassA, highHighPassB;
    };

    BiquadCoefficients lowMidLowPass { 1.0, 0.0, 0.0, 0.0, 0.0 };
    BiquadCoefficients lowMidHighPass { 1.0, 0.0, 0.0, 0.0, 0.0 };
    BiquadCoefficients midHighAllPass { 1.0, 0.0, 0.0, 0.0, 0.0 };
    BiquadCoefficients midHighLowPass { 1.0, 0.0, 0.0, 0.0, 0.0 };
    BiquadCoefficients midHighHighPass { 1.0, 0.0, 0.0, 0.0, 0.0 };
    std::vector<ChannelStates> states;
};

} // namespace finale::dsp
