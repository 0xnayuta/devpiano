#include "MidiFileImporter.h"

#include "Diagnostics/Log.h"
#include "Diagnostics/MidiTrace.h"
#include "MidiTrackMergeEngine.h"
#include "Recording/TimelineValidation.h"
#include "RecordingEngine.h"

#include <cstddef>

namespace {

constexpr std::int64_t kMaxMidiFileSizeBytes = 32LL * 1024 * 1024; // 32MB guard (SEC-002 / PERF-003)

inline uint32_t readBigEndianUint32(const uint8_t* d) noexcept {
    return (static_cast<uint32_t>(d[0]) << 24) | (static_cast<uint32_t>(d[1]) << 16)
        | (static_cast<uint32_t>(d[2]) << 8) | static_cast<uint32_t>(d[3]);
}

inline uint16_t readBigEndianUint16(const uint8_t* d) noexcept {
    return (static_cast<uint16_t>(d[0]) << 8) | static_cast<uint16_t>(d[1]);
}

bool validateTrackEvents(const uint8_t* data, std::size_t size, int trackIndex) {
    std::size_t offset = 0;
    uint8_t runningStatus = 0;
    uint64_t trackTicks = 0;
    bool endOfTrackEncountered = false;

    while (offset < size) {
        if (endOfTrackEncountered) {
            DP_LOG_ERROR("MidiFileImporter: events or bytes found after terminal End of Track in track "
                         + juce::String(trackIndex + 1));
            return false;
        }

        uint32_t deltaTime = 0;
        int deltaBytes = 0;
        bool deltaValid = false;
        for (int b = 0; b < 4 && offset + static_cast<std::size_t>(b) < size; ++b) {
            const uint8_t byte = data[offset + static_cast<std::size_t>(b)];
            deltaTime = (deltaTime << 7) | (byte & 0x7F);
            if (!(byte & 0x80)) {
                deltaBytes = b + 1;
                deltaValid = true;
                break;
            }
        }
        if (!deltaValid) {
            DP_LOG_ERROR("MidiFileImporter: malformed variable-length delta time in track "
                         + juce::String(trackIndex + 1));
            return false;
        }
        offset += static_cast<std::size_t>(deltaBytes);
        trackTicks += deltaTime;
        if (offset >= size) {
            DP_LOG_ERROR("MidiFileImporter: track chunk truncated after delta time in track "
                         + juce::String(trackIndex + 1));
            return false;
        }
        const uint8_t firstByte = data[offset];
        uint8_t status = 0;
        if (firstByte >= 0x80) {
            status = firstByte;
            ++offset;
            if (status < 0xF0) {
                runningStatus = status;
            } else if (status <= 0xF7 || status == 0xFF) {
                runningStatus = 0;
            }
        } else {
            if (runningStatus < 0x80 || runningStatus >= 0xF0) {
                DP_LOG_ERROR("MidiFileImporter: invalid running status 0x" + juce::String::toHexString(runningStatus)
                             + " for data byte 0x" + juce::String::toHexString(firstByte) + " in track "
                             + juce::String(trackIndex + 1));
                return false;
            }
            status = runningStatus;
        }

        if (status == 0xFF) {
            if (offset >= size) {
                DP_LOG_ERROR("MidiFileImporter: truncated meta event type in track " + juce::String(trackIndex + 1));
                return false;
            }
            const uint8_t metaType = data[offset++];

            uint32_t metaLength = 0;
            int lenBytes = 0;
            bool lenValid = false;
            for (int b = 0; b < 4 && offset + static_cast<std::size_t>(b) < size; ++b) {
                const uint8_t byte = data[offset + static_cast<std::size_t>(b)];
                metaLength = (metaLength << 7) | (byte & 0x7F);
                if (!(byte & 0x80)) {
                    lenBytes = b + 1;
                    lenValid = true;
                    break;
                }
            }
            if (!lenValid) {
                DP_LOG_ERROR("MidiFileImporter: malformed variable-length length for meta event 0x"
                             + juce::String::toHexString(metaType) + " in track " + juce::String(trackIndex + 1));
                return false;
            }
            offset += static_cast<std::size_t>(lenBytes);
            if (offset + metaLength > size) {
                DP_LOG_ERROR("MidiFileImporter: truncated meta event data: declared " + juce::String(metaLength)
                             + ", but only " + juce::String(size - offset) + " bytes remain in track "
                             + juce::String(trackIndex + 1));
                return false;
            }

            if (metaType == 0x58) {
                if (metaLength != 4) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x58 time signature meta event length: "
                                 + juce::String(metaLength) + " (must be 4)");
                    return false;
                }
                const uint8_t num = data[offset];
                const uint8_t denomExp = data[offset + 1];
                if (num == 0) {
                    DP_LOG_ERROR("MidiFileImporter: invalid time signature numerator: 0");
                    return false;
                }
                if (denomExp > 30) {
                    DP_LOG_ERROR("MidiFileImporter: invalid time signature denominator exponent: "
                                 + juce::String(denomExp) + " (must be 0..30)");
                    return false;
                }
            } else if (metaType == 0x51) {
                if (metaLength != 3) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x51 tempo meta event length: " + juce::String(metaLength)
                                 + " (must be 3)");
                    return false;
                }
                const uint32_t us = (static_cast<uint32_t>(data[offset]) << 16)
                    | (static_cast<uint32_t>(data[offset + 1]) << 8) | static_cast<uint32_t>(data[offset + 2]);
                if (us == 0) {
                    DP_LOG_ERROR("MidiFileImporter: invalid tempo: 0 microseconds per quarter note");
                    return false;
                }
            } else if (metaType == 0x59) {
                if (metaLength != 2) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x59 key signature meta event length: "
                                 + juce::String(metaLength) + " (must be 2)");
                    return false;
                }
                const int8_t sf = static_cast<int8_t>(data[offset]);
                const uint8_t mi = data[offset + 1];
                if (sf < -7 || sf > 7) {
                    DP_LOG_ERROR("MidiFileImporter: invalid key signature sharps/flats: " + juce::String(sf));
                    return false;
                }
                if (mi > 1) {
                    DP_LOG_ERROR("MidiFileImporter: invalid key signature mode: " + juce::String(mi));
                    return false;
                }
            } else if (metaType == 0x20) {
                if (metaLength != 1) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x20 channel prefix length: " + juce::String(metaLength));
                    return false;
                }
            } else if (metaType == 0x21) {
                if (metaLength != 1) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x21 port length: " + juce::String(metaLength));
                    return false;
                }
            } else if (metaType == 0x2F) {
                if (metaLength != 0) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x2F end of track length: " + juce::String(metaLength));
                    return false;
                }
                endOfTrackEncountered = true;
            } else if (metaType == 0x00) {
                if (metaLength != 0 && metaLength != 2) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x00 sequence number length: " + juce::String(metaLength));
                    return false;
                }
            } else if (metaType == 0x54) {
                if (metaLength != 5) {
                    DP_LOG_ERROR("MidiFileImporter: invalid 0x54 SMPTE offset length: " + juce::String(metaLength));
                    return false;
                }
            }

            offset += static_cast<std::size_t>(metaLength);
        } else if (status == 0xF0 || status == 0xF7) {
            uint32_t sysexLength = 0;
            int lenBytes = 0;
            bool lenValid = false;
            for (int b = 0; b < 4 && offset + static_cast<std::size_t>(b) < size; ++b) {
                const uint8_t byte = data[offset + static_cast<std::size_t>(b)];
                sysexLength = (sysexLength << 7) | (byte & 0x7F);
                if (!(byte & 0x80)) {
                    lenBytes = b + 1;
                    lenValid = true;
                    break;
                }
            }
            if (!lenValid) {
                DP_LOG_ERROR("MidiFileImporter: malformed variable-length length for sysex event in track "
                             + juce::String(trackIndex + 1));
                return false;
            }
            offset += static_cast<std::size_t>(lenBytes);
            if (offset + sysexLength > size) {
                DP_LOG_ERROR("MidiFileImporter: truncated sysex event data in track " + juce::String(trackIndex + 1));
                return false;
            }
            offset += static_cast<std::size_t>(sysexLength);
        } else if (status >= 0x80 && status < 0xF0) {
            int dataBytes = 0;
            switch (status & 0xF0) {
            case 0x80:
            case 0x90:
            case 0xA0:
            case 0xB0:
            case 0xE0:
                dataBytes = 2;
                break;
            case 0xC0:
            case 0xD0:
                dataBytes = 1;
                break;
            default:
                DP_LOG_ERROR("MidiFileImporter: unknown channel status 0x" + juce::String::toHexString(status));
                return false;
            }
            if (offset + static_cast<std::size_t>(dataBytes) > size) {
                DP_LOG_ERROR("MidiFileImporter: truncated channel message (status 0x"
                             + juce::String::toHexString(status) + ") in track " + juce::String(trackIndex + 1));
                return false;
            }
            for (int d = 0; d < dataBytes; ++d) {
                if (data[offset + static_cast<std::size_t>(d)] & 0x80) {
                    DP_LOG_ERROR("MidiFileImporter: invalid data byte (MSB set) in channel message status 0x"
                                 + juce::String::toHexString(status) + " in track " + juce::String(trackIndex + 1));
                    return false;
                }
            }
            offset += static_cast<std::size_t>(dataBytes);
        } else {
            int dataBytes = 0;
            switch (status) {
            case 0xF1:
                dataBytes = 1;
                break;
            case 0xF2:
                dataBytes = 2;
                break;
            case 0xF3:
                dataBytes = 1;
                break;
            case 0xF6:
                dataBytes = 0;
                break;
            default:
                dataBytes = 0;
                break;
            }
            if (offset + static_cast<std::size_t>(dataBytes) > size) {
                DP_LOG_ERROR("MidiFileImporter: truncated system message status 0x" + juce::String::toHexString(status)
                             + " in track " + juce::String(trackIndex + 1));
                return false;
            }
            offset += static_cast<std::size_t>(dataBytes);
        }
    }

    if (!endOfTrackEncountered) {
        DP_LOG_ERROR("MidiFileImporter: track chunk missing required terminal End of Track in track "
                     + juce::String(trackIndex + 1));
        return false;
    }
    if (offset != size) {
        DP_LOG_ERROR("MidiFileImporter: track chunk not fully consumed: offset=" + juce::String(offset)
                     + ", size=" + juce::String(size));
        return false;
    }
    return true;
}

struct SmfValidationResult {
    bool valid = false;
    std::size_t validBytes = 0;
    std::size_t headerBytes = 0;
    bool hasExtensionChunks = false;
};

SmfValidationResult validateSmfStructure(const uint8_t* data, std::size_t size, const juce::String& filePath) {
    SmfValidationResult res;
    if (size < 14) {
        DP_LOG_ERROR("MidiFileImporter: file too small for SMF header: " + filePath);
        return res;
    }

    std::size_t offset = 0;
    const uint32_t mthdTag = 0x4D546864; // "MThd"
    const uint32_t riffTag = 0x52494646; // "RIFF"

    uint32_t firstTag = readBigEndianUint32(data);
    if (firstTag != mthdTag) {
        bool foundRiffMthd = false;
        if (firstTag == riffTag) {
            // Search up to 8 32-bit big-endian words for "MThd", matching JUCE behavior
            for (std::size_t i = 1; i <= 8 && (i * 4 + 4) <= size; ++i) {
                if (readBigEndianUint32(data + i * 4) == mthdTag) {
                    offset = i * 4;
                    foundRiffMthd = true;
                    break;
                }
            }
        }
        if (!foundRiffMthd) {
            DP_LOG_ERROR("MidiFileImporter: invalid MIDI header (not MThd or supported RIFF): " + filePath);
            return res;
        }
    }

    offset += 4; // past "MThd"
    if (offset + 4 > size) {
        DP_LOG_ERROR("MidiFileImporter: truncated MThd chunk size: " + filePath);
        return res;
    }

    const uint32_t headerLength = readBigEndianUint32(data + offset);
    offset += 4;
    if (headerLength < 6 || offset + headerLength > size) {
        DP_LOG_ERROR("MidiFileImporter: invalid MThd chunk length " + juce::String(headerLength) + ": " + filePath);
        return res;
    }

    const uint16_t fileFormat = readBigEndianUint16(data + offset);
    const uint16_t numTracks = readBigEndianUint16(data + offset + 2);
    const int16_t timeFormat = static_cast<int16_t>(readBigEndianUint16(data + offset + 4));
    offset += headerLength;
    res.headerBytes = offset;

    if (fileFormat > 2) {
        DP_LOG_ERROR("MidiFileImporter: unsupported MIDI format " + juce::String(fileFormat) + ": " + filePath);
        return res;
    }
    if (numTracks == 0) {
        DP_LOG_ERROR("MidiFileImporter: file declares 0 tracks: " + filePath);
        return res;
    }
    if (fileFormat == 0 && numTracks != 1) {
        DP_LOG_ERROR("MidiFileImporter: format 0 file declares " + juce::String(numTracks)
                     + " tracks (must be 1): " + filePath);
        return res;
    }
    if (timeFormat == 0) {
        DP_LOG_ERROR("MidiFileImporter: invalid time division zero: " + filePath);
        return res;
    }
    if (timeFormat < 0) {
        const int fps = -(timeFormat >> 8);
        const int subframes = timeFormat & 0xFF;
        const bool validFps = (fps == 24 || fps == 25 || fps == 29 || fps == 30);
        if (!validFps || subframes <= 0) {
            DP_LOG_ERROR("MidiFileImporter: invalid SMPTE time format: fps=" + juce::String(fps)
                         + ", subframes=" + juce::String(subframes) + ": " + filePath);
            return res;
        }
    }

    const uint32_t mtrkTag = 0x4D54726B; // "MTrk"
    int mtrkCount = 0;
    while (mtrkCount < static_cast<int>(numTracks)) {
        if (offset + 8 > size) {
            DP_LOG_ERROR("MidiFileImporter: missing declared MTrk track " + juce::String(mtrkCount + 1) + " of "
                         + juce::String(numTracks) + " in " + filePath);
            return res;
        }

        const uint32_t chunkType = readBigEndianUint32(data + offset);
        const uint32_t chunkSize = readBigEndianUint32(data + offset + 4);
        offset += 8;

        if (offset + chunkSize > size) {
            DP_LOG_ERROR("MidiFileImporter: short chunk body: declared " + juce::String(chunkSize) + " bytes, but only "
                         + juce::String(size - offset) + " remain in " + filePath);
            return res;
        }

        if (chunkType == mtrkTag) {
            if (!validateTrackEvents(data + offset, chunkSize, mtrkCount)) {
                return res;
            }
            ++mtrkCount;
        } else {
            res.hasExtensionChunks = true;
        }

        offset += chunkSize;
    }

    const std::size_t validBytes = offset;
    const std::size_t trailingBytes = size - validBytes;
    if (trailingBytes > 0) {
        DP_LOG_WARN("MidiFileImporter: tolerated " + juce::String(trailingBytes) + " trailing bytes after all "
                    + juce::String(numTracks) + " complete declared chunks: " + filePath);
    }

    res.valid = true;
    res.validBytes = validBytes;
    return res;
}

bool readMidiFile(juce::MidiFile& midiFile, const juce::File& file) {
    juce::MemoryBlock fileData;
    {
        std::unique_ptr<juce::FileInputStream> stream { file.createInputStream() };
        if (!stream || !stream->openedOk()) {
            DP_LOG_ERROR("MidiFileImporter: could not open file for reading: " + file.getFullPathName());
            return false;
        }
        const auto streamLength = stream->getTotalLength();
        const auto expectedSize = file.getSize();
        if (streamLength <= 0 || expectedSize <= 0 || streamLength != expectedSize) {
            DP_LOG_ERROR("MidiFileImporter: stream length mismatch: " + file.getFullPathName());
            return false;
        }
        if (streamLength > kMaxMidiFileSizeBytes) {
            DP_LOG_ERROR("MidiFileImporter: file exceeds maximum allowed size: " + file.getFullPathName());
            return false;
        }
        const auto bytesRead = stream->readIntoMemoryBlock(fileData, static_cast<std::ptrdiff_t>(streamLength));
        if (stream->getStatus().failed() || !stream->isExhausted() || static_cast<int64_t>(bytesRead) != streamLength
            || static_cast<int64_t>(fileData.getSize()) != streamLength) {
            DP_LOG_ERROR("MidiFileImporter: short read or unexhausted stream: " + file.getFullPathName());
            return false;
        }
    }

    const auto valResult = validateSmfStructure(static_cast<const uint8_t*>(fileData.getData()), fileData.getSize(),
                                                file.getFullPathName());
    if (!valResult.valid) {
        return false;
    }

    if (valResult.hasExtensionChunks) {
        juce::MemoryOutputStream normalized(valResult.validBytes);
        const auto* bytes = static_cast<const uint8_t*>(fileData.getData());
        if (!normalized.write(bytes, valResult.headerBytes)) {
            return false;
        }
        auto offset = valResult.headerBytes;
        while (offset < valResult.validBytes) {
            const auto chunkBytes = static_cast<std::size_t>(readBigEndianUint32(bytes + offset + 4)) + 8;
            if (readBigEndianUint32(bytes + offset) == 0x4D54726B && !normalized.write(bytes + offset, chunkBytes)) {
                return false;
            }
            offset += chunkBytes;
        }
        juce::MemoryInputStream stream(normalized.getData(), normalized.getDataSize(), false);
        return midiFile.readFrom(stream, true);
    }
    juce::MemoryInputStream validatedStream { fileData.getData(), valResult.validBytes, false };
    if (!midiFile.readFrom(validatedStream, true)) {
        DP_LOG_ERROR("MidiFileImporter: JUCE failed to parse validated MIDI stream: " + file.getFullPathName());
        return false;
    }

    return true;
}
} // namespace

namespace devpiano::recording {

std::optional<MidiTrackMergeResult> importMidiFileWithMetadata(const juce::File& midiFile, double targetSampleRate,
                                                               const MidiImportOptions& options) {
    if (!isSupportedTimelineSampleRate(targetSampleRate)) {
        DP_LOG_ERROR("MidiFileImporter: unsupported sample rate: " + juce::String(targetSampleRate));
        return std::nullopt;
    }

    if (!midiFile.existsAsFile()) {
        DP_LOG_ERROR("MidiFileImporter: file does not exist: " + midiFile.getFullPathName());
        return std::nullopt;
    }

    if (midiFile.getSize() == 0) {
        DP_LOG_ERROR("MidiFileImporter: file is empty: " + midiFile.getFullPathName());
        return std::nullopt;
    }

    if (midiFile.getSize() > kMaxMidiFileSizeBytes) {
        DP_LOG_ERROR("MidiFileImporter: file exceeds maximum allowed size (32MB): " + midiFile.getFullPathName());
        return std::nullopt;
    }
    juce::MidiFile file;
    if (!readMidiFile(file, midiFile)) {
        return std::nullopt;
    }

    DP_TRACE_MIDI("MidiFile imported: " + midiFile.getFileName() + ", tracks=" + juce::String(file.getNumTracks()),
                  "MidiImporter");
#if defined(JUCE_DEBUG) || defined(DEBUG)
    const auto timeFormat = file.getTimeFormat();
    if (timeFormat < 0) {
        const auto fps = -(timeFormat >> 8);
        const auto subframes = timeFormat & 0xff;
        DP_DEBUG_LOG("MidiFileImporter: SMPTE timing detected: " + juce::String(fps) + " fps, "
                     + juce::String(subframes) + " subframes/frame");
    } else {
        DP_DEBUG_LOG("MidiFileImporter: PPQ = " + juce::String(timeFormat));
    }
#endif

    // Do NOT override timeFormat - readFrom() has already set it correctly from the
    // MIDI file header. Convert native tick timestamps -> seconds across all tracks.
    file.convertTimestampTicksToSeconds();

    MidiTrackMergeOptions mergeOptions;
    mergeOptions.channelStrategy = options.channelStrategy;
    auto mergeResult = MidiTrackMergeEngine::mergeTracks(file, targetSampleRate, mergeOptions);
    if (!mergeResult.has_value()) {
        DP_LOG_ERROR("MidiFileImporter: failed to merge tracks from " + midiFile.getFileName());
        return std::nullopt;
    }

    DP_LOG_INFO("MidiFileImporter: successfully imported " + midiFile.getFileName() + " ("
                + juce::String(mergeResult->stats.mergedEventCount) + " events, "
                + juce::String(mergeResult->stats.durationSeconds, 2) + "s, tracks="
                + juce::String(mergeResult->stats.trackCount) + ") | " + mergeResult->metadata.formatSummary());

    return mergeResult;
}

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate) {
    return importMidiFile(midiFile, targetSampleRate, MidiImportOptions {});
}

std::optional<RecordingTake> importMidiFile(const juce::File& midiFile, double targetSampleRate,
                                            const MidiImportOptions& options) {
    auto result = importMidiFileWithMetadata(midiFile, targetSampleRate, options);
    if (!result.has_value()) {
        return std::nullopt;
    }
    return std::move(result->take);
}

} // namespace devpiano::recording
