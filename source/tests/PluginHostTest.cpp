#include <JuceHeader.h>

#include "Plugin/PluginHost.h"

// =============================================================================
// PluginHost 测试：默认状态、只读查询、错误消息、扫描状态、格式与插件列表查询。
//
// 核心插件生命周期（loadPlugin、scanVst3Plugins、prepareToPlay）需要磁盘上真实的
// VST3 文件，此处不做测试——该路径由集成 / 手动测试覆盖。
//
// 本文件覆盖：
//   - 默认构造状态（hasLoadedPlugin、isPrepared、扫描状态）
//   - 错误消息与扫描摘要的默认值
//   - 新建 host 的空插件列表
//   - getAvailableFormatsDescription() 非空
//   - getDefaultVst3SearchPath() 不崩溃
//   - createKnownPluginListXml() 生成合法的空 XML
// =============================================================================

class PluginHostDefaultStateTest : public juce::UnitTest {
public:
    PluginHostDefaultStateTest()
        : juce::UnitTest("PluginHost: default state", "DevPiano/Engine") {
    }

    void runTest() override {
        beginTest("fresh PluginHost 默认状态：无插件、未预备");
        {
            PluginHost host;
            expect(!host.hasLoadedPlugin(), "should not have loaded plugin");
            expect(!host.isPrepared(), "should not be prepared");
            expect(host.getCurrentPluginName().isEmpty(), "plugin name should be empty");
            expect(host.getInstance() == nullptr, "instance should be null");
            expect(host.getLoadedPluginDescription() == nullptr, "description should be null");
            expectEquals(host.getPreparedSampleRate(), 44100.0);
            expectEquals(host.getPreparedBlockSize(), 512);
        }

        beginTest("默认错误消息、扫描摘要与扫描状态");
        {
            PluginHost host;
            expect(host.getLastLoadError().isNotEmpty(), "should have a default error message");
            expect(host.getLastScanSummary().isNotEmpty(), "should have a default scan summary message");
            expectEquals(host.getLastScanPluginCount(), 0);
            expectEquals(host.getLastScanFailedCount(), 0);
            expect(!host.isCurrentlyScanning(), "should not be scanning");
        }

        beginTest("fresh PluginHost 插件列表为空");
        {
            PluginHost host;
            expect(host.getKnownPluginNames().isEmpty(), "known plugin names should be empty");
            expect(host.getInstrumentPluginNames().isEmpty(), "instrument names should be empty");
            expect(host.getEffectPluginNames().isEmpty(), "effect names should be empty");
            expect(host.getPluginListDescription().isNotEmpty(),
                   "list description should describe state even when empty");
        }

        beginTest("格式描述非空、默认 VST3 搜索路径不崩溃");
        {
            PluginHost host;
            expect(host.getAvailableFormatsDescription().isNotEmpty(), "should describe available formats");
            // 在 Linux/WSL 上可能返回空路径或默认路径，仅验证不崩溃。
            auto path = host.getDefaultVst3SearchPath();
            juce::ignoreUnused(path);
        }

        beginTest("插件列表 XML 导出非空、restore 空元素不崩溃");
        {
            PluginHost host;
            auto xml = host.createKnownPluginListXml();
            expect(xml != nullptr, "XML should not be null for empty list");

            juce::XmlElement elem("dummy");
            bool result = host.restoreKnownPluginListFromXml(elem);
            juce::ignoreUnused(result);
        }
    }
};

static PluginHostDefaultStateTest pluginHostDefaultStateTest;
