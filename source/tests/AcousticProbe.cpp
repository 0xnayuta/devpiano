#include "AcousticProbe.h"

#include "Audio/BuiltinSynthesiser.h"
#include "Export/WavExportOptions.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"
#include "Recording/WavFileExporter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <vector>

namespace devpiano::test {
namespace {

using devpiano::audio::BuiltinSynthesiser;
using devpiano::audio::kPianoVoiceCount;
using devpiano::exporting::WavExportOptions;
using devpiano::recording::PerformanceEventType;
using devpiano::recording::RecordingEventSource;
using devpiano::recording::RecordingTake;

constexpr double kProbeSampleRate = 48000.0;
constexpr double kDefaultDurationSeconds = 2.0;
constexpr int kDefaultBlockSize = 512;
constexpr int kDefaultBitsPerSample = 24;

// Phase 40 representative keys for the low/high velocity scan. The exact
// set is a protocol choice; keep it stable so historical reports remain
// comparable.
constexpr int kRepresentativeKeys[16] = { 21, 24, 28, 33, 36, 40, 45, 48, 52, 57, 60, 69, 72, 84, 96, 108 };

struct ProbeCase {
    int midi = 60;
    int velocity = 70;
};

struct ProbeSettings {
    juce::File outputDirectory;
    double sampleRate = kProbeSampleRate;
    double durationSeconds = kDefaultDurationSeconds;
    double tailSeconds = 0.0;
    int blockSize = kDefaultBlockSize;
    int bitsPerSample = kDefaultBitsPerSample;
    bool benchmark = false;
    std::vector<int> keys;
    std::vector<int> velocities;
};

std::vector<int> parseIntegerList(const juce::String& text, int minimum, int maximum) {
    std::vector<int> parsed;
    for (auto token : juce::StringArray::fromTokens(text, ",", "")) {
        token = token.trim();
        if (token.isEmpty()) {
            continue;
        }
        const auto range = juce::StringArray::fromTokens(token, "-", "");
        if (range.size() == 2) {
            auto first = range[0].getIntValue();
            auto last = range[1].getIntValue();
            if (first > last) {
                std::swap(first, last);
            }
            for (auto value = first; value <= last; ++value) {
                parsed.push_back(value);
            }
        } else {
            parsed.push_back(token.getIntValue());
        }
    }

    std::vector<int> unique;
    for (const auto value : parsed) {
        if (value < minimum || value > maximum) {
            return {};
        }
        if (std::find(unique.begin(), unique.end(), value) == unique.end()) {
            unique.push_back(value);
        }
    }
    return unique;
}

void printUsage() {
    std::cout << "Usage: devpiano_tests --acoustic-probe [options]\n"
              << "\n"
              << "Renders deterministic single-note 48 kHz/24-bit WAV cases through\n"
              << "exportTakeAsWavFile and writes manifest.json for tools/acoustic_probe.py.\n"
              << "\n"
              << "Options:\n"
              << "  --out <dir>          Output directory (default: ./acoustic-probe-out)\n"
              << "  --keys <list>        MIDI notes, e.g. 60,64,67 or 21-108 (default: phase40 set)\n"
              << "  --velocities <list>  MIDI velocities, e.g. 41,70,98 (default: phase40 set)\n"
              << "  --duration <sec>     Held-note duration (default: 2.0)\n"
              << "  --tail <sec>         Extra post-take tail (default: 0.0)\n"
              << "  --block <samples>    Render block size (default: 512)\n"
              << "  --benchmark          Add a 12-voice synthetic block-time benchmark\n"
              << "  --help               Show this help\n"
              << "\n"
              << "Default phase40 set: 88 keys at v70 plus 16 representative keys at v41/v98.\n";
}

[[nodiscard]] RecordingTake makeTake(int midi, int velocity, double sampleRate, double durationSeconds) {
    RecordingTake take;
    take.sampleRate = sampleRate;
    take.lengthSamples = static_cast<std::int64_t>(std::llround(durationSeconds * sampleRate));
    take.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                            juce::MidiMessage::noteOn(1, midi, static_cast<juce::uint8>(velocity)) });
    take.events.push_back({ take.lengthSamples, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                            juce::MidiMessage::noteOff(1, midi) });
    return take;
}

[[nodiscard]] WavExportOptions makeOptions(const ProbeSettings& settings) {
    WavExportOptions options;
    options.sampleRate = settings.sampleRate;
    options.numChannels = 2;
    options.blockSize = settings.blockSize;
    options.bitsPerSample = settings.bitsPerSample;
    options.masterGain = 1.0f;
    options.adsr = { 0.01f, 0.20f, 0.80f, 0.30f };
    options.builtinTone = SettingsModel::BuiltinTone::piano;
    options.pianoBrightness = 0.5f;
    options.pianoHammerHardness = 0.5f;
    options.hammerVoicing = devpiano::audio::kCalibratedHammerVoicing;
    options.pianoResonance = 0.5f;
    options.temperament = devpiano::audio::Temperament::equal;
    options.referencePitchA4 = 440.0;
    options.soundPerspective = devpiano::audio::SoundPerspective::player;
    options.reverbSpace = devpiano::audio::ReverbSpace::chamber;
    options.reverbWet = 0.0f;
    options.lidPosition = SettingsModel::LidPosition::fullOpen;
    options.pedalNoiseLevel = 0.0f;
    options.feltAgeingAmount = 0.0f;
    options.stretchTuningEnabled = true;
    options.duplexResonance = 0.15f;
    options.layers = {};
    options.tailSeconds = settings.tailSeconds;
    return options;
}

[[nodiscard]] juce::var makeBenchmark(const ProbeSettings& settings, const WavExportOptions& options) {
    constexpr int kBenchmarkPolyphony = 12;
    constexpr double kBenchmarkSeconds = 5.0;

    BuiltinSynthesiser synth;
    devpiano::recording::initialiseOfflinePianoSynth(synth, options, kPianoVoiceCount);

    juce::AudioBuffer<float> buffer(2, settings.blockSize);
    juce::MidiBuffer noteOnBuffer;
    for (int index = 0; index < kBenchmarkPolyphony; ++index) {
        noteOnBuffer.addEvent(juce::MidiMessage::noteOn(1, 48 + index * 2, 0.85f), 0);
    }

    const auto blocks = static_cast<int>(
        std::ceil(kBenchmarkSeconds * settings.sampleRate / static_cast<double>(settings.blockSize)));
    std::vector<double> blockMilliseconds;
    blockMilliseconds.reserve(static_cast<std::size_t>(blocks));

    for (int block = 0; block < blocks; ++block) {
        buffer.clear();
        juce::MidiBuffer blockMidi;
        if (block == 0) {
            blockMidi = noteOnBuffer;
        }
        const auto start = std::chrono::steady_clock::now();
        synth.renderNextBlock(buffer, blockMidi, 0, settings.blockSize);
        const auto end = std::chrono::steady_clock::now();
        blockMilliseconds.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    synth.allNotesOff(0, false);

    auto sorted = blockMilliseconds;
    std::sort(sorted.begin(), sorted.end());
    const auto percentile = [&sorted](double fraction) {
        if (sorted.empty()) {
            return 0.0;
        }
        const auto index
            = std::min(sorted.size() - 1, static_cast<std::size_t>(fraction * static_cast<double>(sorted.size())));
        return sorted[index];
    };
    const auto mean = std::accumulate(blockMilliseconds.begin(), blockMilliseconds.end(), 0.0)
        / static_cast<double>(std::max<std::size_t>(1, blockMilliseconds.size()));

    auto* result = new juce::DynamicObject();
    result->setProperty("polyphony", kBenchmarkPolyphony);
    result->setProperty("blockSize", settings.blockSize);
    result->setProperty("seconds", kBenchmarkSeconds);
    result->setProperty("blocks", blocks);
    result->setProperty("meanMs", mean);
    result->setProperty("p50Ms", percentile(0.50));
    result->setProperty("p99Ms", percentile(0.99));
    result->setProperty("maxMs", sorted.empty() ? 0.0 : sorted.back());
    result->setProperty("note", "Debug measurements are not a Release SLA.");
    return juce::var(result);
}

} // namespace

int runAcousticProbe(const juce::StringArray& args) {
    ProbeSettings settings;
    settings.outputDirectory = juce::File::getCurrentWorkingDirectory().getChildFile("acoustic-probe-out");

    juce::String keysText;
    juce::String velocitiesText;

    // args[0] is the executable and args[1] is the --acoustic-probe mode flag.
    auto firstOption = 1;
    if (args.size() > 1 && args[1] == "--acoustic-probe") {
        firstOption = 2;
    }

    for (auto index = firstOption; index < args.size(); ++index) {
        const auto argument = args[index];
        const auto nextValue = [&]() -> juce::String {
            if (index + 1 >= args.size()) {
                return {};
            }
            return args[++index];
        };

        if (argument == "--help" || argument == "-h") {
            printUsage();
            return 0;
        }
        if (argument == "--out") {
            const auto value = nextValue();
            if (value.isEmpty()) {
                std::cerr << "--out requires a directory\n";
                return 1;
            }
            settings.outputDirectory = juce::File(value);
        } else if (argument == "--keys") {
            keysText = nextValue();
        } else if (argument == "--velocities") {
            velocitiesText = nextValue();
        } else if (argument == "--duration") {
            settings.durationSeconds = nextValue().getDoubleValue();
        } else if (argument == "--tail") {
            settings.tailSeconds = nextValue().getDoubleValue();
        } else if (argument == "--block") {
            settings.blockSize = nextValue().getIntValue();
        } else if (argument == "--benchmark") {
            settings.benchmark = true;
        } else if (argument == "--protocol") {
            const auto protocol = nextValue();
            if (protocol != "phase40") {
                std::cerr << "unsupported protocol: " << protocol << " (expected phase40)\n";
                return 1;
            }
        } else {
            std::cerr << "unknown argument: " << argument << "\n";
            printUsage();
            return 1;
        }
    }

    if (settings.sampleRate != kProbeSampleRate) {
        std::cerr << "47-1 currently fixes the probe sample rate at 48000 Hz\n";
        return 1;
    }
    if (settings.durationSeconds < 1.4) {
        std::cerr << "--duration must be at least 1.4 s for the 1.1-1.3 s window\n";
        return 1;
    }
    if (settings.blockSize <= 0 || settings.bitsPerSample != kDefaultBitsPerSample) {
        std::cerr << "invalid block size or bit depth\n";
        return 1;
    }

    if (!keysText.isEmpty()) {
        settings.keys = parseIntegerList(keysText, 21, 108);
        if (settings.keys.empty()) {
            std::cerr << "invalid --keys list\n";
            return 1;
        }
    }
    if (!velocitiesText.isEmpty()) {
        settings.velocities = parseIntegerList(velocitiesText, 1, 127);
        if (settings.velocities.empty()) {
            std::cerr << "invalid --velocities list\n";
            return 1;
        }
    }

    std::vector<ProbeCase> cases;
    if (settings.keys.empty() && settings.velocities.empty()) {
        for (auto midi = 21; midi <= 108; ++midi) {
            cases.push_back({ midi, 70 });
        }
        for (const auto midi : kRepresentativeKeys) {
            cases.push_back({ midi, 41 });
            cases.push_back({ midi, 98 });
        }
    } else {
        if (settings.keys.empty()) {
            for (auto midi = 21; midi <= 108; ++midi) {
                settings.keys.push_back(midi);
            }
        }
        if (settings.velocities.empty()) {
            settings.velocities.push_back(70);
        }
        for (const auto midi : settings.keys) {
            for (const auto velocity : settings.velocities) {
                cases.push_back({ midi, velocity });
            }
        }
    }

    if (!settings.outputDirectory.createDirectory()) {
        std::cerr << "cannot create output directory: " << settings.outputDirectory.getFullPathName() << "\n";
        return 1;
    }

    const auto options = makeOptions(settings);
    if (!devpiano::recording::hasUsableRenderOptions(options)) {
        std::cerr << "probe options rejected by the production export validator\n";
        return 1;
    }

    juce::Array<juce::var> renderedCases;
    std::cout << "Rendering " << cases.size() << " case(s) to " << settings.outputDirectory.getFullPathName() << "\n";

    for (const auto& probeCase : cases) {
        const auto take = makeTake(probeCase.midi, probeCase.velocity, settings.sampleRate, settings.durationSeconds);
        const auto fileName = "case_" + juce::String(probeCase.midi).paddedLeft('0', 3) + "_v"
            + juce::String(probeCase.velocity) + ".wav";
        const auto file = settings.outputDirectory.getChildFile(fileName);

        const auto start = std::chrono::steady_clock::now();
        if (!devpiano::exporting::exportTakeAsWavFile(take, file, options)) {
            std::cerr << "render failed for midi=" << probeCase.midi << " velocity=" << probeCase.velocity << "\n";
            return 1;
        }
        const auto end = std::chrono::steady_clock::now();
        const auto exportMs = std::chrono::duration<double, std::milli>(end - start).count();

        auto* entry = new juce::DynamicObject();
        entry->setProperty("midi", probeCase.midi);
        entry->setProperty("velocity", probeCase.velocity);
        entry->setProperty("file", fileName);
        entry->setProperty("exportMs", exportMs);
        renderedCases.add(juce::var(entry));

        std::cout << "  midi=" << probeCase.midi << " v=" << probeCase.velocity << " " << juce::String(exportMs, 1)
                  << " ms\n";
    }

    auto* manifest = new juce::DynamicObject();
    manifest->setProperty("protocol", "phase40");
    manifest->setProperty("sampleRate", settings.sampleRate);
    manifest->setProperty("bitsPerSample", settings.bitsPerSample);
    manifest->setProperty("blockSize", settings.blockSize);
    manifest->setProperty("durationSeconds", settings.durationSeconds);
    manifest->setProperty("tailSeconds", settings.tailSeconds);
    manifest->setProperty("caseCount", static_cast<int>(renderedCases.size()));
    manifest->setProperty("cases", juce::var(renderedCases));
    if (settings.benchmark) {
        manifest->setProperty("benchmark", makeBenchmark(settings, options));
    }

    const auto manifestFile = settings.outputDirectory.getChildFile("manifest.json");
    if (!manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifest), true))) {
        std::cerr << "cannot write manifest: " << manifestFile.getFullPathName() << "\n";
        return 1;
    }

    std::cout << "Wrote " << manifestFile.getFullPathName() << "\n";
    return 0;
}

} // namespace devpiano::test