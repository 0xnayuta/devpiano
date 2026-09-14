#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace devpiano::recording {

/**
 * Robust cross-platform decoder for MIDI text meta-events and other untrusted text payloads.

 * Standard MIDI Files carry no character-set tag, so text meta-events (0x01..0x07) contain
 * whatever byte encoding the authoring tool happened to write: UTF-8, a legacy double-byte
 * encoding (GBK/CP936 and friends) or a raw single-byte Windows code page. juce::String
 * assumes UTF-8 unconditionally, which turns such payloads into replacement characters or,
 * worse, into plausible-looking but wrong code points.
 *
 * This decoder applies a cascading heuristic chain instead:
 *   1. Pure 7-bit ASCII fast path (single allocation, no scanning overhead).
 *   2. Structurally valid UTF-8 is preserved verbatim, unless the string looks like a legacy
 *      single-byte mis-decode (see the recovery step below), which is then unwrapped.
 *   3. Structurally valid GBK (CP936) is decoded through a compact 47 KB read-only table;
 *      a buffer only qualifies when every non-ASCII byte pair is actually mapped, which keeps
 *      Big5 and other overlapping double-byte encodings out of this path.
 *   4. Legacy single-byte mis-decodes are unwrapped by restoring the original bytes and
 *      re-reading them as GBK. Authoring tools applied that mistake repeatedly, so the
 *      unwrapping runs for a bounded number of rounds.
 *   5. Anything else falls back to the Windows-1252 code page, which is lossless for every
 *      input byte (never produces U+FFFD).
 *
 * The implementation is pure C++20 and depends only on juce_core / juce_audio_basics, so its
 * results are bit-identical on WSL/Linux, Windows/MSVC and macOS. Platform headers such as
 * <windows.h> are deliberately not involved.
 *
 * Known limitation: GBK and the JIS double-byte planes share their kana rows, so unwrapped
 * Japanese payloads normally decode correctly, but kanji outside that overlap follow the GBK
 * mapping of the same byte pair. Shift-JIS and Big5 specific tables are out of scope.
 */
class MidiTextDecoder {
public:
    /// Decodes the payload of a text meta-event (types 0x01..0x07) of the given message.
    /// Returns an empty string when the message is not a meta-event or carries no payload.
    [[nodiscard]] static juce::String decodeTextMetaEvent(const juce::MidiMessage& message);

    /// Decodes a raw byte buffer of the given length into a UTF-8 juce::String.
    [[nodiscard]] static juce::String decodeText(const void* rawData, int sizeInBytes);
};

} // namespace devpiano::recording
