#include "Plugin/PluginProcessor.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

// Mede o custo do processBlock inteiro, em % do tempo real de cada bloco (100 % = um núcleo inteiro).
// Média = custo contínuo; p99 = blocos lentos. O máximo não é impresso: no Windows ele é dominado
// por preempção do sistema e muda muito de uma execução para outra.

namespace
{

void setParam (finale::FinaleProcessor& p, const char* id, float value)
{
    auto* param = p.parameters.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

struct Config
{
    const char* name;
    int character;
    bool compare;
    int assistant;
};

} // namespace

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const Config configs[] { { "Clean", 0, false, 0 }, { "DOOM", 4, false, 0 }, { "Clean + A/B", 0, true, 0 },
        { "Clean + Auto", 0, false, 2 }, { "DOOM + A/B + Auto", 4, true, 2 } };
    constexpr double seconds = 10.0;
    constexpr int sourceBlocks = 64; // ruído pré-gerado: gerar fora do cronômetro

    std::printf ("taxa      bloco  configuração        média   p99   (%% do tempo real do bloco)\n");

    for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
        for (int blockSize : { 64, 512 })
            for (const auto& config : configs)
            {
                finale::FinaleProcessor p;
                setParam (p, finale::ParamIDs::amount, 100.0f);
                setParam (p, finale::ParamIDs::character, static_cast<float> (config.character));
                setParam (p, finale::ParamIDs::compare, config.compare ? 1.0f : 0.0f);
                setParam (p, finale::ParamIDs::assistant, static_cast<float> (config.assistant));
                p.prepareToPlay (sampleRate, blockSize);

                std::mt19937 rng (1);
                std::normal_distribution<float> noise (0.0f, 0.2f);
                std::vector<float> source (static_cast<std::size_t> (2 * blockSize * sourceBlocks));
                for (auto& s : source)
                    s = noise (rng);

                juce::AudioBuffer<float> buffer (2, blockSize);
                juce::MidiBuffer midi;
                const int blocks = static_cast<int> (seconds * sampleRate / blockSize);
                std::vector<double> times;
                times.reserve (static_cast<std::size_t> (blocks));

                for (int b = 0; b < blocks; ++b)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        std::copy_n (source.data() + ((b % sourceBlocks) * 2 + ch) * blockSize, blockSize,
                            buffer.getWritePointer (ch));

                    const auto start = std::chrono::steady_clock::now();
                    p.processBlock (buffer, midi);
                    const auto end = std::chrono::steady_clock::now();
                    times.push_back (std::chrono::duration<double> (end - start).count());
                }

                double total = 0.0;
                for (double t : times)
                    total += t;
                std::sort (times.begin(), times.end());

                const double blockSeconds = blockSize / sampleRate;
                const double mean = 100.0 * total / (blocks * blockSeconds);
                const double p99 = 100.0 * times[static_cast<std::size_t> (blocks * 0.99)] / blockSeconds;
                std::printf (
                    "%6.0f Hz  %4d   %-18s %5.2f %%  %5.1f %%\n", sampleRate, blockSize, config.name, mean, p99);
            }

    return 0;
}
