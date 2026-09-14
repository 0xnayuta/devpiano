#include "MidiTextDecoder.h"

#include "GbkMappingTable.inl"

#include <optional>
#include <vector>

namespace devpiano::recording {

namespace {

// ---------------------------------------------------------------------------
// GBK (CP936) structural helpers
// ---------------------------------------------------------------------------

constexpr int kGbkLeadMin = 0x81;
constexpr int kGbkLeadMax = 0xFE;

// Each lead byte row holds 190 valid trail bytes: 0x40..0x7E and 0x80..0xFE.
constexpr int kGbkTrailColumns = 190;

[[nodiscard]] constexpr bool isValidGbkTrail(int trail) noexcept {
    return (trail >= 0x40 && trail <= 0x7E) || (trail >= 0x80 && trail <= 0xFE);
}

[[nodiscard]] constexpr int gbkTrailIndex(int trail) noexcept {
    return (trail <= 0x7E) ? (trail - 0x40) : (trail - 0x41);
}

/// Maps a GBK byte pair to its Unicode BMP code point, or 0 when the pair is not mapped.
[[nodiscard]] uint16_t gbkPairToUnicode(int lead, int trail) noexcept {
    if (lead < kGbkLeadMin || lead > kGbkLeadMax || !isValidGbkTrail(trail)) {
        return 0;
    }
    return detail::kGbkToUnicodeTable[(lead - kGbkLeadMin) * kGbkTrailColumns + gbkTrailIndex(trail)];
}

/// Non-ASCII classification of a buffer that has already passed UTF-8 validation.
struct Utf8Shape {
    int latin1SupplementSequences = 0;
    int otherMultibyteCodePoints = 0;
    int codePointCount = 0;
};

[[nodiscard]] Utf8Shape scanValidUtf8(const uint8_t* data, int sizeInBytes) noexcept {
    Utf8Shape shape;

    for (int i = 0; i < sizeInBytes;) {
        const auto byte = data[i];

        int sequenceLength = 1;
        if ((byte & 0xE0) == 0xC0) {
            sequenceLength = 2;
        } else if ((byte & 0xF0) == 0xE0) {
            sequenceLength = 3;
        } else if ((byte & 0xF8) == 0xF0) {
            sequenceLength = 4;
        }

        if (sequenceLength == 1) {
            ++shape.codePointCount;
            ++i;
            continue;
        }

        ++shape.codePointCount;

        // U+0080..U+00FF are always encoded as a 0xC2/0xC3 led 2-byte sequence.
        if (sequenceLength == 2 && (byte == 0xC2 || byte == 0xC3)) {
            ++shape.latin1SupplementSequences;
        } else {
            ++shape.otherMultibyteCodePoints;
        }

        i += sequenceLength;
    }

    return shape;
}

// A legacy single-byte mis-decode (raw GBK/EUC/Shift-JIS bytes read as Latin-1 and re-encoded
// as UTF-8) leaves a string that is dominated by U+0080..U+00FF characters, while genuine
// UTF-8 Western text stays mostly ASCII with only occasional accented letters.
constexpr int kMinLatin1SupplementSequences = 3;

[[nodiscard]] bool isLegacyDoubleEncoding(const Utf8Shape& shape) noexcept {
    return shape.otherMultibyteCodePoints == 0 && shape.latin1SupplementSequences >= kMinLatin1SupplementSequences
        && shape.latin1SupplementSequences * 2 >= shape.codePointCount;
}

/// Recovers the original single-byte stream of a legacy double-encoded string.
/// Returns an empty vector as soon as a byte sequence does not fit the 0xC2/0xC3 pattern.
[[nodiscard]] std::vector<uint8_t> extractLegacyBytes(const uint8_t* data, int sizeInBytes) {
    std::vector<uint8_t> recovered;
    recovered.reserve(static_cast<size_t>(sizeInBytes));

    for (int i = 0; i < sizeInBytes;) {
        const auto byte = data[i];

        if (byte < 0x80) {
            recovered.push_back(byte);
            ++i;
            continue;
        }

        if ((byte == 0xC2 || byte == 0xC3) && i + 1 < sizeInBytes && (data[i + 1] & 0xC0) == 0x80) {
            recovered.push_back(static_cast<uint8_t>(((byte & 0x1F) << 6) | (data[i + 1] & 0x3F)));
            i += 2;
            continue;
        }

        return {};
    }

    return recovered;
}

/// True for U+4E00..U+9FFF (CJK Unified Ideographs). Used to reject recovered streams that
/// decode to symbol rows only, which is the typical outcome of an accidental Latin-1 pairing.
[[nodiscard]] bool containsCjkIdeograph(const juce::String& text) noexcept {
    auto pointer = text.getCharPointer();

    while (!pointer.isEmpty()) {
        const auto character = pointer.getAndAdvance();
        if (character >= 0x4E00 && character <= 0x9FFF) {
            return true;
        }
    }

    return false;
}

/// Consumes the buffer as GBK when every non-ASCII byte forms a mapped double-byte character.
[[nodiscard]] bool isStructurallyGbk(const uint8_t* data, int sizeInBytes) noexcept {
    int mappedPairs = 0;

    for (int i = 0; i < sizeInBytes;) {
        const auto byte = data[i];

        if (byte < 0x80) {
            ++i;
            continue;
        }

        if (i + 1 < sizeInBytes && gbkPairToUnicode(byte, data[i + 1]) != 0) {
            ++mappedPairs;
            i += 2;
            continue;
        }

        return false;
    }

    return mappedPairs > 0;
}

[[nodiscard]] juce::String decodeGbk(const uint8_t* data, int sizeInBytes) {
    juce::HeapBlock<juce::juce_wchar> characters(static_cast<size_t>(sizeInBytes));
    size_t numCharacters = 0;

    for (int i = 0; i < sizeInBytes;) {
        const auto byte = data[i];

        if (byte < 0x80) {
            characters[numCharacters++] = static_cast<juce::juce_wchar>(byte);
            ++i;
            continue;
        }

        const auto codePoint = (i + 1 < sizeInBytes) ? gbkPairToUnicode(byte, data[i + 1]) : uint16_t {};
        if (codePoint != 0) {
            characters[numCharacters++] = static_cast<juce::juce_wchar>(codePoint);
            i += 2;
            continue;
        }

        // Unreachable for buffers that passed isStructurallyGbk: keep the raw byte readable.
        characters[numCharacters++]
            = juce::CharacterFunctions::getUnicodeCharFromWindows1252Codepage(static_cast<uint8_t>(byte));
        ++i;
    }

    return juce::String(juce::CharPointer_UTF32(characters.get()),
                        juce::CharPointer_UTF32(characters.get() + numCharacters));
}

[[nodiscard]] juce::String decodeWindows1252(const uint8_t* data, int sizeInBytes) {
    juce::HeapBlock<juce::juce_wchar> characters(static_cast<size_t>(sizeInBytes));

    for (int i = 0; i < sizeInBytes; ++i) {
        characters[i] = juce::CharacterFunctions::getUnicodeCharFromWindows1252Codepage(data[i]);
    }

    return juce::String(juce::CharPointer_UTF32(characters.get()),
                        juce::CharPointer_UTF32(characters.get() + sizeInBytes));
}

/// Attempts to undo a legacy single-byte mis-decode, returning nullopt when the payload is
/// either plausible UTF-8 or cannot be re-read as GBK with a credible CJK result.
///
/// Authoring tools historically applied the mistake more than once (bytes read as Latin-1,
/// re-encoded as UTF-8, then read as Latin-1 again), so the recovery is applied repeatedly
/// until the stream stops matching the pattern, is no longer valid UTF-8, or a bounded
/// number of rounds is exhausted. Each round strictly shrinks the buffer.
[[nodiscard]] std::optional<juce::String> tryRecoverLegacyDoubleEncoding(const uint8_t* data, int sizeInBytes) {
    constexpr int kMaxRounds = 4;

    std::vector<uint8_t> current(data, data + sizeInBytes);
    int rounds = 0;

    while (rounds < kMaxRounds) {
        const auto currentSize = static_cast<int>(current.size());
        if (!juce::CharPointer_UTF8::isValidString(reinterpret_cast<const char*>(current.data()), currentSize)) {
            break;
        }
        if (!isLegacyDoubleEncoding(scanValidUtf8(current.data(), currentSize))) {
            break;
        }

        auto recovered = extractLegacyBytes(current.data(), currentSize);
        if (recovered.empty()) {
            break;
        }

        current = std::move(recovered);
        ++rounds;
    }

    if (rounds == 0) {
        return std::nullopt;
    }

    const auto currentSize = static_cast<int>(current.size());
    if (!isStructurallyGbk(current.data(), currentSize)) {
        return std::nullopt;
    }

    auto recovered = decodeGbk(current.data(), currentSize);
    if (!containsCjkIdeograph(recovered)) {
        return std::nullopt;
    }

    return recovered;
}

} // namespace

juce::String MidiTextDecoder::decodeTextMetaEvent(const juce::MidiMessage& message) {
    if (!message.isMetaEvent()) {
        return {};
    }

    return decodeText(message.getMetaEventData(), message.getMetaEventLength());
}

juce::String MidiTextDecoder::decodeText(const void* rawData, int sizeInBytes) {
    if (rawData == nullptr || sizeInBytes <= 0) {
        return {};
    }

    const auto* data = static_cast<const uint8_t*>(rawData);

    bool isPureAscii = true;
    for (int i = 0; i < sizeInBytes; ++i) {
        if (data[i] >= 0x80) {
            isPureAscii = false;
            break;
        }
    }

    if (isPureAscii) {
        return juce::String(reinterpret_cast<const char*>(data), static_cast<size_t>(sizeInBytes));
    }

    if (juce::CharPointer_UTF8::isValidString(reinterpret_cast<const char*>(data), sizeInBytes)) {
        if (auto recovered = tryRecoverLegacyDoubleEncoding(data, sizeInBytes)) {
            return *recovered;
        }

        return juce::String(juce::CharPointer_UTF8(reinterpret_cast<const char*>(data)),
                            juce::CharPointer_UTF8(reinterpret_cast<const char*>(data) + sizeInBytes));
    }

    if (isStructurallyGbk(data, sizeInBytes)) {
        return decodeGbk(data, sizeInBytes);
    }

    return decodeWindows1252(data, sizeInBytes);
}

} // namespace devpiano::recording
