// finale_curvegen: mede as referências de cada gênero e regenera a tabela de curvas do plugin.
// Uso: finale_curvegen [pastaDeReferencias] [arquivoDeSaida]
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

#include "DSP/BandAnalyzer.h"
#include "curvegen/CurveGen.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
using finale::dsp::BandAnalyzer;
using finale::tools::currentTable;
using finale::tools::CurveTable;
using finale::tools::genreFolders;
using finale::tools::genreNames;
using Shape = std::array<float, 4>;

// Blocos de 65536 amostras: a memória fica limitada mesmo com arquivos longos.
constexpr int chunkSize = 65536;

// Lê o arquivo inteiro e devolve a forma tonal; nullopt (com aviso) se o arquivo não servir.
std::optional<Shape> measureFile (const juce::File& file, juce::AudioFormatManager& formats)
{
    const juce::String name = file.getFileName();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->numChannels == 0)
    {
        std::cout << "aviso: " << name.toRawUTF8() << " não pôde ser lido; arquivo pulado\n";
        return std::nullopt;
    }

    const int channels = static_cast<int> (reader->numChannels);
    BandAnalyzer analyzer;
    analyzer.prepare (reader->sampleRate);

    juce::AudioBuffer<float> block (channels, chunkSize);
    std::vector<float> mono (chunkSize);
    const juce::int64 total = reader->lengthInSamples;

    for (juce::int64 start = 0; start < total;)
    {
        const int n = static_cast<int> (std::min<juce::int64> (chunkSize, total - start));
        if (!reader->read (block.getArrayOfWritePointers(), channels, start, n))
        {
            std::cout << "aviso: erro ao ler " << name.toRawUTF8() << "; arquivo pulado\n";
            return std::nullopt;
        }

        std::fill (mono.begin(), mono.begin() + n, 0.0f);
        for (int c = 0; c < channels; ++c)
        {
            const float* src = block.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                mono[i] += src[i];
        }
        for (int i = 0; i < n; ++i)
            mono[i] /= static_cast<float> (channels);

        analyzer.process (mono.data(), n);
        start += n;
    }

    const Shape shape = analyzer.getShapeDb();
    const bool finite = std::all_of (shape.begin(), shape.end(), [] (float v) { return std::isfinite (v); });
    const bool isUsableShape = std::any_of (shape.begin(), shape.end(), [] (float v) { return v != 0.0f; });
    if (!finite || !isUsableShape)
    {
        std::cout << "aviso: " << name.toRawUTF8() << " sem energia suficiente; arquivo pulado\n";
        return std::nullopt;
    }
    return shape;
}
} // namespace

int main (int argc, char* argv[])
{
#if defined(_WIN32)
    SetConsoleOutputCP (CP_UTF8);
#endif

    if (argc > 3)
    {
        std::cout << "Uso: finale_curvegen [pastaDeReferencias] [arquivoDeSaida]\n";
        return 1;
    }

    const juce::File cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File referencesDir = cwd.getChildFile (juce::String (argc > 1 ? argv[1] : "references"));
    const juce::File outputFile = cwd.getChildFile (juce::String (argc > 2 ? argv[2] : "Source/DSP/GenreCurveTable.h"));

    if (!referencesDir.isDirectory())
    {
        std::cout << "Pasta de referências não encontrada: " << referencesDir.getFullPathName().toRawUTF8() << "\n";
        return 1;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::array<std::optional<Shape>, 8> shapes;
    std::array<int, 8> fileCounts { };

    for (std::size_t g = 0; g < shapes.size(); ++g)
    {
        const juce::File folder = referencesDir.getChildFile (genreFolders[g]);
        Shape sum { };
        int used = 0;

        if (folder.isDirectory())
        {
            for (const juce::File& file : folder.findChildFiles (juce::File::findFiles, false))
            {
                const juce::String ext = file.getFileExtension().toLowerCase();
                if (ext == ".mp3")
                {
                    std::cout << "aviso: " << file.getFileName().toRawUTF8()
                              << ": MP3 não é suportado; converta para WAV/FLAC. Arquivo pulado\n";
                    continue;
                }
                if (ext != ".wav" && ext != ".aif" && ext != ".aiff" && ext != ".flac")
                    continue;

                const auto shape = measureFile (file, formats);
                if (!shape)
                    continue;

                for (std::size_t b = 0; b < sum.size(); ++b)
                    sum[b] += (*shape)[b];
                ++used;
            }
        }

        fileCounts[g] = used;
        if (used > 0)
        {
            for (float& v : sum)
                v /= static_cast<float> (used);
            shapes[g] = sum;
        }
    }

    const CurveTable current = currentTable();
    const finale::tools::CurveResult result = finale::tools::computeCurves (current, shapes);

    std::cout << "\n" << result.message << "\n\n";
    std::printf ("%-14s %5s %8s %8s %8s %8s  %s\n", "Gênero", "Arq.", "90 Hz", "300 Hz", "3 kHz", "10 kHz", "Situação");
    for (std::size_t g = 0; g < shapes.size(); ++g)
    {
        const char* status
            = !shapes[g] ? "sem referências" : (result.table[g] == current[g] ? "sem mudança" : "atualizada");
        std::printf ("%-14s %5d %8.1f %8.1f %8.1f %8.1f  %s\n", genreNames[g], fileCounts[g], result.table[g][0],
            result.table[g][1], result.table[g][2], result.table[g][3], status);
    }

    if (!result.changed)
        return 0;

    if (!outputFile.getParentDirectory().isDirectory())
    {
        std::cout << "Pasta de saída não existe: " << outputFile.getParentDirectory().getFullPathName().toRawUTF8()
                  << "\n";
        return 1;
    }

    const std::string text = finale::tools::writeCurveTableHeader (result.table);
    if (!outputFile.replaceWithData (text.data(), text.size()))
    {
        std::cout << "Erro ao gravar " << outputFile.getFullPathName().toRawUTF8() << "\n";
        return 1;
    }

    std::cout << "Gravado: " << outputFile.getFullPathName().toRawUTF8() << "\n";
    return 0;
}
