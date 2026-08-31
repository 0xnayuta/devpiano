#pragma once

#include <JuceHeader.h>

#include "Diagnostics/Log.h"
#include "UI/jive/DesignTokens.h"
#include "UI/jive/StyleCatalog.h"

namespace devpiano::ui::jive {

/**
 * 负责应用启动与热重载时设计 Token 及样式表的加载与解析。
 *
 * 遵循双层加载策略：
 * 1. 编译期 BinaryData 基准兜底（保证独立运行环境 100% 可用）；
 * 2. 本地源码文件覆盖（支持开发阶段 design_tokens.json / style_sheets.json 热重载）。
 */
class StyleBootstrap {
public:
    /// 在指定目录下查找本地文件（开发调试热重载用）
    [[nodiscard]] static juce::File resolveSourceFile(const juce::String& relativePath) {
        auto cwdFile = juce::File::getCurrentWorkingDirectory().getChildFile(relativePath);
        if (cwdFile.existsAsFile()) {
            return cwdFile;
        }

        auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        for (int i = 0; i < 4; ++i) {
            auto candidate = dir.getChildFile(relativePath);
            if (candidate.existsAsFile()) {
                return candidate;
            }
            dir = dir.getParentDirectory();
        }

        return cwdFile;
    }

    /// 初始化加载 DesignTokens（先嵌入兜底，再本地覆盖）
    static void bootstrapDesignTokens(juce::Time& outTokensModTime) {
        // 1. 基准兜底：从编译期嵌入的 BinaryData 加载
        auto embeddedTokens = juce::JSON::parse(
            juce::String::fromUTF8(BinaryData::design_tokens_json, BinaryData::design_tokens_jsonSize));
        if (!embeddedTokens.isVoid()) {
            devpiano::jive::DesignTokens::get().loadFromJSON(embeddedTokens);
        } else {
            DP_LOG_ERROR("[Style] BinaryData::design_tokens_json failed to parse");
        }

        // 2. 开发环境增强：若本地源码存在文件，覆盖加载并记录修改时间
        const auto tokensFile = resolveSourceFile("source/UI/jive/design_tokens.json");
        if (tokensFile.existsAsFile()) {
            outTokensModTime = tokensFile.getLastModificationTime();
            if (auto stream = tokensFile.createInputStream()) {
                auto json = juce::JSON::parse(*stream);
                if (json.isVoid()) {
                    DP_LOG_ERROR("[Style] design_tokens.json failed to parse: " + tokensFile.getFullPathName());
                } else {
                    devpiano::jive::DesignTokens::get().loadFromJSON(json);
                }
            }
        }
    }

    /// 初始化加载 StyleCatalog（先嵌入兜底，再本地覆盖）
    static void bootstrapStyleCatalog(juce::Time& outStylesModTime) {
        // 1. 基准兜底：从编译期嵌入的 BinaryData 加载全局样式表规则
        auto embeddedStyles = juce::JSON::parse(
            juce::String::fromUTF8(BinaryData::style_sheets_json, BinaryData::style_sheets_jsonSize));
        if (!embeddedStyles.isVoid()) {
            StyleCatalog::get().loadFromJSON(embeddedStyles);
        } else {
            DP_LOG_ERROR("[Style] BinaryData::style_sheets_json failed to parse");
        }

        // 2. 开发环境增强：若本地源码存在文件，覆盖加载并记录修改时间
        const auto styleFile = resolveSourceFile("source/UI/jive/style_sheets.json");
        if (styleFile.existsAsFile()) {
            outStylesModTime = styleFile.getLastModificationTime();
            if (auto stream = styleFile.createInputStream()) {
                auto json = juce::JSON::parse(*stream);
                if (json.isVoid()) {
                    DP_LOG_ERROR("[Style] style_sheets.json failed to parse: " + styleFile.getFullPathName());
                } else {
                    StyleCatalog::get().loadFromJSON(json);
                }
            }
        }
    }
};

} // namespace devpiano::ui::jive
