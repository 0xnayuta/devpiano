#include <JuceHeader.h>

#include "Plugin/PluginHost.h"
#include "TestHelpers.h"
#include <memory>

// =============================================================================
// PluginHost 崩溃安全扫描持久化测试（Phase 34-E-1）：
//   - dead-man's pedal 记录的崩溃插件在扫描前被列入黑名单并推迟到末尾
//   - 扫描会话取消时，已发现的部分结果通过增量回调外泄供调用方立即持久化
//   - 未注册回调时这些路径必须保持无副作用（不崩溃、不误报进度）
// =============================================================================

namespace {

// 构造一个含单个插件的 KnownPluginList XML，模拟缓存的部分扫描结果。
std::unique_ptr<juce::XmlElement> makeCachedPluginListXml() {
    auto root = std::make_unique<juce::XmlElement>("KNOWNPLUGINS");
    auto* plugin = new juce::XmlElement("PLUGIN");
    plugin->setAttribute("name", "Cached Synth");
    plugin->setAttribute("desc", "Cached Synth");
    plugin->setAttribute("category", "Synth");
    plugin->setAttribute("manufacturer", "Test");
    plugin->setAttribute("version", "1.0.0");
    plugin->setAttribute("file", "/tmp/cached-synth.vst3");
    plugin->setAttribute("uid", "90001");
    plugin->setAttribute("isInstrument", "1");
    plugin->setAttribute("numInputChannels", "2");
    plugin->setAttribute("numOutputChannels", "2");
    root->addChildElement(plugin);
    return root;
}

constexpr auto crashedPluginPath = "/tmp/devpiano-crashed-plugin.vst3";

} // namespace

class PluginScanPersistenceTest final : public juce::UnitTest {
public:
    PluginScanPersistenceTest()
        : juce::UnitTest("PluginHost: crash-safe scan persistence", "DevPiano/Engine") {
    }

    void runTest() override {
        testDeadMansPedalRecovery();
        testIncrementalCallbackReportsPartialResults();
        testNoCallbackIsSideEffectFree();
    }

private:
    // 扫描会话依赖可用的 VST3 格式；该格式在部分 Linux/WSL 配置下不存在，
    // 此时跳过用例（环境限制，非产品缺陷）。
    static bool beginScanOrSkip(PluginHost& host, const juce::FileSearchPath& path) {
        return host.beginVst3ScanSession(path, false);
    }

    void testDeadMansPedalRecovery() {
        beginTest("crash-prone plugin from a previous scan is blacklisted before scanning");
        {
            devpiano::test::ScopedTempDir tempDir("plugin-scan-pedal");

            const auto scanDir = tempDir.getChildFile("plugins");
            expect(scanDir.createDirectory().wasOk(), "scan directory must be created");
            const juce::FileSearchPath scanPath(scanDir.getFullPathName());

            // 对照组：没有 pedal 文件时不产生任何黑名单条目。
            {
                PluginHost cleanHost;
                cleanHost.setDeadMansPedalFile(tempDir.getChildFile("absent-pedal.txt"));

                if (!beginScanOrSkip(cleanHost, scanPath)) {
                    return;
                }

                expect(cleanHost.getBlacklistedPluginFiles().isEmpty(),
                       "a scan without a pedal file must not blacklist anything");
                cleanHost.cancelVst3ScanSession();
            }

            // 实验组：pedal 文件记录的崩溃插件必须被列入黑名单并推迟。
            {
                const auto pedalFile = tempDir.getChildFile("dead-mans-pedal.txt");
                expect(pedalFile.replaceWithText(crashedPluginPath), "pedal file must be writable");

                PluginHost host;
                host.setDeadMansPedalFile(pedalFile);

                if (!beginScanOrSkip(host, scanPath)) {
                    return;
                }

                const auto blacklisted = host.getBlacklistedPluginFiles();
                expect(blacklisted.contains(crashedPluginPath),
                       "the crashed plugin recorded in the pedal file must be blacklisted");

                host.cancelVst3ScanSession();
            }
        }
    }

    void testIncrementalCallbackReportsPartialResults() {
        beginTest("cancelling a scan with discoveries reports them through the incremental callback");
        {
            PluginHost host;

            int callbackCount = 0;
            juce::StringArray observedNames;
            host.setScanIncrementalCallback([&](const PluginHost& source) {
                ++callbackCount;
                observedNames = source.getKnownPluginNames();
            });

            // 空列表：取消不得虚报进度。
            host.cancelVst3ScanSession();
            expectEquals(callbackCount, 0, "an empty scan must not report incremental progress");

            expect(host.restoreKnownPluginListFromXml(*makeCachedPluginListXml()), "cached plugin list must restore");

            host.cancelVst3ScanSession();
            expectEquals(callbackCount, 1, "partial results must be persisted once on cancellation");
            expect(observedNames.contains("Cached Synth"), "the callback must observe the plugins discovered so far");
        }
    }

    void testNoCallbackIsSideEffectFree() {
        beginTest("scan teardown without a registered callback stays side-effect free");
        {
            PluginHost host;
            expect(host.restoreKnownPluginListFromXml(*makeCachedPluginListXml()));

            host.cancelVst3ScanSession();
            expectEquals(host.getKnownPluginNames().size(), 1,
                         "cancelling without a callback must leave the known list untouched");

            expect(host.getBlacklistedPluginFiles().isEmpty(), "no pedal file must leave no blacklist entries");
        }
    }
};

static PluginScanPersistenceTest pluginScanPersistenceTest;
