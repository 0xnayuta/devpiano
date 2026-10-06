#include "PerformanceFile.h"

#include "../Audio/PerspectiveProcessor.h"
#include "../Audio/RoomReverbEngine.h"
#include "../Audio/TemperamentEngine.h"
#include "Diagnostics/Log.h"
#include "Layout/PerformancePreset.h"
#include "Recording/RecordedPreset.h"
#include "Recording/RecordingEngine.h"
#include "Recording/RenderPipeline.h"
#include "Recording/TimelineValidation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace devpiano::recording {
namespace {

constexpr std::int64_t maxPerformanceFileSizeBytes = 32LL * 1024 * 1024; // 32 MiB
constexpr size_t maxMidiFrameBytes = size_t { 1024 } * 1024; // 1 MiB per frame

[[nodiscard]] std::optional<std::int64_t> parseExactIntegerSample(const juce::var& v) noexcept {
    if (v.isInt() || v.isInt64()) {
        const auto val = static_cast<std::int64_t>(static_cast<juce::int64>(v));
        if (val < 0) {
            return std::nullopt;
        }
        return val;
    }
    if (v.isDouble()) {
        const auto d = static_cast<double>(v);
        if (!std::isfinite(d) || d < 0.0) {
            return std::nullopt;
        }
        if (std::floor(d) != d) {
            return std::nullopt;
        }
        return checkedSampleCount(d);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<double> parseSupportedSampleRate(const juce::var& v) noexcept {
    if (!v.isDouble() && !v.isInt() && !v.isInt64()) {
        return std::nullopt;
    }
    const auto sr = static_cast<double>(v);
    if (!isSupportedTimelineSampleRate(sr)) {
        return std::nullopt;
    }
    return sr;
}

[[nodiscard]] std::optional<juce::DynamicObject::Ptr> parsePerformanceFileRoot(const juce::String& json) {
    if (json.isEmpty() || json.getNumBytesAsUTF8() > maxPerformanceFileSizeBytes) {
        return std::nullopt;
    }

    juce::var parsed;
    // ERR-007：JUCE JSON::parse 不抛异常，用 Result 重载获得行/列错误信息。
    const auto parseResult = juce::JSON::parse(json, parsed);
    if (parseResult.failed()) {
        DP_LOG_WARN("[PerformanceFile] JSON parse failed: " + parseResult.getErrorMessage());
        return std::nullopt;
    }
    if (!parsed.isObject()) {
        return std::nullopt;
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr) {
        return std::nullopt;
    }

    const auto formatVar = root->getProperty(performance_file::keyFormat);
    if (!formatVar.isString() || formatVar.toString() != performance_file::formatIdentifier) {
        return std::nullopt;
    }

    return juce::DynamicObject::Ptr(root);
}

juce::String sourceToString(RecordingEventSource source) {
    switch (source) {
    case RecordingEventSource::computerKeyboard:
        return performance_file::sourceComputerKeyboard;
    case RecordingEventSource::realtimeMidiBuffer:
        return performance_file::sourceRealtimeMidiBuffer;
    case RecordingEventSource::playback:
        return performance_file::sourcePlayback;
    }
    return {};
}

RecordingEventSource stringToSource(const juce::String& str) {
    if (str == performance_file::sourceRealtimeMidiBuffer) {
        return RecordingEventSource::realtimeMidiBuffer;
    }
    if (str == performance_file::sourcePlayback) {
        return RecordingEventSource::playback;
    }
    return RecordingEventSource::computerKeyboard;
}

[[nodiscard]] std::optional<uint8_t> decodeBase64Char(char c) noexcept {
    if (c == '.') {
        return uint8_t { 0 };
    }
    if (c >= 'A' && c <= 'Z') {
        return static_cast<uint8_t>(1 + (c - 'A'));
    }
    if (c >= 'a' && c <= 'z') {
        return static_cast<uint8_t>(27 + (c - 'a'));
    }
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(53 + (c - '0'));
    }
    if (c == '+') {
        return uint8_t { 63 };
    }
    return std::nullopt;
}

[[nodiscard]] bool isValidRawMidiFrame(const uint8_t* data, size_t size) noexcept {
    if (data == nullptr || size == 0 || size > maxMidiFrameBytes) {
        return false;
    }

    const uint8_t status = data[0];
    if (status < 0x80) {
        return false;
    }

    if (status < 0xF0) {
        const uint8_t type = status & 0xF0;
        const size_t expectedLen = (type == 0xC0 || type == 0xD0) ? 2 : 3;
        if (size != expectedLen) {
            return false;
        }
        for (size_t i = 1; i < size; ++i) {
            if (data[i] >= 0x80) {
                return false;
            }
        }
        return true;
    }

    switch (status) {
    case 0xF0: {
        if (size >= 2 && data[size - 1] == 0xF7) {
            for (size_t i = 1; i < size - 1; ++i) {
                if (data[i] >= 0x80) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    case 0xF1:
        return size == 2 && data[1] < 0x80;
    case 0xF2:
        return size == 3 && data[1] < 0x80 && data[2] < 0x80;
    case 0xF3:
        return size == 2 && data[1] < 0x80;
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFD:
    case 0xFE:
        return size == 1;
    case 0xFF: {
        if (size == 1) {
            return true;
        }
        if (size < 3) {
            return false;
        }
        const uint8_t metaType = data[1];
        uint32_t vlqLen = 0;
        size_t vlqBytes = 0;
        bool vlqDone = false;
        for (size_t i = 2; i < size && vlqBytes < 4; ++i) {
            const uint8_t b = data[i];
            vlqLen = (vlqLen << 7) | (b & 0x7F);
            ++vlqBytes;
            if ((b & 0x80) == 0) {
                vlqDone = true;
                break;
            }
        }
        if (!vlqDone || (2 + vlqBytes + vlqLen != size)) {
            return false;
        }

        const uint8_t* payload = data + 2 + vlqBytes;
        switch (metaType) {
        case 0x00:
            if (vlqLen != 0 && vlqLen != 2) {
                return false;
            }
            break;
        case 0x20:
            if (vlqLen != 1 || payload[0] >= 16) {
                return false;
            }
            break;
        case 0x21:
            if (vlqLen != 1) {
                return false;
            }
            break;
        case 0x2F:
            if (vlqLen != 0) {
                return false;
            }
            break;
        case 0x51:
            if (vlqLen != 3) {
                return false;
            }
            break;
        case 0x54:
            if (vlqLen != 5) {
                return false;
            }
            break;
        case 0x58:
            if (vlqLen != 4 || payload[1] > 30) {
                return false;
            }
            break;
        case 0x59:
            if (vlqLen != 2) {
                return false;
            }
            break;
        default:
            break;
        }
        return true;
    }
    default:
        return false;
    }
}

juce::var midiMessageToVar(const juce::MidiMessage& msg) {
    juce::MemoryBlock mb(msg.getRawData(), static_cast<size_t>(msg.getRawDataSize()));
    return { mb.toBase64Encoding() };
}

std::optional<juce::MidiMessage> varToMidiMessage(const juce::var& v, size_t maxAllowedFrameBytes = maxMidiFrameBytes) {
    if (!v.isString()) {
        return std::nullopt;
    }
    const auto str = v.toString();
    if (str.isEmpty()) {
        return std::nullopt;
    }

    const size_t effectiveMaxBytes = std::min(maxMidiFrameBytes, maxAllowedFrameBytes);
    auto cursor = str.getCharPointer();
    size_t decodedSize = 0;
    size_t prefixLength = 0;
    for (auto c = cursor.getAndAdvance(); c != '.'; c = cursor.getAndAdvance()) {
        if (c < '0' || c > '9' || (prefixLength == 0 && c == '0')) {
            return std::nullopt;
        }
        const auto digit = static_cast<size_t>(c - '0');
        if (digit > effectiveMaxBytes || decodedSize > (effectiveMaxBytes - digit) / 10) {
            return std::nullopt;
        }
        decodedSize = decodedSize * 10 + digit;
        ++prefixLength;
    }
    if (prefixLength == 0 || decodedSize == 0 || decodedSize > effectiveMaxBytes) {
        return std::nullopt;
    }

    const size_t expectedChars = (decodedSize * 8 + 5) / 6;
    size_t payloadLength = 0;
    uint8_t lastValue = 0;
    for (auto c = cursor.getAndAdvance(); c != 0; c = cursor.getAndAdvance()) {
        if (c > 127 || payloadLength >= expectedChars) {
            return std::nullopt;
        }
        const auto decoded = decodeBase64Char(static_cast<char>(c));
        if (!decoded.has_value()) {
            return std::nullopt;
        }
        lastValue = *decoded;
        ++payloadLength;
    }
    const auto remainder = decodedSize * 8 % 6;
    if (payloadLength != expectedChars || (remainder != 0 && (lastValue >> remainder) != 0)) {
        return std::nullopt;
    }

    juce::MemoryBlock mb;
    try {
        if (!mb.fromBase64Encoding(str) || mb.getSize() != decodedSize) {
            return std::nullopt;
        }
    } catch (const std::bad_alloc&) {
        return std::nullopt;
    }

    if (!isValidRawMidiFrame(static_cast<const uint8_t*>(mb.getData()), mb.getSize())) {
        return std::nullopt;
    }

    return juce::MidiMessage(mb.getData(), static_cast<int>(mb.getSize()), 0);
}
juce::var acousticSnapshotToVar(const audio::AcousticSnapshot& ac) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("builtinTone", (ac.builtinTone == core::BuiltinTone::piano) ? "piano" : "sine");
    obj->setProperty("masterGain", ac.masterGain);

    {
        juce::DynamicObject::Ptr adsr = new juce::DynamicObject();
        adsr->setProperty("attack", ac.adsr.attack);
        adsr->setProperty("decay", ac.adsr.decay);
        adsr->setProperty("sustain", ac.adsr.sustain);
        adsr->setProperty("release", ac.adsr.release);
        obj->setProperty("adsr", juce::var(adsr.get()));
    }

    obj->setProperty("brightness", ac.brightness);
    obj->setProperty("hammerHardness", ac.hammerHardness);
    obj->setProperty("resonance", ac.resonance);
    obj->setProperty("lidPosition", static_cast<int>(ac.lidPosition));

    const auto tempId = audio::TemperamentEngine::getIdentifier(ac.temperament);
    obj->setProperty("temperament", juce::String(tempId.data(), tempId.size()));
    obj->setProperty("referencePitchA4", ac.referencePitchA4);

    const auto perspId = audio::PerspectiveProcessor::toIdentifier(ac.soundPerspective);
    obj->setProperty("soundPerspective", juce::String(perspId.data(), perspId.size()));

    const auto spaceId = audio::RoomReverbEngine::toIdentifier(ac.reverbSpace);
    obj->setProperty("reverbSpace", juce::String(spaceId.data(), spaceId.size()));

    obj->setProperty("reverbWet", ac.reverbWet);
    obj->setProperty("pedalNoiseLevel", ac.pedalNoiseLevel);
    obj->setProperty("feltAgeingAmount", ac.feltAgeingAmount);
    obj->setProperty("unaCorda", ac.unaCorda);

    obj->setProperty("sustainPolicy", (ac.sustainPolicy == core::SustainPolicy::syncPedal) ? "syncPedal" : "normal");
    obj->setProperty("transposeEnabled", ac.transposeEnabled);
    obj->setProperty("transposeOffset", ac.transposeOffset);
    obj->setProperty("channelFollowKeyMask", static_cast<int>(ac.channelFollowKeyMask));

    return { obj.get() };
}

audio::AcousticSnapshot varToAcousticSnapshot(const juce::var& v) {
    audio::AcousticSnapshot ac;
    if (!v.isObject()) {
        return ac;
    }
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) {
        return ac;
    }

    if (obj->hasProperty("builtinTone")) {
        const auto str = obj->getProperty("builtinTone").toString();
        ac.builtinTone = (str == "sine") ? core::BuiltinTone::sine : core::BuiltinTone::piano;
    }
    if (obj->hasProperty("masterGain")) {
        ac.masterGain = juce::jlimit(0.0f, 4.0f, static_cast<float>(obj->getProperty("masterGain")));
    }
    if (obj->hasProperty("adsr")) {
        const auto adsrVar = obj->getProperty("adsr");
        if (auto* ao = adsrVar.getDynamicObject()) {
            if (ao->hasProperty("attack")) {
                ac.adsr.attack = juce::jlimit(0.001f, 5.0f, static_cast<float>(ao->getProperty("attack")));
            }
            if (ao->hasProperty("decay")) {
                ac.adsr.decay = juce::jlimit(0.001f, 5.0f, static_cast<float>(ao->getProperty("decay")));
            }
            if (ao->hasProperty("sustain")) {
                ac.adsr.sustain = juce::jlimit(0.0f, 1.0f, static_cast<float>(ao->getProperty("sustain")));
            }
            if (ao->hasProperty("release")) {
                ac.adsr.release = juce::jlimit(0.001f, 10.0f, static_cast<float>(ao->getProperty("release")));
            }
        }
    }
    if (obj->hasProperty("brightness")) {
        ac.brightness = juce::jlimit(0.0f, 1.0f, static_cast<float>(obj->getProperty("brightness")));
    }
    if (obj->hasProperty("hammerHardness")) {
        ac.hammerHardness = juce::jlimit(0.0f, 1.0f, static_cast<float>(obj->getProperty("hammerHardness")));
    }
    if (obj->hasProperty("resonance")) {
        ac.resonance = juce::jlimit(0.0f, 1.0f, static_cast<float>(obj->getProperty("resonance")));
    }
    if (obj->hasProperty("lidPosition")) {
        ac.lidPosition
            = static_cast<std::uint8_t>(juce::jlimit(0, 2, static_cast<int>(obj->getProperty("lidPosition"))));
    }
    if (obj->hasProperty("temperament")) {
        ac.temperament
            = audio::TemperamentEngine::fromIdentifier(obj->getProperty("temperament").toString().toStdString());
    }
    if (obj->hasProperty("referencePitchA4")) {
        ac.referencePitchA4
            = audio::TemperamentEngine::clampReferencePitch(static_cast<double>(obj->getProperty("referencePitchA4")));
    }
    if (obj->hasProperty("soundPerspective")) {
        ac.soundPerspective = audio::PerspectiveProcessor::fromIdentifier(
            obj->getProperty("soundPerspective").toString().toStdString());
    }
    if (obj->hasProperty("reverbSpace")) {
        ac.reverbSpace
            = audio::RoomReverbEngine::fromIdentifier(obj->getProperty("reverbSpace").toString().toStdString());
    }
    if (obj->hasProperty("reverbWet")) {
        ac.reverbWet = juce::jlimit(0.0f, 1.0f, static_cast<float>(obj->getProperty("reverbWet")));
    }
    if (obj->hasProperty("pedalNoiseLevel")) {
        ac.pedalNoiseLevel = juce::jlimit(0.0f, 1.0f, static_cast<float>(obj->getProperty("pedalNoiseLevel")));
    }
    if (obj->hasProperty("feltAgeingAmount")) {
        ac.feltAgeingAmount = juce::jlimit(0.0f, 1.0f, static_cast<float>(obj->getProperty("feltAgeingAmount")));
    }
    if (obj->hasProperty("unaCorda")) {
        ac.unaCorda = static_cast<bool>(obj->getProperty("unaCorda"));
    }
    if (obj->hasProperty("sustainPolicy")) {
        const auto spStr = obj->getProperty("sustainPolicy").toString();
        ac.sustainPolicy = (spStr == "syncPedal") ? core::SustainPolicy::syncPedal : core::SustainPolicy::normal;
    }
    if (obj->hasProperty("transposeEnabled")) {
        ac.transposeEnabled = static_cast<bool>(obj->getProperty("transposeEnabled"));
    }
    if (obj->hasProperty("transposeOffset")) {
        ac.transposeOffset = juce::jlimit(-48, 48, static_cast<int>(obj->getProperty("transposeOffset")));
    }
    if (obj->hasProperty("channelFollowKeyMask")) {
        ac.channelFollowKeyMask = static_cast<std::uint16_t>(
            juce::jlimit(0, 65535, static_cast<int>(obj->getProperty("channelFollowKeyMask"))));
    }
    return ac;
}

juce::var recordedPresetToVar(const RecordedPreset& rp) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("preset", layout::performancePresetToVar(rp.preset));
    obj->setProperty("acoustic", acousticSnapshotToVar(rp.acoustic));
    return { obj.get() };
}

std::optional<RecordedPreset> varToRecordedPreset(const juce::var& v) {
    if (!v.isObject()) {
        return std::nullopt;
    }
    auto* obj = v.getDynamicObject();
    if (obj == nullptr) {
        return std::nullopt;
    }

    const auto acoustic = obj->getProperty("acoustic");
    auto* ac = acoustic.getDynamicObject();
    if (ac == nullptr) {
        return std::nullopt;
    }
    constexpr const char* numericFields[]
        = { "masterGain",       "brightness",       "hammerHardness",      "resonance",
            "lidPosition",      "referencePitchA4", "reverbWet",           "pedalNoiseLevel",
            "feltAgeingAmount", "transposeOffset",  "channelFollowKeyMask" };
    const auto finiteNumber = [](const juce::var& value) {
        return (value.isInt() || value.isInt64() || value.isDouble()) && std::isfinite(static_cast<double>(value));
    };
    for (const auto* key : numericFields) {
        if (!finiteNumber(ac->getProperty(key))) {
            return std::nullopt;
        }
    }
    const auto adsr = ac->getProperty("adsr");
    auto* envelope = adsr.getDynamicObject();
    if (envelope == nullptr) {
        return std::nullopt;
    }
    for (const auto* key : { "attack", "decay", "sustain", "release" }) {
        if (!finiteNumber(envelope->getProperty(key))) {
            return std::nullopt;
        }
    }
    for (const auto* key : { "builtinTone", "temperament", "soundPerspective", "reverbSpace", "sustainPolicy" }) {
        const auto value = ac->getProperty(key);
        if (!value.isString() || value.toString().isEmpty()) {
            return std::nullopt;
        }
    }
    for (const auto* key : { "unaCorda", "transposeEnabled" }) {
        if (!ac->getProperty(key).isBool()) {
            return std::nullopt;
        }
    }
    auto presetOpt = layout::performancePresetFromVar(obj->getProperty("preset"));
    if (!presetOpt.has_value()) {
        return std::nullopt;
    }

    RecordedPreset rp;
    rp.preset = std::move(*presetOpt);
    rp.acoustic = varToAcousticSnapshot(obj->getProperty("acoustic"));
    if (!isAcousticSnapshotValid(rp.acoustic)) {
        return std::nullopt;
    }
    return rp;
}

juce::var eventToVar(const PerformanceEvent& event) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty(performance_file::keyTimestampSamples, static_cast<juce::int64>(event.timestampSamples));

    if (event.type == PerformanceEventType::presetChange) {
        obj->setProperty(performance_file::keyEventType, juce::String(performance_file::eventTypePresetChange));
        obj->setProperty(performance_file::keyPresetId, static_cast<juce::int64>(event.presetId));
    } else {
        obj->setProperty(performance_file::keyEventType, juce::String(performance_file::eventTypeMidi));
        obj->setProperty(performance_file::keySource, sourceToString(event.source));
        obj->setProperty(performance_file::keyMidiData, midiMessageToVar(event.message));
    }
    return obj.get();
}

juce::var metadataToVar(const PerformanceFileMetadata& metadata) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty(performance_file::keyCreatedAt, metadata.createdAt);
    obj->setProperty(performance_file::keyTitle, metadata.title);
    obj->setProperty(performance_file::keyNotes, metadata.notes);
    return obj.get();
}

PerformanceFileMetadata metadataFromVar(const juce::var& v) {
    PerformanceFileMetadata m;
    if (auto* obj = v.getDynamicObject()) {
        m.createdAt = obj->getProperty(performance_file::keyCreatedAt).toString();
        m.title = obj->getProperty(performance_file::keyTitle).toString();
        m.notes = obj->getProperty(performance_file::keyNotes).toString();
    }
    return m;
}

juce::String currentIso8601() {
    return juce::Time::getCurrentTime().toISO8601(true);
}

} // anonymous namespace

// --- Public API: serialise ---

juce::String serialiseTakeToJson(const RecordingTake& take, const PerformanceFileMetadata& metadata) {
    juce::DynamicObject::Ptr root = new juce::DynamicObject();

    root->setProperty(performance_file::keyVersion, performance_file::currentVersion);
    root->setProperty(performance_file::keyFormat, juce::String(performance_file::formatIdentifier));
    root->setProperty(performance_file::keySampleRate, take.sampleRate);
    root->setProperty(performance_file::keyLengthSamples, static_cast<juce::int64>(take.lengthSamples));

    auto meta = metadata;
    if (meta.createdAt.isEmpty()) {
        meta.createdAt = currentIso8601();
    }
    root->setProperty(performance_file::keyMetadata, metadataToVar(meta));
    // Embedded presets table
    juce::Array<juce::var> presetsArray;
    presetsArray.ensureStorageAllocated(static_cast<int>(take.presets.size()));
    for (const auto& preset : take.presets) {
        presetsArray.add(recordedPresetToVar(preset));
    }
    root->setProperty(performance_file::keyPresets, juce::var(presetsArray));

    juce::Array<juce::var> eventsArray;
    eventsArray.ensureStorageAllocated(static_cast<int>(take.events.size()));
    for (const auto& event : take.events) {
        eventsArray.add(eventToVar(event));
    }
    root->setProperty(performance_file::keyEvents, juce::var(eventsArray));
    return juce::JSON::toString(root.get(), true);
}

// --- Public API: deserialise ---

std::optional<RecordingTake> deserialiseTakeFromJson(const juce::String& json) {
    if (json.isEmpty() || json.getNumBytesAsUTF8() > maxPerformanceFileSizeBytes) {
        return std::nullopt;
    }

    auto root = parsePerformanceFileRoot(json);
    if (!root.has_value()) {
        return std::nullopt;
    }

    const auto versionVar = (*root)->getProperty(performance_file::keyVersion);
    if (!versionVar.isInt() && !versionVar.isInt64()) {
        return std::nullopt;
    }
    const auto version = static_cast<juce::int64>(versionVar);
    if (version < 1 || version > performance_file::currentVersion) {
        return std::nullopt;
    }

    const auto sampleRateOpt = parseSupportedSampleRate((*root)->getProperty(performance_file::keySampleRate));
    if (!sampleRateOpt.has_value()) {
        return std::nullopt;
    }

    const auto lengthOpt = parseExactIntegerSample((*root)->getProperty(performance_file::keyLengthSamples));
    if (!lengthOpt.has_value()) {
        return std::nullopt;
    }
    const auto lengthSamples = *lengthOpt;
    if (!isRepresentableTimelineLength(lengthSamples, *sampleRateOpt)) {
        return std::nullopt;
    }
    // Deserialise embedded presets table (v3+)
    std::vector<RecordedPreset> presets;
    const auto presetsVar = (*root)->getProperty(performance_file::keyPresets);
    if (presetsVar.isArray()) {
        auto* arr = presetsVar.getArray();
        presets.reserve(static_cast<size_t>(arr->size()));
        for (const auto& pv : *arr) {
            auto rpOpt = varToRecordedPreset(pv);
            if (!rpOpt.has_value()) {
                DP_LOG_ERROR("[PerformanceFile] Admission rejected: invalid RecordedPreset in embedded presets table");
                return std::nullopt;
            }
            presets.push_back(std::move(*rpOpt));
        }
    } else if (!presetsVar.isVoid()) {
        DP_LOG_ERROR("[PerformanceFile] Admission rejected: presets property is present but not an array");
        return std::nullopt;
    }

    const auto eventsVar = (*root)->getProperty(performance_file::keyEvents);
    if (!eventsVar.isArray()) {
        return std::nullopt;
    }

    auto* eventsArray = eventsVar.getArray();
    if (eventsArray == nullptr) {
        return std::nullopt;
    }

    RecordingTake take;
    take.sampleRate = *sampleRateOpt;
    take.lengthSamples = lengthSamples;
    take.events.reserve(static_cast<size_t>(eventsArray->size()));
    size_t totalDecodedMidiBytes = 0;

    for (const auto& elem : *eventsArray) {
        if (!elem.isObject()) {
            return std::nullopt;
        }

        auto* obj = elem.getDynamicObject();
        if (obj == nullptr) {
            return std::nullopt;
        }

        const auto tsOpt = parseExactIntegerSample(obj->getProperty(performance_file::keyTimestampSamples));
        if (!tsOpt.has_value()) {
            return std::nullopt;
        }
        const auto timestamp = *tsOpt;
        if (timestamp < 0 || timestamp > lengthSamples) {
            return std::nullopt;
        }

        PerformanceEvent event;
        event.timestampSamples = timestamp;

        const auto typeVar = obj->getProperty(performance_file::keyEventType);
        if (typeVar.isVoid() || (typeVar.isString() && typeVar.toString().isEmpty())
            || (typeVar.isString() && typeVar.toString() == performance_file::eventTypeMidi)) {
            event.type = PerformanceEventType::midi;
            event.source = stringToSource(obj->getProperty(performance_file::keySource).toString());

            const auto midiDataVar = obj->getProperty(performance_file::keyMidiData);
            if (!midiDataVar.isString()) {
                return std::nullopt;
            }

            const auto remainingBudget = (static_cast<size_t>(maxPerformanceFileSizeBytes) > totalDecodedMidiBytes)
                ? (static_cast<size_t>(maxPerformanceFileSizeBytes) - totalDecodedMidiBytes)
                : 0;

            auto msg = varToMidiMessage(midiDataVar, remainingBudget);
            if (!msg.has_value()) {
                return std::nullopt;
            }
            totalDecodedMidiBytes += static_cast<size_t>(msg->getRawDataSize());
            event.message = std::move(*msg);
        } else if (typeVar.isString() && typeVar.toString() == performance_file::eventTypePresetChange) {
            // Legacy v1/v2 numeric preset events must be explicitly rejected with diagnostic
            if (version < 3) {
                DP_LOG_ERROR("[PerformanceFile] Legacy numeric preset change event in version " + juce::String(version)
                             + " rejected: format v1/v2 directory indices cannot be safely reinterpreted as take-local "
                               "snapshot slots");
                return std::nullopt;
            }

            event.type = PerformanceEventType::presetChange;
            event.source = stringToSource(obj->getProperty(performance_file::keySource).toString());

            const auto presetIdVar = obj->getProperty(performance_file::keyPresetId);
            if (!presetIdVar.isInt() && !presetIdVar.isInt64()) {
                return std::nullopt;
            }
            const auto pid = static_cast<juce::int64>(presetIdVar);
            if (pid < 0 || static_cast<size_t>(pid) >= presets.size()) {
                DP_LOG_ERROR("[PerformanceFile] Admission rejected: presetId " + juce::String(pid)
                             + " exceeds presets table size (" + juce::String(presets.size()) + ")");
                return std::nullopt;
            }
            event.presetId = static_cast<std::uint32_t>(pid);
        } else {
            return std::nullopt;
        }

        take.events.push_back(std::move(event));
    }

    take.presets = std::move(presets);
    std::stable_sort(take.events.begin(), take.events.end(),
                     [](const PerformanceEvent& a, const PerformanceEvent& b) noexcept {
                         return a.timestampSamples < b.timestampSamples;
                     });

    return take;
}

// --- Public API: file I/O ---

bool savePerformanceFile(const RecordingTake& take, const juce::File& destinationFile,
                         const PerformanceFileMetadata& metadata) {
    if (destinationFile == juce::File() || destinationFile.isDirectory()) {
        return false;
    }

    if (take.isEmpty() || !isSupportedTimelineSampleRate(take.sampleRate)
        || !isRepresentableTimelineLength(take.lengthSamples, take.sampleRate)) {
        return false;
    }

    // Verify all presetChange events point to valid slots in take.presets
    for (const auto& event : take.events) {
        if (event.timestampSamples < 0 || event.timestampSamples > take.lengthSamples) {
            return false;
        }
        if (event.type == PerformanceEventType::presetChange) {
            if (event.presetId >= take.presets.size()) {
                DP_LOG_ERROR("[PerformanceFile] Save rejected: presetChange event references out-of-bounds slot");
                return false;
            }
        }
    }

    // Verify all presets have valid bindings (trigger == keyDown)
    for (const auto& rp : take.presets) {
        for (const auto& binding : rp.preset.layout.bindings) {
            if (binding.action.trigger != devpiano::core::KeyTrigger::keyDown) {
                DP_LOG_ERROR("[PerformanceFile] Save rejected: embedded preset contains unsupported trigger");
                return false;
            }
        }
    }

    size_t totalMidiBytes = 0;
    for (const auto& event : take.events) {
        if (event.type == PerformanceEventType::midi) {
            const auto bytes = static_cast<size_t>(event.message.getRawDataSize());
            if (!isValidRawMidiFrame(event.message.getRawData(), bytes)
                || bytes > static_cast<size_t>(maxPerformanceFileSizeBytes) - totalMidiBytes) {
                return false;
            }
            totalMidiBytes += bytes;
        }
    }

    const auto json = serialiseTakeToJson(take, metadata);
    if (json.isEmpty() || json.getNumBytesAsUTF8() > maxPerformanceFileSizeBytes) {
        return false;
    }

    const auto parentDir = destinationFile.getParentDirectory();
    if (!parentDir.createDirectory()) {
        return false;
    }

    juce::TemporaryFile tempFile(destinationFile);
    {
        juce::FileOutputStream output(tempFile.getFile());
        if (!output.openedOk() || output.getStatus().failed() || !output.writeText(json, false, false, nullptr)) {
            return false;
        }
        output.flush();
        if (output.getStatus().failed()) {
            return false;
        }
    }
    return tempFile.overwriteTargetFileWithTemporary();
}

std::optional<RecordingTake> loadPerformanceFile(const juce::File& sourceFile) {
    if (!sourceFile.existsAsFile()) {
        return std::nullopt;
    }

    const auto fileSize = sourceFile.getSize();
    if (fileSize <= 0 || fileSize > maxPerformanceFileSizeBytes) {
        return std::nullopt;
    }

    std::unique_ptr<juce::FileInputStream> in(sourceFile.createInputStream());
    if (in == nullptr || in->failedToOpen() || in->getStatus().failed()) {
        return std::nullopt;
    }

    const auto streamLength = in->getTotalLength();
    if (streamLength <= 0 || streamLength > maxPerformanceFileSizeBytes || streamLength != fileSize) {
        return std::nullopt;
    }

    juce::MemoryBlock mb;
    const auto bytesRead = in->readIntoMemoryBlock(mb, static_cast<std::ptrdiff_t>(streamLength));
    if (in->getStatus().failed() || std::cmp_not_equal(bytesRead, streamLength)
        || mb.getSize() != static_cast<size_t>(streamLength)) {
        return std::nullopt;
    }

    const auto json = mb.toString();
    if (json.isEmpty() || json.getNumBytesAsUTF8() > maxPerformanceFileSizeBytes) {
        return std::nullopt;
    }

    return deserialiseTakeFromJson(json);
}

std::optional<PerformanceFileMetadata> loadPerformanceFileMetadata(const juce::File& sourceFile) {
    if (!sourceFile.existsAsFile()) {
        return std::nullopt;
    }

    const auto fileSize = sourceFile.getSize();
    if (fileSize <= 0 || fileSize > maxPerformanceFileSizeBytes) {
        return std::nullopt;
    }

    std::unique_ptr<juce::FileInputStream> in(sourceFile.createInputStream());
    if (in == nullptr || in->failedToOpen() || in->getStatus().failed()) {
        return std::nullopt;
    }

    const auto streamLength = in->getTotalLength();
    if (streamLength <= 0 || streamLength > maxPerformanceFileSizeBytes || streamLength != fileSize) {
        return std::nullopt;
    }

    juce::MemoryBlock mb;
    const auto bytesRead = in->readIntoMemoryBlock(mb, static_cast<std::ptrdiff_t>(streamLength));
    if (in->getStatus().failed() || std::cmp_not_equal(bytesRead, streamLength)
        || mb.getSize() != static_cast<size_t>(streamLength)) {
        return std::nullopt;
    }

    const auto json = mb.toString();
    if (json.isEmpty() || json.getNumBytesAsUTF8() > maxPerformanceFileSizeBytes) {
        return std::nullopt;
    }

    auto root = parsePerformanceFileRoot(json);
    if (!root.has_value()) {
        return std::nullopt;
    }

    const auto versionVar = (*root)->getProperty(performance_file::keyVersion);
    if (!versionVar.isInt() && !versionVar.isInt64()) {
        return std::nullopt;
    }
    const auto version = static_cast<int>(versionVar);
    if (version < 1 || version > performance_file::currentVersion) {
        return std::nullopt;
    }

    const auto metadataVar = (*root)->getProperty(performance_file::keyMetadata);
    if (metadataVar.isVoid()) {
        return PerformanceFileMetadata {};
    }
    if (!metadataVar.isObject()) {
        return std::nullopt;
    }

    return metadataFromVar(metadataVar);
}

} // namespace devpiano::recording
