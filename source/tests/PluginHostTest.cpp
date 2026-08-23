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
        beginTest("fresh PluginHost default state: no plugin, unprepared");
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

        beginTest("default error messages, scan summary, and scanning status");
        {
            PluginHost host;
            expect(host.getLastLoadError().isNotEmpty(), "should have a default error message");
            expect(host.getLastScanSummary().isNotEmpty(), "should have a default scan summary message");
            expectEquals(host.getLastScanPluginCount(), 0);
            expectEquals(host.getLastScanFailedCount(), 0);
            expect(!host.isCurrentlyScanning(), "should not be scanning");
        }

        beginTest("fresh PluginHost plugin list is empty");
        {
            PluginHost host;
            expect(host.getKnownPluginNames().isEmpty(), "known plugin names should be empty");
            expect(host.getInstrumentPluginNames().isEmpty(), "instrument names should be empty");
            expect(host.getEffectPluginNames().isEmpty(), "effect names should be empty");
            expect(host.getPluginListDescription().isNotEmpty(),
                   "list description should describe state even when empty");
        }

        beginTest("formats description non-empty, default VST3 search path is absolute");
        {
            PluginHost host;
            expect(host.getAvailableFormatsDescription().isNotEmpty(), "should describe available formats");
            // Linux/WSL 上 VST3 格式不可用时路径列表为空；存在时每条都必须是
            // 绝对路径——可证伪且跨平台稳定。
            const auto searchPath = host.getDefaultVst3SearchPath();
            for (int i = 0; i < searchPath.getNumPaths(); ++i) {
                const auto path = searchPath[i].getFullPathName();
                expect(juce::File::isAbsolutePath(path), "default VST3 search path must be absolute: " + path);
            }
        }

        beginTest("plugin list XML export non-null, restore empty element reports empty cache");
        {
            PluginHost host;
            auto xml = host.createKnownPluginListXml();
            expect(xml != nullptr, "XML should not be null for empty list");

            juce::XmlElement elem("dummy");
            // 空 XML 恢复：应返回 false（空缓存）并更新扫描摘要/计数——可观察语义。
            bool result = host.restoreKnownPluginListFromXml(elem);
            expect(!result, "restoring an empty plugin list must report an empty cache");
            expectEquals(host.getLastScanPluginCount(), 0);
            expect(host.getLastScanSummary().isNotEmpty(), "summary must describe the empty-cache result");
        }
    }
};

static PluginHostDefaultStateTest pluginHostDefaultStateTest;
