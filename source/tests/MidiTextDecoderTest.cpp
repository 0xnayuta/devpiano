#include <JuceHeader.h>

#include "Recording/MidiTextDecoder.h"
#include "Recording/MidiTrackMergeEngine.h"

#include <initializer_list>

// =============================================================================
// MIDI 文本元事件解码测试
//
// 标准 MIDI 文件不携带字符集信息，历史文件中的轨道名/标题多为本地编码
// (GBK/CP936、EUC-JP 等) 或已被作者工具双重编码。本测试覆盖解码链的每一层：
//   1. 纯 ASCII 快速通道
//   2. 合法 UTF-8 原样保留
//   3. GBK 结构校验与码表解码（真实回归：简体中文标题）
//   4. 历史双重编码 (Latin-1 Mojibake) 的逆向恢复
//   5. Windows-1252 无损兜底
//
// 期望文本一律以 Unicode 码点构造，测试源码保持纯 7-bit ASCII。
// =============================================================================

namespace {

/// 以 Unicode 码点构造期望字符串，避免在源码中书写裸非 ASCII 字符。
juce::String fromCodePoints(std::initializer_list<juce::uint32> codePoints) {
    juce::String result;
    for (const auto codePoint : codePoints) {
        result += juce::String::charToString(static_cast<juce::juce_wchar>(codePoint));
    }
    return result;
}

/// GBK 编码的简体中文标题（"meng zhong de hun li" = Wedding in a Dream）。
constexpr juce::uint8 kGbkChineseTitle[] = { 0xC3, 0xCE, 0xD6, 0xD0, 0xB5, 0xC4, 0xBB, 0xE9, 0xC0, 0xF1 };

/// 同一标题的 UTF-8 表示，用于验证合法 UTF-8 不被改写。
constexpr juce::uint8 kUtf8ChineseTitle[]
    = { 0xE6, 0xA2, 0xA6, 0xE4, 0xB8, 0xAD, 0xE7, 0x9A, 0x84, 0xE5, 0xA9, 0x9A, 0xE7, 0xA4, 0xBC };

/// 日文平假名标题先经 EUC-JP 编码、再被按 Latin-1 解读并以 UTF-8 写回文件的字节流
/// (外部来源 MIDI 文件中常见的双重编码标题，需要一轮解包)。
constexpr juce::uint8 kDoubleEncodedKanaTitle[]
    = { 0xC2, 0xA4, 0xC3, 0x92, 0xC2, 0xA4, 0xC2, 0xB0, 0xC2, 0xA4, 0xC3, 0xA9, 0xC2, 0xA4, 0xC2, 0xB7, 0xC2, 0xA4,
        0xC3, 0x8E, 0xC2, 0xA4, 0xC3, 0x8A, 0xC2, 0xA4, 0xC2, 0xAF, 0xC2, 0xBA, 0xC2, 0xA2, 0xC2, 0xA4, 0xC3, 0x8B };

/// 真实外部 MIDI 文件 (you.mid) 中的轨道名原始字节：该文件被连续两次按 Latin-1 误读
/// 并重新以 UTF-8 写回，需要多轮解包才能还原日文标题。
constexpr juce::uint8 kTwiceReEncodedKanaTitle[]
    = { 0xC3, 0x82, 0xC2, 0xA4, 0xC3, 0x83, 0xC2, 0x92, 0xC3, 0x82, 0xC2, 0xA4, 0xC3, 0x82, 0xC2, 0xB0, 0xC3, 0x82,
        0xC2, 0xA4, 0xC3, 0x83, 0xC2, 0xA9, 0xC3, 0x82, 0xC2, 0xA4, 0xC3, 0x82, 0xC2, 0xB7, 0xC3, 0x82, 0xC2, 0xA4,
        0xC3, 0x83, 0xC2, 0x8E, 0xC3, 0x82, 0xC2, 0xA4, 0xC3, 0x83, 0xC2, 0x8A, 0xC3, 0x82, 0xC2, 0xA4, 0xC3, 0x82,
        0xC2, 0xAF, 0xC3, 0x83, 0xC2, 0xAD, 0xC3, 0x82, 0xC2, 0x95, 0xC3, 0x82, 0xC2, 0xA4, 0xC3, 0x83, 0xC2, 0x8B,
        0xC3, 0x82, 0xC2, 0xA1, 0xC3, 0x82, 0xC2, 0xA4, 0xC3, 0x82, 0xC2, 0xBD, 0xC3, 0x83, 0xC2, 0xA2 };

} // namespace

class MidiTextDecoderTest : public juce::UnitTest {
public:
    MidiTextDecoderTest()
        : juce::UnitTest("MidiTextDecoder", "DevPiano/Recording") {
    }

    void runTest() override {
        using devpiano::recording::MidiTextDecoder;

        testCase("null and empty payloads decode to an empty string", [&] {
            constexpr juce::uint8 sample[] = { 0x41 };
            expectEquals(MidiTextDecoder::decodeText(nullptr, 0), juce::String());
            expectEquals(MidiTextDecoder::decodeText(nullptr, 4), juce::String());
            expectEquals(MidiTextDecoder::decodeText(sample, 0), juce::String());
            expectEquals(MidiTextDecoder::decodeText(sample, -1), juce::String());
        });

        testCase("pure ASCII payload is returned verbatim", [&] {
            const char text[] = "Sonata in C Major";
            expectEquals(MidiTextDecoder::decodeText(text, static_cast<int>(sizeof(text) - 1)),
                         juce::String("Sonata in C Major"));
        });

        testCase("valid UTF-8 payload keeps its original code points", [&] {
            const auto decoded = MidiTextDecoder::decodeText(kUtf8ChineseTitle, sizeof(kUtf8ChineseTitle));
            expectEquals(decoded, fromCodePoints({ 0x68A6, 0x4E2D, 0x7684, 0x5A5A, 0x793C }));
            expect(!decoded.containsChar(0xFFFD), "no replacement characters may appear");
        });

        testCase("GBK payload decodes through the structural GBK path", [&] {
            const auto decoded = MidiTextDecoder::decodeText(kGbkChineseTitle, sizeof(kGbkChineseTitle));
            expectEquals(decoded, fromCodePoints({ 0x68A6, 0x4E2D, 0x7684, 0x5A5A, 0x793C }));
            expect(!decoded.containsChar(0xFFFD), "no replacement characters may appear");
        });

        testCase("GBK payload mixed with ASCII keeps both parts readable", [&] {
            constexpr juce::uint8 payload[]
                = { 0xB8, 0xD6, 0xC7, 0xD9, 0x20, 0x2D, 0x20, 0x50, 0x69, 0x61, 0x6E, 0x6F };
            expectEquals(MidiTextDecoder::decodeText(payload, sizeof(payload)),
                         fromCodePoints({ 0x94A2, 0x7434 }) + juce::String(" - Piano"));
        });

        testCase("accented Latin-1 UTF-8 text is not mistaken for a legacy re-encoding", [&] {
            const char text[] = "Caf\xC3\xA9 Gr\xC3\xB6\xC3\x9F"
                                "e";
            expectEquals(MidiTextDecoder::decodeText(text, static_cast<int>(sizeof(text) - 1)),
                         fromCodePoints({ 0x43, 0x61, 0x66, 0xE9, 0x20, 0x47, 0x72, 0xF6, 0xDF, 0x65 }));
        });

        testCase("single-byte accented titles are not absorbed by the double-byte path", [&] {
            // Real payloads: German and French titles stored in the Windows-1252 code page.
            // Their accented bytes happen to form GBK-mapped pairs, so acceptance by the table
            // alone must not be enough to claim double-byte text.
            constexpr juce::uint8 furElise[] = { 0x46, 0xFC, 0x72, 0x20, 0x45, 0x6C, 0x69, 0x73, 0x65 };
            expectEquals(MidiTextDecoder::decodeText(furElise, sizeof(furElise)),
                         fromCodePoints({ 0x46, 0xFC, 0x72, 0x20, 0x45, 0x6C, 0x69, 0x73, 0x65 }));

            constexpr juce::uint8 cafeGrosse[] = { 0x43, 0x61, 0x66, 0xE9, 0x20, 0x47, 0x72, 0xF6, 0xDF, 0x65 };
            expectEquals(MidiTextDecoder::decodeText(cafeGrosse, sizeof(cafeGrosse)),
                         fromCodePoints({ 0x43, 0x61, 0x66, 0xE9, 0x20, 0x47, 0x72, 0xF6, 0xDF, 0x65 }));
        });

        testCase("ASCII prefixed GBK payload keeps both parts readable", [&] {
            // Real payload (instrument name of a multi-track file): ASCII vendor name followed
            // by a GBK product name.
            constexpr juce::uint8 payload[]
                = { 0x4D, 0x69, 0x63, 0x72, 0x6F, 0x73, 0x6F, 0x66, 0x74, 0x20, 0x47, 0x53, 0x20, 0xB2,
                    0xA8, 0xB1, 0xED, 0xC8, 0xED, 0xBC, 0xFE, 0xBA, 0xCF, 0xB3, 0xC9, 0xC6, 0xF7 };
            expectEquals(MidiTextDecoder::decodeText(payload, sizeof(payload)),
                         juce::String("Microsoft GS ")
                             + fromCodePoints({ 0x6CE2, 0x8868, 0x8F6F, 0x4EF6, 0x5408, 0x6210, 0x5668 }));
        });

        testCase("legacy double-encoded kana title is recovered", [&] {
            const auto decoded = MidiTextDecoder::decodeText(kDoubleEncodedKanaTitle, sizeof(kDoubleEncodedKanaTitle));

            // Kana rows are shared between the JIS and GBK double-byte planes, so the kana of the
            // original title survive; the trailing kanji maps to its GBK byte-pair equivalent.
            expectEquals(decoded,
                         fromCodePoints({ 0x3072, 0x3050, 0x3089, 0x3057, 0x306E, 0x306A, 0x304F, 0x5B69, 0x306B }));
            expect(!decoded.containsChar(0xFFFD), "no replacement characters may appear");
        });

        testCase("repeatedly re-encoded real-world kana title is unwrapped", [&] {
            const auto decoded
                = MidiTextDecoder::decodeText(kTwiceReEncodedKanaTitle, sizeof(kTwiceReEncodedKanaTitle));

            expectEquals(decoded,
                         fromCodePoints(
                             { 0x3072, 0x3050, 0x3089, 0x3057, 0x306E, 0x306A, 0x304F, 0x9803, 0x306B, 0xB7, 0x89E3 }));
            expect(!decoded.containsChar(0xFFFD), "no replacement characters may appear");
        });

        testCase("undecodable bytes fall back to Windows-1252 without loss", [&] {
            constexpr juce::uint8 loneByte[] = { 0xFF };
            expectEquals(MidiTextDecoder::decodeText(loneByte, sizeof(loneByte)), fromCodePoints({ 0xFF }));
            expect(!MidiTextDecoder::decodeText(loneByte, sizeof(loneByte)).containsChar(0xFFFD));
        });

        testCase("meta-event accessor forwards payloads and rejects other messages", [&] {
            constexpr juce::uint8 trackNameMeta[]
                = { 0xFF, 0x03, 0x0A, 0xC3, 0xCE, 0xD6, 0xD0, 0xB5, 0xC4, 0xBB, 0xE9, 0xC0, 0xF1 };
            const juce::MidiMessage message(trackNameMeta, sizeof(trackNameMeta), 0.0);
            expect(message.isMetaEvent());
            expectEquals(message.getMetaEventType(), 3);
            expectEquals(MidiTextDecoder::decodeTextMetaEvent(message),
                         fromCodePoints({ 0x68A6, 0x4E2D, 0x7684, 0x5A5A, 0x793C }));

            expectEquals(MidiTextDecoder::decodeTextMetaEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100)),
                         juce::String());
            expectEquals(MidiTextDecoder::decodeTextMetaEvent(juce::MidiMessage::textMetaEvent(3, juce::String())),
                         juce::String());
        });

        testCase("merged import surfaces a GBK track name as readable metadata", [&] {
            juce::MidiFile file;

            juce::MidiMessageSequence track0;
            constexpr juce::uint8 trackNameMeta[]
                = { 0xFF, 0x03, 0x0A, 0xC3, 0xCE, 0xD6, 0xD0, 0xB5, 0xC4, 0xBB, 0xE9, 0xC0, 0xF1 };
            track0.addEvent(juce::MidiMessage(trackNameMeta, sizeof(trackNameMeta), 0.0), 0.0);
            track0.addEvent(juce::MidiMessage::textMetaEvent(1, "Piano Solo"), 0.0);
            file.addTrack(track0);

            juce::MidiMessageSequence track1;
            track1.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0.1);
            track1.addEvent(juce::MidiMessage::noteOff(1, 60, (juce::uint8)0), 0.5);
            file.addTrack(track1);

            const auto merged = devpiano::recording::MidiTrackMergeEngine::mergeTracks(file, 48000.0);
            expect(merged.has_value());
            if (!merged.has_value()) {
                return;
            }

            const auto expectedTitle = fromCodePoints({ 0x68A6, 0x4E2D, 0x7684, 0x5A5A, 0x793C });
            expectEquals(merged->metadata.songTitle, expectedTitle);
            expectEquals(merged->metadata.tracks[0].trackName, expectedTitle);
            expect(merged->metadata.formatSummary().contains(expectedTitle), "summary must surface the decoded title");
            expect(!merged->metadata.songTitle.containsChar(0xFFFD));
        });
    }
};

static MidiTextDecoderTest midiTextDecoderTest;
