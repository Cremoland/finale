#pragma once

#include "DSP/LoudnessMeter.h"
#include "DSP/TruePeakLimiter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace finale::dsp
{

// A/B do masterizador. O lado "processado" (wet) fica como está. O lado "original"
// (dry, atrasado para bater com a cadeia e igualado em loudness pelo ganho de
// igualação) entra por um crossfade. O limiter do lado original roda sempre, para
// não haver transitório quando o usuário liga o A/B.
//
// Toda a memória é alocada em prepare(). process() não aloca, então é seguro no
// audio thread.
class AbCompare
{
public:
    AbCompare() = default;

    // scratchPtrs aponta para o próprio vetor scratch: copiar o objeto deixaria a
    // cópia apontando para a memória da original.
    AbCompare (const AbCompare&) = delete;
    AbCompare& operator= (const AbCompare&) = delete;

    // true toca o original. Pode ser chamado antes de prepare().
    void setCompare (bool original) { compareTarget = original ? 1.0f : 0.0f; }

    // Ganho de igualação em dB, limitado a ±24 dB. Valor não-finito é ignorado e
    // mantém o alvo anterior.
    void setMatchGainDecibels (float db)
    {
        // shortcut: ±24 dB é um limite de segurança escolhido à mão, não derivado do DSP.
        if (std::isfinite (db))
            matchTargetDb = std::clamp (db, -24.0f, 24.0f);
    }

    // Iguala o original ao processado pelos LUFS de curto prazo de cada lado. Abaixo do
    // piso (silêncio) ou não-finito, o alvo anterior é mantido: o medidor não tem o que
    // comparar, e zerar o ganho daria um salto ao voltar o sinal.
    void setMatchFromLufs (float inputLufs, float outputLufs)
    {
        if (std::isfinite (inputLufs) && std::isfinite (outputLufs) && inputLufs > LoudnessMeter::floorLufs
            && outputLufs > LoudnessMeter::floorLufs)
            setMatchGainDecibels (outputLufs - inputLufs);
    }

    void setCeilingDecibels (float db)
    {
        ceilingDb = db;
        limiter.setCeilingDecibels (db);
    }

    void prepare (double sampleRate, int numChannels, int dryDelaySamples, int maxBlockSize)
    {
        numPrepared = std::max (0, numChannels);
        blockCapacity = std::max (0, maxBlockSize);
        delaySamples = std::max (0, dryDelaySamples);

        // O teto precisa estar no limiter antes do prepare, que o usa para encher o
        // atraso interno com o valor certo desde a primeira amostra.
        limiter.setCeilingDecibels (ceilingDb);
        limiter.prepare (sampleRate, numPrepared);

        // shortcut: 20 ms é o crossfade da troca, rápido o bastante para o usuário
        // comparar e lento o bastante para não estalar.
        crossfadeCoef = static_cast<float> (std::exp (-1.0 / (0.02 * sampleRate)));
        // shortcut: 200 ms para a igualação, para o ganho não acompanhar o áudio.
        gainCoef = static_cast<float> (std::exp (-1.0 / (0.2 * sampleRate)));

        // Suavizadores começam no alvo: nada de rampa ao preparar.
        crossfade = compareTarget;
        matchDb = matchTargetDb;
        linearGain = std::pow (10.0f, matchDb / 20.0f);
        linearGainDb = matchDb;

        delayLine.assign (static_cast<std::size_t> (numPrepared) * static_cast<std::size_t> (delaySamples), 0.0f);
        delayPos = 0;

        scratch.assign (static_cast<std::size_t> (numPrepared) * static_cast<std::size_t> (blockCapacity), 0.0f);
        scratchPtrs.resize (static_cast<std::size_t> (numPrepared));
        for (std::size_t ch = 0; ch < scratchPtrs.size(); ++ch)
            scratchPtrs[ch] = scratch.data() + ch * static_cast<std::size_t> (blockCapacity);

        fade.assign (static_cast<std::size_t> (blockCapacity), 0.0f);
    }

    // numChannels acima do valor de prepare() não são tocados. numSamples pode ser
    // maior que maxBlockSize: o trabalho é quebrado em pedaços de maxBlockSize, para
    // que o atraso do original e os suavizadores andem amostra a amostra sem pular.
    void process (const float* const* dry, float* const* wet, int numChannels, int numSamples)
    {
        const int active = std::min (numChannels, numPrepared);
        if (active <= 0 || blockCapacity <= 0 || numSamples <= 0)
            return;

        for (int offset = 0; offset < numSamples; offset += blockCapacity)
        {
            const int length = std::min (blockCapacity, numSamples - offset);
            processChunk (dry, wet, active, offset, length);
        }
    }

private:
    // Processa `length` amostras começando em `offset`. length <= blockCapacity.
    void processChunk (const float* const* dry, float* const* wet, int active, int offset, int length)
    {
        const auto block = static_cast<std::size_t> (blockCapacity);
        const auto delayLen = static_cast<std::size_t> (delaySamples);

        // Uma passada por amostra: a e g avançam uma vez e valem para todos os canais.
        for (int n = 0; n < length; ++n)
        {
            // O one-pole nunca chega ao alvo. Encaixar em 1e-6 faz a mistura voltar exata
            // (a == 0 pula a conta) e tira o pow do laço depois que o ganho assenta.
            crossfade = compareTarget + (crossfade - compareTarget) * crossfadeCoef;
            if (std::abs (crossfade - compareTarget) < snapEpsilon)
                crossfade = compareTarget;
            matchDb = matchTargetDb + (matchDb - matchTargetDb) * gainCoef;
            if (std::abs (matchDb - matchTargetDb) < snapEpsilon)
                matchDb = matchTargetDb;
            fade[static_cast<std::size_t> (n)] = crossfade;

            if (matchDb != linearGainDb)
            {
                linearGain = std::pow (10.0f, matchDb / 20.0f);
                linearGainDb = matchDb;
            }
            const float gain = linearGain;

            for (int ch = 0; ch < active; ++ch)
            {
                const auto c = static_cast<std::size_t> (ch);
                const float x = finiteOrZero (dry[ch][offset + n]);

                // Linha de atraso por canal, com uma posição compartilhada.
                float delayed = x;
                if (delayLen > 0)
                {
                    float& slot = delayLine[c * delayLen + static_cast<std::size_t> (delayPos)];
                    delayed = slot;
                    slot = x;
                }

                scratch[c * block + static_cast<std::size_t> (n)] = delayed * gain;
            }

            if (delaySamples > 0)
                delayPos = (delayPos + 1) % delaySamples;
        }

        // O limiter roda mesmo com compare desligado: o lado A não tem transitório.
        limiter.process (scratchPtrs.data(), active, length);

        for (int ch = 0; ch < active; ++ch)
        {
            const auto c = static_cast<std::size_t> (ch);
            for (int n = 0; n < length; ++n)
            {
                const auto i = static_cast<std::size_t> (n);
                const float a = fade[i];
                const float w = finiteOrZero (wet[ch][offset + n]);

                // Nas pontas do crossfade não há conta: fora do original, o processado
                // passa como está (só não-finito vira zero); dentro, o original puro.
                if (a == 0.0f)
                    wet[ch][offset + n] = w;
                else if (a == 1.0f)
                    wet[ch][offset + n] = scratch[c * block + i];
                else
                    wet[ch][offset + n] = w + a * (scratch[c * block + i] - w);
            }
        }
    }

    // Entrada não-finita vira silêncio, senão contaminaria a saída.
    static float finiteOrZero (float x) { return std::isfinite (x) ? x : 0.0f; }

    float compareTarget = 0.0f;
    float matchTargetDb = 0.0f;
    float ceilingDb = 0.0f;

    // Estado dos suavizadores, avançado uma vez por amostra.
    float crossfade = 0.0f;
    float matchDb = 0.0f;
    float crossfadeCoef = 0.0f;
    float gainCoef = 0.0f;

    // Ganho linear em cache: recalculado só quando matchDb muda.
    float linearGain = 1.0f;
    float linearGainDb = 0.0f;

    // Distância abaixo da qual o suavizador é considerado assentado no alvo.
    static constexpr float snapEpsilon = 1e-6f;

    int numPrepared = 0;
    int blockCapacity = 0;
    int delaySamples = 0;
    int delayPos = 0;

    TruePeakLimiter limiter;

    std::vector<float> delayLine; // numPrepared * delaySamples
    std::vector<float> scratch; // numPrepared * blockCapacity
    std::vector<float*> scratchPtrs; // ponteiros para cada canal de scratch
    std::vector<float> fade; // crossfade por amostra do bloco atual
};

} // namespace finale::dsp
