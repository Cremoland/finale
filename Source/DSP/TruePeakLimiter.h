#pragma once

#include "DSP/TruePeakDetector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace finale::dsp
{

// Limiter de true peak com lookahead, seguindo o limiter de lookahead do
// Signalsmith Audio ("Designing a straightforward limiter"). O ganho é calculado
// antes de o pico chegar à saída, então a redução já está pronta quando ele passa.
//
// Toda a memória é alocada em prepare(). process() e setCeilingDecibels() não
// alocam, então são seguros para o audio thread.
class TruePeakLimiter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        numPreparedChannels = std::max (0, numChannels);

        // 2 ms de lookahead: o ganho baixa antes de o pico chegar à saída.
        lookahead = std::max (1, static_cast<int> (std::round (0.002 * sampleRate)));
        latencySamples = TruePeakDetector::latency + lookahead - 1;

        // Release de 100 ms: o ganho volta a 1 em constante de tempo de 100 ms.
        releaseCoef = static_cast<float> (1.0 - std::exp (-1.0 / (0.1 * sampleRate)));

        detector.prepare (numPreparedChannels);

        // Linha de atraso do sinal, um anel por canal. O ganho é único entre
        // canais, então todos avançam juntos e compartilham a mesma posição.
        delayLines.assign (
            static_cast<std::size_t> (numPreparedChannels) * static_cast<std::size_t> (latencySamples), 0.0f);
        delayPos = 0;

        // Teto atrasado junto com o áudio: a amostra que sai é limitada pelo teto que
        // estava vigente quando ela entrou. Sem isso, baixar o teto corta as amostras
        // que ainda estão no lookahead e o corte vira clipe duro. Começa com o teto atual.
        ceilingDelay.assign (static_cast<std::size_t> (latencySamples), ceilingLin);

        // Mínimo deslizante começa com uma entrada de valor 1 no instante -1, que
        // representa a janela inicial cheia de unidades: o ganho começa em 1.
        minTimes.assign (static_cast<std::size_t> (lookahead) + 1, 0);
        minValues.assign (static_cast<std::size_t> (lookahead) + 1, 1.0f);
        minHead = 0;
        minCount = 1;
        minTimes[0] = -1;
        sampleIndex = 0;

        prevReleased = 1.0f;

        // Média móvel também começa cheia de 1, com soma igual a L.
        averageRing.assign (static_cast<std::size_t> (lookahead), 1.0f);
        averagePos = 0;
        averageSum = static_cast<double> (lookahead);
    }

    void setCeilingDecibels (float db) { ceilingLin = std::pow (10.0f, db / 20.0f); }

    // numChannels acima do valor de prepare() não são tocados.
    void process (float* const* channels, int numChannels, int numSamples)
    {
        const int active = std::min (numChannels, numPreparedChannels);
        if (active <= 0 || numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            // O pico de todos os canais decide o ganho, já que ele é linkado.
            float peak = 0.0f;
            for (int ch = 0; ch < active; ++ch)
                peak = std::max (peak, detector.push (ch, sanitize (channels[ch][i])));

            const float required = peak > ceilingLin ? ceilingLin / peak : 1.0f;
            const float held = slidingMinimum (required);

            // Ataque instantâneo (held < prev) e release exponencial de volta a 1.
            // Como coef <= 1, released nunca passa de held.
            const float released = held < prevReleased ? held : prevReleased + (held - prevReleased) * releaseCoef;
            prevReleased = released;

            const float gain = movingAverage (released);

            // Mesmo atraso do áudio: lê o teto de quando a amostra que sai entrou e grava o atual.
            float& ceilingSlot = ceilingDelay[static_cast<std::size_t> (delayPos)];
            const float ceilingOut = ceilingSlot;
            ceilingSlot = ceilingLin;

            for (int ch = 0; ch < active; ++ch)
            {
                const float x = sanitize (channels[ch][i]);
                float& slot = delayLines[static_cast<std::size_t> (ch * latencySamples + delayPos)];
                const float delayed = slot;
                slot = x;

                // O clamp é rede de segurança: o ganho já respeita o teto, mas o
                // true peak medido pelo detector é uma aproximação.
                channels[ch][i] = std::clamp (delayed * gain, -ceilingOut, ceilingOut);
            }

            delayPos = (delayPos + 1) % latencySamples;
        }
    }

    int getLatencySamples() const { return latencySamples; }

private:
    // Entrada não-finita vira silêncio, igual ao detector: senão contaminaria a saída.
    static float sanitize (float x) { return std::isfinite (x) ? x : 0.0f; }

    // Mínimo dos últimos L+1 valores. Deque monotônica (valores crescentes da
    // frente para trás) sobre um anel de capacidade L+1, O(1) amortizado.
    float slidingMinimum (float value)
    {
        const std::int64_t now = sampleIndex++;
        const int capacity = lookahead + 1;

        // Antes de inserir, descarta quem saiu da janela [now - L, now]. Assim a
        // deque nunca passa de L+1 entradas.
        while (minCount > 0 && minTimes[static_cast<std::size_t> (minHead)] <= now - capacity)
        {
            minHead = (minHead + 1) % capacity;
            --minCount;
        }

        // Quem é maior ou igual ao novo valor nunca mais será o mínimo.
        while (minCount > 0)
        {
            const int back = (minHead + minCount - 1) % capacity;
            if (minValues[static_cast<std::size_t> (back)] < value)
                break;
            --minCount;
        }

        const int tail = (minHead + minCount) % capacity;
        minTimes[static_cast<std::size_t> (tail)] = now;
        minValues[static_cast<std::size_t> (tail)] = value;
        ++minCount;

        return minValues[static_cast<std::size_t> (minHead)];
    }

    // Média móvel de L valores. A soma é double para que o erro de arredondamento
    // não se acumule ao longo do tempo.
    float movingAverage (float value)
    {
        float& oldest = averageRing[static_cast<std::size_t> (averagePos)];
        averageSum += static_cast<double> (value) - static_cast<double> (oldest);
        oldest = value;
        averagePos = (averagePos + 1) % lookahead;

        return static_cast<float> (averageSum / static_cast<double> (lookahead));
    }

    TruePeakDetector detector;
    int numPreparedChannels = 0;
    float ceilingLin = 1.0f; // 0 dBTP até setCeilingDecibels() ser chamado
    float releaseCoef = 0.0f;
    int lookahead = 1; // L
    int latencySamples = 0; // Dd + L - 1

    std::vector<float> delayLines;
    std::vector<float> ceilingDelay; // um teto por instante, no mesmo anel de delayPos
    int delayPos = 0;

    std::vector<std::int64_t> minTimes;
    std::vector<float> minValues;
    int minHead = 0;
    int minCount = 0;
    std::int64_t sampleIndex = 0;

    float prevReleased = 1.0f;

    std::vector<float> averageRing;
    int averagePos = 0;
    double averageSum = 0.0;
};

} // namespace finale::dsp
