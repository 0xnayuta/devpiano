#include "PerformanceFile.h"

#include "Recording/RecordingEngine.h"

namespace devpiano::recording {
namespace {
// --- Source enum <-> string ---

juce::String sourceToString(RecordingEventSource source) {
    switch (source) {
    case RecordingEventSource::computerKeyboard:
        return performance_file::sourceComputerKeyboard;
    case RecordingEventSource::realtimeMidiBuffer:
        return performance_file::sourceRealtimeMidiBuffer;
    case RecordingEventSource::playback:
        return performance_file::sourcePlayback;
    default:
        return performance_file::sourceComputerKeyboard;
    }
}

RecordingEventSource stringToSource(const juce::String& str) {
    if (str == performance_file::sourceRealtimeMidiBuffer)
        return RecordingEventSource::realtimeMidiBuffer;
    if (str == performance_file::sourcePlayback)
        return RecordingEventSource::playback;
    return RecordingEventSource::computerKeyboard;
}

// --- MidiMessage <-> var ---

juce::var midiMessageToVar(const juce::MidiMessage& msg) {
    juce::MemoryBlock mb(msg.getRawData(), msg.getRawDataSize());
    return juce::var(mb.toBase64Encoding());
}

std::optional<juce::MidiMessage> varToMidiMessage(const juce::var& v) {
    juce::MemoryBlock mb;
    if (!mb.fromBase64Encoding(v.toString()))
        return std::nullopt;
    if (mb.getSize() == 0)
        return std::nullopt;
    return juce::MidiMessage(mb.getData(), static_cast<int>(mb.getSize()), 0);
}

// --- PerformanceEvent <-> var ---

juce::var eventToVar(const PerformanceEvent& event) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty(performance_file::keyTimestampSamples, static_cast<juce::int64>(event.timestampSamples));

    if (event.type == PerformanceEventType::presetChange) {
        obj->setProperty(performance_file::keyEventType, juce::String(performance_file::eventTypePresetChange));
        obj->setProperty(performance_file::keyPresetId, static_cast<int>(event.presetId));
    } else {
        obj->setProperty(performance_file::keyEventType, juce::String(performance_file::eventTypeMidi));
        obj->setProperty(performance_file::keySource, sourceToString(event.source));
        obj->setProperty(performance_file::keyMidiData, midiMessageToVar(event.message));
    }
    return obj.get();
}

std::optional<PerformanceEvent> varToEvent(const juce::var& v) {
    if (!v.isObject())
        return std::nullopt;

    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    PerformanceEvent event;
    event.timestampSamples
        = static_cast<std::int64_t>(static_cast<juce::int64>(obj->getProperty(performance_file::keyTimestampSamples)));

    auto typeStr = obj->getProperty(performance_file::keyEventType).toString();
    if (typeStr == performance_file::eventTypePresetChange) {
        event.type = PerformanceEventType::presetChange;
        auto presetIdVar = obj->getProperty(performance_file::keyPresetId);
        if (presetIdVar.isVoid())
            return std::nullopt;
        event.presetId = static_cast<uint8_t>(static_cast<int>(presetIdVar));
        return event;
    }

    // Default (missing keyEventType or "midi"): legacy MIDI event
    event.type = PerformanceEventType::midi;
    event.source = stringToSource(obj->getProperty(performance_file::keySource).toString());

    auto msg = varToMidiMessage(obj->getProperty(performance_file::keyMidiData));
    if (!msg.has_value())
        return std::nullopt;

    event.message = *msg;
    return event;
}
// --- Metadata <-> var ---

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

// --- ISO 8601 timestamp ---

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

    // Metadata: fill createdAt if empty
    auto meta = metadata;
    if (meta.createdAt.isEmpty())
        meta.createdAt = currentIso8601();
    root->setProperty(performance_file::keyMetadata, metadataToVar(meta));

    // Events
    juce::Array<juce::var> eventsArray;
    eventsArray.ensureStorageAllocated(static_cast<int>(take.events.size()));
    for (const auto& event : take.events)
        eventsArray.add(eventToVar(event));
    root->setProperty(performance_file::keyEvents, juce::var(eventsArray));

    return juce::JSON::toString(root.get(), true);
}

// --- Public API: deserialise ---

std::optional<RecordingTake> deserialiseTakeFromJson(const juce::String& json) {
    juce::var parsed;
    try { parsed = juce::JSON::parse(json); }
    catch (...) { return std::nullopt; }
     if (!parsed.isObject())
        return std::nullopt;

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return std::nullopt;

    // Check format identifier
    auto format = root->getProperty(performance_file::keyFormat).toString();
    if (format != performance_file::formatIdentifier)
        return std::nullopt;

    // Check version
    auto version = static_cast<int>(root->getProperty(performance_file::keyVersion));
    if (version < 1 || version > performance_file::currentVersion)
        return std::nullopt;

    // Read take fields
    RecordingTake take;
    take.sampleRate = static_cast<double>(root->getProperty(performance_file::keySampleRate));
    take.lengthSamples
        = static_cast<std::int64_t>(static_cast<juce::int64>(root->getProperty(performance_file::keyLengthSamples)));

    if (take.sampleRate <= 0.0)
        return std::nullopt;

    // Read events
    auto eventsVar = root->getProperty(performance_file::keyEvents);
    if (!eventsVar.isArray())
        return std::nullopt;

    auto* eventsArray = eventsVar.getArray();
    if (eventsArray == nullptr)
        return std::nullopt;

    take.events.reserve(static_cast<size_t>(eventsArray->size()));
    for (const auto& elem : *eventsArray) {
        auto event = varToEvent(elem);
        if (!event.has_value())
            return std::nullopt;
        take.events.push_back(*event);
    }

    return take;
}

// --- Public API: file I/O ---

bool savePerformanceFile(const RecordingTake& take, const juce::File& destinationFile,
                         const PerformanceFileMetadata& metadata) {
    if (take.isEmpty() || take.sampleRate <= 0.0)
        return false;

    auto json = serialiseTakeToJson(take, metadata);

    if (!destinationFile.getParentDirectory().createDirectory())
        return false;

    return destinationFile.replaceWithText(json);
}

std::optional<RecordingTake> loadPerformanceFile(const juce::File& sourceFile) {
    if (!sourceFile.existsAsFile())
        return std::nullopt;

    auto json = sourceFile.loadFileAsString();
    if (json.isEmpty())
        return std::nullopt;

    return deserialiseTakeFromJson(json);
}

std::optional<PerformanceFileMetadata> loadPerformanceFileMetadata(const juce::File& sourceFile) {
    if (!sourceFile.existsAsFile())
        return std::nullopt;

    auto json = sourceFile.loadFileAsString();
    if (json.isEmpty())
        return std::nullopt;

    juce::var parsed;
    try { parsed = juce::JSON::parse(json); }
    catch (...) { return std::nullopt; }
     if (!parsed.isObject())
        return std::nullopt;

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return std::nullopt;

    // Check format identifier (same guard as deserialiseTakeFromJson)
    auto format = root->getProperty(performance_file::keyFormat).toString();
    if (format != performance_file::formatIdentifier)
        return std::nullopt;

    auto metadataVar = root->getProperty(performance_file::keyMetadata);
    if (metadataVar.isVoid())
        return PerformanceFileMetadata {}; // legacy files: return empty metadata

    return metadataFromVar(metadataVar);
}

} // namespace devpiano::recording
