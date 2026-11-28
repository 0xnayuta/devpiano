#include "AcousticProbe.h"

#include "Audio/BuiltinSynthesiser.h"
#include "Audio/PianoSynthVoice.h"
#include "Audio/RoomReverbEngine.h"
#include "Export/WavExportOptions.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"
#include "Recording/WavFileExporter.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <numeric>
#include <vector>

namespace devpiano::test {
namespace {

using devpiano::audio::BuiltinSynthesiser;
using devpiano::audio::kDefaultPianoVoiceCount;
using devpiano::exporting::WavExportOptions;
using devpiano::recording::PerformanceEventType;
using devpiano::recording::RecordingEventSource;
using devpiano::recording::RecordingTake;

constexpr double kProbeSampleRate = 48000.0;
constexpr double kDefaultDurationSeconds = 2.0;
constexpr double kMaxDurationSeconds = 30.0;
constexpr int kDefaultBlockSize = 512;
constexpr int kMaxBlockSize = 8192;
constexpr int kDefaultBitsPerSample = 24;

// Phase 40 representative keys for the low/high velocity scan. The exact
// set is a protocol choice; keep it stable so historical reports remain
// comparable.
constexpr int kRepresentativeKeys[16] = { 21, 24, 28, 33, 36, 40, 45, 48, 52, 57, 60, 69, 72, 84, 96, 108 };

// 45-1 same-note retrigger protocol: eight C4 strikes at v41/v70/v98,
// using the 30/60/120/200 ms interval cycle.
constexpr int kRetriggerNote = 60;
constexpr int kRetriggerVelocities[3] = { 41, 70, 98 };
constexpr int kRetriggerIntervalsMs[8] = { 30, 60, 120, 200, 30, 60, 120, 200 };

// 45-2 phase experiment defaults: C2-C8 octave anchors at v41/v70/v98.
constexpr int kPhaseExperimentNotes[7] = { 36, 48, 60, 72, 84, 96, 108 };
constexpr int kPhaseExperimentVelocities[3] = { 41, 70, 98 };

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
    bool benchmarkOnly = false;
    bool retrigger = false;
    bool chord = false;
    int pianoVoiceCount = kDefaultPianoVoiceCount;
    juce::File phaseExperimentFile;
    std::vector<int> keys;
    std::vector<int> velocities;

    // Phase 46-4 benchmark matrix. The production voice count stays at
    // kDefaultPianoVoiceCount; these values only configure the development probe.
    int benchmarkPolyphony = kDefaultPianoVoiceCount;
    std::vector<int> benchmarkNotes;
    bool benchmarkNotesSpecified = false;
    bool benchmarkPolyphonySpecified = false;
    bool bassStress = false;
    double benchmarkSeconds = 5.0;
    int benchmarkWarmupBlocks = -1; // -1 selects one second at the probe rate.
    int benchmarkRepeats = 1;
    bool benchmarkRoom = false;
    float reverbWet = 0.0f;
};

[[nodiscard]] bool parseStrictInt(const juce::String& text, int& value) {
    const auto trimmed = text.trim();
    if (trimmed.isEmpty()) {
        return false;
    }
    const auto utf8 = trimmed.toStdString();
    const auto result = std::from_chars(utf8.data(), utf8.data() + utf8.size(), value);
    return result.ec == std::errc {} && result.ptr == utf8.data() + utf8.size();
}

std::vector<int> parseIntegerList(const juce::String& text, int minimum, int maximum) {
    std::vector<int> unique;
    for (auto token : juce::StringArray::fromTokens(text, ",", "")) {
        token = token.trim();
        if (token.isEmpty()) {
            continue;
        }
        const auto range = juce::StringArray::fromTokens(token, "-", "");
        if (range.size() == 2) {
            int first = 0;
            int last = 0;
            if (!parseStrictInt(range[0], first) || !parseStrictInt(range[1], last)) {
                return {};
            }
            if (first > last) {
                std::swap(first, last);
            }
            // 先校验端点，再展开，避免误输入的超大范围耗尽内存。
            if (first < minimum || last > maximum) {
                return {};
            }
            for (auto value = first; value <= last; ++value) {
                if (std::find(unique.begin(), unique.end(), value) == unique.end()) {
                    unique.push_back(value);
                }
            }
        } else {
            int value = 0;
            if (!parseStrictInt(token, value) || value < minimum || value > maximum) {
                return {};
            }
            if (std::find(unique.begin(), unique.end(), value) == unique.end()) {
                unique.push_back(value);
            }
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
              << "  --benchmark          Add a synthetic block-time benchmark\n"
              << "  --benchmark-only     Run the benchmark without rendering WAV cases\n"
              << "  --benchmark-polyphony <n>  Benchmark voices (default: 12, max 24)\n"
              << "  --benchmark-notes <list>   Benchmark MIDI notes; count must equal polyphony\n"
              << "  --bass-stress        Use the bass register instead of the mid register\n"
              << "  --benchmark-seconds <s>    Measured seconds per repeat (default: 5.0)\n"
              << "  --benchmark-warmup <blocks> Discard blocks after note-on (default: 1 s)\n"
              << "  --benchmark-repeats <n>    Rebuild and measure n times (default: 1, max 10)\n"
              << "  --benchmark-room     Include RoomReverbEngine in the measured path\n"
              << "  --wet <0..1>         Room wet level for cases and benchmark (default: 0)\n"
              << "  --chord              Render one dense chord from --keys for level comparison\n"
              << "  --polyphony <12|16|20|24> Voice count for --chord (default: 16)\n"
              << "  --retrigger          Render eight C4 strikes at v41/v70/v98 (30/60/120/200 ms)\n"
              << "  --phase-experiment <json> Render 45-2 phase candidates from a JSON file\n"
              << "  --help               Show this help\n"
              << "\n"
              << "Default phase40 set: 88 keys at v70 plus 16 representative keys at v41/v98.\n"
              << "Default benchmark notes: 48+2i (mid) or 21+2i (bass) for i in [0, polyphony).\n";
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

[[nodiscard]] RecordingTake makeChordTake(const std::vector<int>& notes, int velocity, double sampleRate,
                                          double durationSeconds) {
    RecordingTake take;
    take.sampleRate = sampleRate;
    take.lengthSamples = static_cast<std::int64_t>(std::llround(durationSeconds * sampleRate));
    for (const auto midi : notes) {
        take.events.push_back({ 0, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                juce::MidiMessage::noteOn(1, midi, static_cast<juce::uint8>(velocity)) });
    }
    for (const auto midi : notes) {
        take.events.push_back({ take.lengthSamples, PerformanceEventType::midi, 0,
                                RecordingEventSource::computerKeyboard, juce::MidiMessage::noteOff(1, midi) });
    }
    return take;
}

[[nodiscard]] RecordingTake makeRetriggerTake(int midi, int velocity, double sampleRate) {
    constexpr double kPostReleaseSeconds = 0.5;
    RecordingTake take;
    take.sampleRate = sampleRate;

    auto position = std::int64_t { 0 };
    for (const auto intervalMs : kRetriggerIntervalsMs) {
        take.events.push_back({ position, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                                juce::MidiMessage::noteOn(1, midi, static_cast<juce::uint8>(velocity)) });
        position += static_cast<std::int64_t>(std::llround(intervalMs * sampleRate / 1000.0));
    }
    take.events.push_back({ position, PerformanceEventType::midi, 0, RecordingEventSource::computerKeyboard,
                            juce::MidiMessage::noteOff(1, midi) });
    take.lengthSamples = position + static_cast<std::int64_t>(std::llround(kPostReleaseSeconds * sampleRate));
    return take;
}

[[nodiscard]] WavExportOptions makeOptions(const ProbeSettings& settings) {
    WavExportOptions options;
    options.sampleRate = settings.sampleRate;
    options.numChannels = 2;
    options.blockSize = settings.blockSize;
    options.bitsPerSample = settings.bitsPerSample;
    options.pianoVoiceCount = settings.pianoVoiceCount;
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
    options.reverbWet = settings.reverbWet;
    options.lidPosition = SettingsModel::LidPosition::fullOpen;
    options.pedalNoiseLevel = 0.0f;
    options.feltAgeingAmount = 0.0f;
    options.stretchTuningEnabled = true;
    options.duplexResonance = 0.15f;
    options.layers = {};
    options.tailSeconds = settings.tailSeconds;
    return options;
}

[[nodiscard]] std::vector<int> defaultBenchmarkNotes(int polyphony, bool bassStress) {
    std::vector<int> notes;
    notes.reserve(static_cast<std::size_t>(polyphony));
    const auto start = bassStress ? 21 : 48;
    for (int index = 0; index < polyphony; ++index) {
        notes.push_back(start + index * 2);
    }
    return notes;
}

[[nodiscard]] double percentileOf(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) {
        return 0.0;
    }
    const auto index
        = std::min(sorted.size() - 1, static_cast<std::size_t>(fraction * static_cast<double>(sorted.size())));
    return sorted[index];
}

[[nodiscard]] double medianOf(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if (values.size() % 2 == 0) {
        return 0.5 * (values[middle - 1] + values[middle]);
    }
    return values[middle];
}

struct BenchmarkRun {
    double meanMs = 0.0;
    double p50Ms = 0.0;
    double p99Ms = 0.0;
    double maxMs = 0.0;
};

[[nodiscard]] BenchmarkRun measureBenchmarkRun(const ProbeSettings& settings, const WavExportOptions& options,
                                               const std::vector<int>& notes, int warmupBlocks, int blocks) {
    BuiltinSynthesiser synth;
    devpiano::recording::initialiseOfflinePianoSynth(synth, options, settings.benchmarkPolyphony);

    std::unique_ptr<devpiano::audio::RoomReverbEngine> room;
    if (settings.benchmarkRoom) {
        room = std::make_unique<devpiano::audio::RoomReverbEngine>();
        room->setSpace(options.reverbSpace);
        room->setWetLevel(settings.reverbWet, true);
        room->prepare(settings.sampleRate);
    }

    juce::AudioBuffer<float> buffer(2, settings.blockSize);
    const auto processBlock = [&](const juce::MidiBuffer& midi) {
        synth.renderNextBlock(buffer, midi, 0, settings.blockSize);
        if (room != nullptr) {
            room->processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), settings.blockSize);
        }
    };

    const juce::MidiBuffer emptyMidi;
    // Warm the allocator and code paths before any measured block.
    for (int block = 0; block < 8; ++block) {
        buffer.clear();
        processBlock(emptyMidi);
    }

    juce::MidiBuffer noteOnMidi;
    for (const auto midi : notes) {
        noteOnMidi.addEvent(juce::MidiMessage::noteOn(1, midi, 0.85f), 0);
    }
    buffer.clear();
    processBlock(noteOnMidi);

    // Drop the attack transient so the measured window reflects sustained
    // polyphony rather than the first hammer contact.
    for (int block = 0; block < warmupBlocks; ++block) {
        buffer.clear();
        processBlock(emptyMidi);
    }

    std::vector<double> blockMilliseconds;
    blockMilliseconds.reserve(static_cast<std::size_t>(blocks));
    for (int block = 0; block < blocks; ++block) {
        buffer.clear();
        const auto start = std::chrono::steady_clock::now();
        processBlock(emptyMidi);
        const auto end = std::chrono::steady_clock::now();
        blockMilliseconds.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }
    synth.allNotesOff(0, false);

    auto sorted = blockMilliseconds;
    std::sort(sorted.begin(), sorted.end());

    BenchmarkRun run;
    run.meanMs = std::accumulate(blockMilliseconds.begin(), blockMilliseconds.end(), 0.0)
        / static_cast<double>(std::max<std::size_t>(1, blockMilliseconds.size()));
    run.p50Ms = percentileOf(sorted, 0.50);
    run.p99Ms = percentileOf(sorted, 0.99);
    run.maxMs = sorted.empty() ? 0.0 : sorted.back();
    return run;
}

[[nodiscard]] juce::var makeBenchmark(const ProbeSettings& settings, const WavExportOptions& options) {
    const auto notes = settings.benchmarkNotes.empty()
        ? defaultBenchmarkNotes(settings.benchmarkPolyphony, settings.bassStress)
        : settings.benchmarkNotes;
    const auto blocks = static_cast<int>(
        std::ceil(settings.benchmarkSeconds * settings.sampleRate / static_cast<double>(settings.blockSize)));
    const auto defaultWarmup
        = static_cast<int>(std::ceil(settings.sampleRate / static_cast<double>(settings.blockSize)));
    const auto warmupBlocks = settings.benchmarkWarmupBlocks >= 0 ? settings.benchmarkWarmupBlocks : defaultWarmup;

    std::vector<BenchmarkRun> runs;
    runs.reserve(static_cast<std::size_t>(settings.benchmarkRepeats));
    for (int repeat = 0; repeat < settings.benchmarkRepeats; ++repeat) {
        runs.push_back(measureBenchmarkRun(settings, options, notes, warmupBlocks, blocks));
        const auto& run = runs.back();
        std::cout << "  benchmark repeat " << (repeat + 1) << "/" << settings.benchmarkRepeats << ": mean "
                  << juce::String(run.meanMs, 3) << " ms, p50 " << juce::String(run.p50Ms, 3) << " ms, p99 "
                  << juce::String(run.p99Ms, 3) << " ms, max " << juce::String(run.maxMs, 3) << " ms\n";
    }

#if JUCE_DEBUG
    constexpr auto buildType = "Debug";
    constexpr auto buildNote = "Debug measurements are not a Release SLA.";
#else
    constexpr auto buildType = "Release";
    constexpr auto buildNote = "Release measurement; record CPU model and power plan externally.";
#endif

    juce::Array<juce::var> noteValues;
    for (const auto midi : notes) {
        noteValues.add(juce::var(midi));
    }

    juce::Array<juce::var> runValues;
    std::vector<double> means;
    std::vector<double> p50s;
    std::vector<double> p99s;
    std::vector<double> maxes;
    for (const auto& run : runs) {
        means.push_back(run.meanMs);
        p50s.push_back(run.p50Ms);
        p99s.push_back(run.p99Ms);
        maxes.push_back(run.maxMs);

        auto* runObject = new juce::DynamicObject();
        runObject->setProperty("meanMs", run.meanMs);
        runObject->setProperty("p50Ms", run.p50Ms);
        runObject->setProperty("p99Ms", run.p99Ms);
        runObject->setProperty("maxMs", run.maxMs);
        runValues.add(juce::var(runObject));
    }

    auto* summary = new juce::DynamicObject();
    summary->setProperty("meanMs", medianOf(means));
    summary->setProperty("p50Ms", medianOf(p50s));
    summary->setProperty("p99Ms", medianOf(p99s));
    summary->setProperty("maxMs", maxes.empty() ? 0.0 : *std::max_element(maxes.begin(), maxes.end()));

    auto* result = new juce::DynamicObject();
    result->setProperty("protocol", "phase46-4");
    result->setProperty("buildType", buildType);
    result->setProperty("polyphony", settings.benchmarkPolyphony);
    result->setProperty("notes", juce::var(noteValues));
    result->setProperty("blockSize", settings.blockSize);
    result->setProperty("sampleRate", settings.sampleRate);
    result->setProperty("seconds", settings.benchmarkSeconds);
    result->setProperty("blocks", blocks);
    result->setProperty("warmupBlocks", warmupBlocks);
    result->setProperty("repeats", settings.benchmarkRepeats);
    result->setProperty("room", settings.benchmarkRoom);
    result->setProperty("reverbWet", settings.reverbWet);
    result->setProperty("runs", juce::var(runValues));
    result->setProperty("summary", juce::var(summary));
    result->setProperty("note", buildNote);
    return { result };
}

struct PhaseCandidate {
    juce::String name;
    std::array<std::array<float, 64>, 3> rows {};
    int rowCount = 1;
};

[[nodiscard]] bool parsePhaseCandidates(const juce::File& file, std::vector<PhaseCandidate>& candidates) {
    if (!file.existsAsFile()) {
        std::cerr << "phase candidate file not found: " << file.getFullPathName() << "\n";
        return false;
    }

    const auto parsed = juce::JSON::parse(file.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    if (root == nullptr) {
        std::cerr << "phase candidate JSON must be an object\n";
        return false;
    }
    const auto list = root->getProperty("candidates");
    auto* array = list.getArray();
    if (array == nullptr) {
        std::cerr << "phase candidate JSON needs a candidates array\n";
        return false;
    }

    for (const auto& item : *array) {
        auto* object = item.getDynamicObject();
        if (object == nullptr) {
            std::cerr << "candidate entries must be objects\n";
            return false;
        }

        PhaseCandidate candidate;
        candidate.name = object->getProperty("name").toString();
        const auto rowsVar = object->getProperty("rows");
        auto* rows = rowsVar.getArray();
        if (rows == nullptr || rows->isEmpty() || rows->size() > 3) {
            std::cerr << "candidate rows must contain 1..3 rows\n";
            return false;
        }
        candidate.rowCount = rows->size();
        for (auto rowIndex = 0; rowIndex < candidate.rowCount; ++rowIndex) {
            auto* row = rows->getReference(rowIndex).getArray();
            if (row == nullptr || row->size() != 64) {
                std::cerr << "each candidate row must contain 64 phases\n";
                return false;
            }
            for (auto phaseIndex = 0; phaseIndex < 64; ++phaseIndex) {
                candidate.rows[static_cast<std::size_t>(rowIndex)][static_cast<std::size_t>(phaseIndex)]
                    = static_cast<float>(static_cast<double>(row->getReference(phaseIndex)));
            }
        }
        candidates.push_back(candidate);
    }

    return !candidates.empty();
}

[[nodiscard]] bool renderPhaseWave(const PhaseCandidate& candidate, int midi, int velocity,
                                   const ProbeSettings& settings, const juce::File& file) {
    WavExportOptions options = makeOptions(settings);
    options.tailSeconds = 0.0;

    BuiltinSynthesiser synth;
    devpiano::recording::initialiseOfflinePianoSynth(synth, options, kDefaultPianoVoiceCount);
    for (auto index = 0; index < synth.getNumVoices(); ++index) {
        if (auto* voice = dynamic_cast<::PianoSynthVoice*>(synth.getVoice(index))) {
            voice->setPhaseTableForExperiment(candidate.rows[0].data(), candidate.rowCount);
        }
    }

    const auto totalSamples = static_cast<int>(std::llround(0.5 * settings.sampleRate));
    const auto releaseSample = totalSamples / 2;
    juce::AudioBuffer<float> full(2, totalSamples);
    full.clear();
    juce::AudioBuffer<float> block(2, settings.blockSize);

    for (auto blockStart = 0; blockStart < totalSamples; blockStart += settings.blockSize) {
        const auto numSamples = std::min(settings.blockSize, totalSamples - blockStart);
        block.setSize(2, numSamples, false, false, true);
        block.clear();

        juce::MidiBuffer midiBuffer;
        if (blockStart == 0) {
            midiBuffer.addEvent(
                juce::MidiMessage::noteOn(1, midi, static_cast<juce::uint8>(std::clamp(velocity, 1, 127))), 0);
        }
        if (releaseSample >= blockStart && releaseSample < blockStart + numSamples) {
            midiBuffer.addEvent(juce::MidiMessage::noteOff(1, midi), releaseSample - blockStart);
        }

        synth.renderNextBlock(block, midiBuffer, 0, numSamples);
        for (auto channel = 0; channel < 2; ++channel) {
            full.copyFrom(channel, blockStart, block, channel, 0, numSamples);
        }
    }

    file.getParentDirectory().createDirectory();
    // 先写临时文件再原子替换：FileOutputStream 打开已存在文件时会 seek 到末尾，
    // 直接写目标会让重跑的 WAV 叠加上一轮数据。
    juce::TemporaryFile temporaryFile(file);
    {
        auto fileStream = std::make_unique<juce::FileOutputStream>(temporaryFile.getFile());
        if (!fileStream->openedOk()) {
            std::cerr << "cannot open phase experiment WAV: " << temporaryFile.getFile().getFullPathName() << "\n";
            return false;
        }
        std::unique_ptr<juce::OutputStream> outputStream = std::move(fileStream);
        juce::WavAudioFormat wavFormat;
        auto writerOptions = juce::AudioFormatWriterOptions()
                                 .withSampleRate(settings.sampleRate)
                                 .withNumChannels(2)
                                 .withBitsPerSample(settings.bitsPerSample);
        auto writer = wavFormat.createWriterFor(outputStream, writerOptions);
        if (writer == nullptr) {
            std::cerr << "cannot create phase experiment WAV writer\n";
            return false;
        }
        if (!writer->writeFromAudioSampleBuffer(full, 0, totalSamples)) {
            std::cerr << "cannot write phase experiment WAV: " << file.getFullPathName() << "\n";
            return false;
        }
    }
    if (!temporaryFile.overwriteTargetFileWithTemporary()) {
        std::cerr << "cannot replace phase experiment WAV: " << file.getFullPathName() << "\n";
        return false;
    }
    return true;
}

int runPhaseExperiment(const ProbeSettings& settings) {
    std::vector<PhaseCandidate> candidates;
    if (!parsePhaseCandidates(settings.phaseExperimentFile, candidates)) {
        return 1;
    }

    std::vector<ProbeCase> cases;
    const auto notes
        = settings.keys.empty() ? std::vector<int>(kPhaseExperimentNotes, kPhaseExperimentNotes + 7) : settings.keys;
    const auto velocities = settings.velocities.empty()
        ? std::vector<int>(kPhaseExperimentVelocities, kPhaseExperimentVelocities + 3)
        : settings.velocities;
    for (const auto midi : notes) {
        for (const auto velocity : velocities) {
            cases.push_back({ midi, velocity });
        }
    }

    if (!settings.outputDirectory.createDirectory()) {
        std::cerr << "cannot create output directory: " << settings.outputDirectory.getFullPathName() << "\n";
        return 1;
    }

    juce::Array<juce::var> rendered;
    std::cout << "Rendering " << candidates.size() << " phase candidate(s) x " << cases.size() << " case(s) to "
              << settings.outputDirectory.getFullPathName() << "\n";

    for (std::size_t candidateIndex = 0; candidateIndex < candidates.size(); ++candidateIndex) {
        const auto& candidate = candidates[candidateIndex];
        const auto candidateLabel
            = candidate.name.isNotEmpty() ? candidate.name : "candidate" + juce::String(candidateIndex);
        for (const auto& probeCase : cases) {
            const auto fileName = "phase_" + candidateLabel + "_" + juce::String(probeCase.midi).paddedLeft('0', 3)
                + "_v" + juce::String(probeCase.velocity) + ".wav";
            const auto file = settings.outputDirectory.getChildFile(fileName);
            if (!renderPhaseWave(candidate, probeCase.midi, probeCase.velocity, settings, file)) {
                return 1;
            }

            auto* entry = new juce::DynamicObject();
            entry->setProperty("candidate", candidateLabel);
            entry->setProperty("rows", candidate.rowCount);
            entry->setProperty("midi", probeCase.midi);
            entry->setProperty("velocity", probeCase.velocity);
            entry->setProperty("file", fileName);
            rendered.add(juce::var(entry));
        }
    }

    auto* manifest = new juce::DynamicObject();
    manifest->setProperty("protocol", "phase45-2");
    manifest->setProperty("sampleRate", settings.sampleRate);
    manifest->setProperty("bitsPerSample", settings.bitsPerSample);
    manifest->setProperty("reference", "Salamander Grand Piano V3, Alexander Holm, CC BY 3.0");
    manifest->setProperty("render", "BuiltinSynthesiser direct render, 0.5 s, note off at 0.25 s, no Room reverb");
    manifest->setProperty("cases", juce::var(rendered));
    const auto manifestFile = settings.outputDirectory.getChildFile("phase_experiment_manifest.json");
    if (!manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifest), true))) {
        std::cerr << "cannot write phase experiment manifest\n";
        return 1;
    }

    std::cout << "Wrote " << manifestFile.getFullPathName() << "\n";
    return 0;
}

int runChordProbe(const ProbeSettings& settings) {
    auto notes = settings.keys;
    if (notes.empty()) {
        for (auto midi = 21; midi <= 44; ++midi) {
            notes.push_back(midi);
        }
    }
    if (!settings.outputDirectory.createDirectory()) {
        std::cerr << "cannot create output directory: " << settings.outputDirectory.getFullPathName() << "\n";
        return 1;
    }

    const auto options = makeOptions(settings);
    const auto take = makeChordTake(notes, 70, settings.sampleRate, settings.durationSeconds);
    const auto fileName = "chord_poly" + juce::String(settings.pianoVoiceCount) + "_v70.wav";
    const auto file = settings.outputDirectory.getChildFile(fileName);

    const auto start = std::chrono::steady_clock::now();
    if (!devpiano::exporting::exportTakeAsWavFile(take, file, options)) {
        std::cerr << "chord render failed\n";
        return 1;
    }
    const auto end = std::chrono::steady_clock::now();
    const auto exportMs = std::chrono::duration<double, std::milli>(end - start).count();

    juce::Array<juce::var> noteValues;
    for (const auto midi : notes) {
        noteValues.add(juce::var(midi));
    }

    auto* manifest = new juce::DynamicObject();
    manifest->setProperty("protocol", "phase48-chord");
    manifest->setProperty("polyphony", settings.pianoVoiceCount);
    manifest->setProperty("notes", juce::var(noteValues));
    manifest->setProperty("sampleRate", settings.sampleRate);
    manifest->setProperty("bitsPerSample", settings.bitsPerSample);
    manifest->setProperty("durationSeconds", settings.durationSeconds);
    manifest->setProperty("tailSeconds", settings.tailSeconds);
    manifest->setProperty("file", fileName);
    manifest->setProperty("exportMs", exportMs);
    const auto manifestFile = settings.outputDirectory.getChildFile("manifest.json");
    if (!manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifest), true))) {
        std::cerr << "cannot write chord manifest\n";
        return 1;
    }

    std::cout << "Rendered chord with " << settings.pianoVoiceCount << " voices and " << notes.size() << " notes to "
              << file.getFullPathName() << "\n";
    return 0;
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

    bool benchmarkOptionUsed = false;
    for (auto index = firstOption; index < args.size(); ++index) {
        const auto& argument = args[index];
        const auto nextValue = [&]() -> juce::String {
            if (index + 1 >= args.size()) {
                return {};
            }
            return args[++index];
        };
        const auto nextRequiredValue = [&](const char* optionName) -> juce::String {
            const auto value = nextValue();
            if (value.isEmpty()) {
                std::cerr << optionName << " requires a value\n";
            }
            return value;
        };
        const auto parseDoubleOption = [&](const char* optionName, double& target) {
            const auto value = nextRequiredValue(optionName);
            const auto parsed = value.getDoubleValue();
            if (value.isEmpty() || !std::isfinite(parsed)) {
                std::cerr << optionName << " requires a finite number\n";
                return false;
            }
            target = parsed;
            return true;
        };
        const auto parseIntegerOption = [&](const char* optionName, int& target) {
            const auto value = nextRequiredValue(optionName);
            int parsed = 0;
            if (!parseStrictInt(value, parsed)) {
                std::cerr << optionName << " requires an integer\n";
                return false;
            }
            target = parsed;
            return true;
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
            if (!parseDoubleOption("--duration", settings.durationSeconds)) {
                return 1;
            }
        } else if (argument == "--tail") {
            if (!parseDoubleOption("--tail", settings.tailSeconds)) {
                return 1;
            }
        } else if (argument == "--block") {
            if (!parseIntegerOption("--block", settings.blockSize)) {
                return 1;
            }
        } else if (argument == "--benchmark") {
            settings.benchmark = true;
        } else if (argument == "--benchmark-only") {
            settings.benchmark = true;
            settings.benchmarkOnly = true;
        } else if (argument == "--chord") {
            settings.chord = true;
        } else if (argument == "--polyphony") {
            const auto value = nextRequiredValue("--polyphony");
            if (value.isEmpty()) {
                return 1;
            }
            settings.pianoVoiceCount = value.getIntValue();
        } else if (argument == "--benchmark-polyphony") {
            if (!parseIntegerOption("--benchmark-polyphony", settings.benchmarkPolyphony)) {
                return 1;
            }
            settings.benchmarkPolyphonySpecified = true;
            benchmarkOptionUsed = true;
        } else if (argument == "--benchmark-notes") {
            const auto value = nextRequiredValue("--benchmark-notes");
            if (value.isEmpty()) {
                return 1;
            }
            settings.benchmarkNotes = parseIntegerList(value, 21, 108);
            if (settings.benchmarkNotes.empty()) {
                std::cerr << "invalid --benchmark-notes list\n";
                return 1;
            }
            settings.benchmarkNotesSpecified = true;
            benchmarkOptionUsed = true;
        } else if (argument == "--bass-stress") {
            settings.bassStress = true;
            benchmarkOptionUsed = true;
        } else if (argument == "--benchmark-seconds") {
            if (!parseDoubleOption("--benchmark-seconds", settings.benchmarkSeconds)) {
                return 1;
            }
            benchmarkOptionUsed = true;
        } else if (argument == "--benchmark-warmup") {
            if (!parseIntegerOption("--benchmark-warmup", settings.benchmarkWarmupBlocks)) {
                return 1;
            }
            benchmarkOptionUsed = true;
        } else if (argument == "--benchmark-repeats") {
            if (!parseIntegerOption("--benchmark-repeats", settings.benchmarkRepeats)) {
                return 1;
            }
            benchmarkOptionUsed = true;
        } else if (argument == "--benchmark-room") {
            settings.benchmarkRoom = true;
            benchmarkOptionUsed = true;
        } else if (argument == "--wet") {
            double wet = 0.0;
            if (!parseDoubleOption("--wet", wet)) {
                return 1;
            }
            settings.reverbWet = static_cast<float>(wet);
        } else if (argument == "--retrigger") {
            settings.retrigger = true;
        } else if (argument == "--phase-experiment") {
            const auto value = nextValue();
            if (value.isEmpty()) {
                std::cerr << "--phase-experiment requires a JSON file\n";
                return 1;
            }
            settings.phaseExperimentFile = juce::File(value);
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
    if (!settings.benchmarkOnly && !settings.retrigger && !settings.chord
        && settings.phaseExperimentFile == juce::File() && settings.durationSeconds < 1.4) {
        std::cerr << "--duration must be at least 1.4 s for the 1.1-1.3 s window\n";
        return 1;
    }
    if (settings.blockSize <= 0 || settings.blockSize > kMaxBlockSize) {
        std::cerr << "--block must be between 1 and " << kMaxBlockSize << "\n";
        return 1;
    }
    if (settings.bitsPerSample != kDefaultBitsPerSample) {
        std::cerr << "invalid bit depth\n";
        return 1;
    }
    if (settings.durationSeconds > kMaxDurationSeconds) {
        std::cerr << "--duration must not exceed 30 s\n";
        return 1;
    }
    if (settings.tailSeconds < 0.0 || settings.tailSeconds > kMaxDurationSeconds) {
        std::cerr << "--tail must be between 0 and 30 s\n";
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

    if (!devpiano::audio::isSupportedPianoVoiceCount(settings.pianoVoiceCount)) {
        std::cerr << "--polyphony must be one of 12/16/20/24\n";
        return 1;
    }
    if (settings.chord
        && (settings.retrigger || settings.benchmarkOnly || settings.phaseExperimentFile != juce::File())) {
        std::cerr << "--chord cannot be combined with --retrigger, --benchmark-only or --phase-experiment\n";
        return 1;
    }
    if (benchmarkOptionUsed && !settings.benchmark) {
        std::cerr << "benchmark options require --benchmark or --benchmark-only\n";
        return 1;
    }
    if (settings.benchmarkOnly && (settings.retrigger || settings.phaseExperimentFile != juce::File())) {
        std::cerr << "--benchmark-only cannot be combined with --retrigger or --phase-experiment\n";
        return 1;
    }
    if (settings.benchmarkNotesSpecified && settings.bassStress) {
        std::cerr << "--benchmark-notes and --bass-stress are mutually exclusive\n";
        return 1;
    }
    if (settings.benchmarkNotesSpecified && !settings.benchmarkPolyphonySpecified) {
        settings.benchmarkPolyphony = static_cast<int>(settings.benchmarkNotes.size());
    }
    if (settings.benchmarkPolyphony < 1 || settings.benchmarkPolyphony > 24) {
        std::cerr << "--benchmark-polyphony must be between 1 and 24\n";
        return 1;
    }
    if (settings.benchmarkSeconds <= 0.0 || settings.benchmarkSeconds > 30.0) {
        std::cerr << "--benchmark-seconds must be between 0 and 30\n";
        return 1;
    }
    if (settings.benchmarkWarmupBlocks < -1 || settings.benchmarkWarmupBlocks > 10000) {
        std::cerr << "--benchmark-warmup must be between 0 and 10000 blocks\n";
        return 1;
    }
    if (settings.benchmarkRepeats < 1 || settings.benchmarkRepeats > 10) {
        std::cerr << "--benchmark-repeats must be between 1 and 10\n";
        return 1;
    }
    if (settings.reverbWet < 0.0f || settings.reverbWet > 1.0f) {
        std::cerr << "--wet must be between 0 and 1\n";
        return 1;
    }
    if (settings.benchmarkRoom && settings.reverbWet <= 0.0f) {
        std::cerr << "--benchmark-room requires --wet > 0\n";
        return 1;
    }
    if (settings.benchmarkNotes.empty()) {
        settings.benchmarkNotes = defaultBenchmarkNotes(settings.benchmarkPolyphony, settings.bassStress);
    }
    if (settings.benchmarkNotes.size() != static_cast<std::size_t>(settings.benchmarkPolyphony)) {
        std::cerr << "--benchmark-notes count must equal --benchmark-polyphony\n";
        return 1;
    }

    if (settings.chord) {
        return runChordProbe(settings);
    }

    if (settings.phaseExperimentFile != juce::File()) {
        return runPhaseExperiment(settings);
    }

    std::vector<ProbeCase> cases;
    if (!settings.benchmarkOnly) {
        if (settings.retrigger) {
            for (const auto velocity : kRetriggerVelocities) {
                cases.push_back({ kRetriggerNote, velocity });
            }
        } else if (settings.keys.empty() && settings.velocities.empty()) {
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
    if (settings.benchmarkOnly) {
        std::cout << "Benchmark-only: skipping WAV cases\n";
    } else {
        std::cout << "Rendering " << cases.size() << " case(s) to " << settings.outputDirectory.getFullPathName()
                  << "\n";

        for (const auto& probeCase : cases) {
            const auto take = settings.retrigger
                ? makeRetriggerTake(probeCase.midi, probeCase.velocity, settings.sampleRate)
                : makeTake(probeCase.midi, probeCase.velocity, settings.sampleRate, settings.durationSeconds);
            const auto fileName = settings.retrigger ? "retrigger_" + juce::String(probeCase.midi).paddedLeft('0', 3)
                    + "_v" + juce::String(probeCase.velocity) + ".wav"
                                                     : "case_" + juce::String(probeCase.midi).paddedLeft('0', 3) + "_v"
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
    }

    auto* manifest = new juce::DynamicObject();
    if (settings.benchmarkOnly) {
        manifest->setProperty("protocol", "phase46-4");
    } else if (settings.retrigger) {
        manifest->setProperty("protocol", "phase45-retrigger");
    } else {
        manifest->setProperty("protocol", "phase40");
    }
    manifest->setProperty("benchmarkOnly", settings.benchmarkOnly);
    manifest->setProperty("retrigger", settings.retrigger);
    if (settings.retrigger) {
        manifest->setProperty("retriggerNote", kRetriggerNote);
        manifest->setProperty("strikeCount", static_cast<int>(std::size(kRetriggerIntervalsMs)));
        juce::Array<juce::var> intervals;
        for (const auto intervalMs : kRetriggerIntervalsMs) {
            intervals.add(intervalMs);
        }
        manifest->setProperty("strikeIntervalsMs", juce::var(intervals));
    }
    manifest->setProperty("sampleRate", settings.sampleRate);
    manifest->setProperty("bitsPerSample", settings.bitsPerSample);
    manifest->setProperty("blockSize", settings.blockSize);
    manifest->setProperty("durationSeconds", settings.durationSeconds);
    manifest->setProperty("tailSeconds", settings.tailSeconds);
    manifest->setProperty("caseCount", renderedCases.size());
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