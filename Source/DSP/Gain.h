#pragma once

#include <algorithm>
#include <cmath>

namespace finale::dsp
{

// Ganho com suavização linear, sem alocação: seguro para o audio thread.
class Gain
{
public:
    void prepare (double sampleRate, double rampSeconds = 0.02)
    {
        rampLength = std::max (1, static_cast<int> (sampleRate * rampSeconds));
        current = target;
        remaining = 0;
    }

    void setGainDecibels (float db)
    {
        const float newTarget = std::pow (10.0f, db / 20.0f);
        if (newTarget == target)
            return;

        target = newTarget;
        step = (target - current) / static_cast<float> (rampLength);
        remaining = rampLength;
    }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (remaining > 0)
            {
                current += step;
                if (--remaining == 0)
                    current = target;
            }

            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] *= current;
        }
    }

private:
    float current = 1.0f;
    float target = 1.0f;
    float step = 0.0f;
    int rampLength = 1;
    int remaining = 0;
};

} // namespace finale::dsp
