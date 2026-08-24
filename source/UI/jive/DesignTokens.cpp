#include "DesignTokens.h"

namespace devpiano::jive {

// ── Singleton ──────────────────────────────────────────────────

DesignTokens& DesignTokens::get() {
    static DesignTokens instance;
    return instance;
}

// ── Loading ────────────────────────────────────────────────────

void DesignTokens::loadFromJSON(const juce::var& json) {
    if (auto* obj = json.getDynamicObject()) {
        root = obj;
    }
}

juce::DynamicObject::Ptr DesignTokens::colorsNode() const {
    if (auto* c = root->getProperty("colors").getDynamicObject()) {
        return c;
    }
    return {};
}

juce::DynamicObject::Ptr DesignTokens::typographyNode() const {
    if (auto* t = root->getProperty("typography").getDynamicObject()) {
        return t;
    }
    return {};
}

juce::DynamicObject::Ptr DesignTokens::borderRadiusNode() const {
    if (auto* b = root->getProperty("border-radius").getDynamicObject()) {
        return b;
    }
    return {};
}

juce::DynamicObject::Ptr DesignTokens::spacingNode() const {
    if (auto* s = root->getProperty("spacing").getDynamicObject()) {
        return s;
    }
    return {};
}

// ── Parsing helpers ────────────────────────────────────────────

juce::Colour DesignTokens::parseColor(juce::StringRef key, juce::Colour fallback) const {
    if (auto node = colorsNode()) {
        const auto v = node->getProperty(juce::Identifier(juce::String { key }));
        if (!v.isVoid()) {
            return juce::Colour::fromString(v.toString());
        }
    }
    return fallback;
}

float DesignTokens::parseFloat(juce::StringRef section, juce::StringRef key, float fallback) const {
    juce::DynamicObject::Ptr node;
    const juce::String sec { section };
    if (sec == "typography") {
        node = typographyNode();
    } else if (sec == "border-radius") {
        node = borderRadiusNode();
    } else if (sec == "spacing") {
        node = spacingNode();
    }
    if (node != nullptr) {
        const auto v = node->getProperty(juce::Identifier(juce::String { key }));
        if (!v.isVoid()) {
            return static_cast<float>(v);
        }
    }
    return fallback;
}

int DesignTokens::parseInt(juce::StringRef /*section*/, juce::StringRef key, int fallback) const {
    if (auto node = spacingNode()) {
        const auto v = node->getProperty(juce::Identifier(juce::String { key }));
        if (!v.isVoid()) {
            return static_cast<int>(v);
        }
    }
    return fallback;
}

juce::String DesignTokens::parseString(juce::StringRef /*section*/, juce::StringRef key, juce::String fallback) const {
    if (auto node = typographyNode()) {
        const auto v = node->getProperty(juce::Identifier(juce::String { key }));
        if (!v.isVoid()) {
            return v.toString();
        }
    }
    return fallback;
}

// ── Colors ─────────────────────────────────────────────────────

juce::Colour DesignTokens::mainBg() const {
    return parseColor("main-bg", juce::Colour(0xff111316));
}
juce::Colour DesignTokens::panelBg() const {
    return parseColor("panel-bg", juce::Colour(0xff181a1f));
}
juce::Colour DesignTokens::controlBg() const {
    return parseColor("control-bg", juce::Colour(0xff22252c));
}
juce::Colour DesignTokens::cardBorder() const {
    return parseColor("card-border", juce::Colour(0xff2b2f38));
}
juce::Colour DesignTokens::primary() const {
    return parseColor("primary", juce::Colour(0xff00c8f0));
}
juce::Colour DesignTokens::primaryAlpha30() const {
    return parseColor("primary-alpha-30", juce::Colour(0xff00c8f0).withAlpha(0.3f));
}
juce::Colour DesignTokens::recordActive() const {
    return parseColor("record-active", juce::Colour(0xffe05345));
}
juce::Colour DesignTokens::playActive() const {
    return parseColor("play-active", juce::Colour(0xff2ecc71));
}
juce::Colour DesignTokens::textPrimary() const {
    return parseColor("text-primary", juce::Colour(0xfff0f2f5));
}
juce::Colour DesignTokens::textSecondary() const {
    return parseColor("text-secondary", juce::Colour(0xffa0a6b2));
}
juce::Colour DesignTokens::textDisabled() const {
    return parseColor("text-disabled", juce::Colour(0xff555b66));
}
juce::Colour DesignTokens::highlightOverlay() const {
    return parseColor("highlight-overlay", juce::Colours::white.withAlpha(0.094f));
}
juce::Colour DesignTokens::pressOverlay() const {
    return parseColor("press-overlay", juce::Colours::black.withAlpha(0.2f));
}
juce::Colour DesignTokens::rotaryBgTrack() const {
    return parseColor("rotary-bg-track", juce::Colour(0xff181a1e));
}
juce::Colour DesignTokens::rotaryCapTop() const {
    return parseColor("rotary-cap-top", juce::Colour(0xff353942));
}
juce::Colour DesignTokens::rotaryCapBottom() const {
    return parseColor("rotary-cap-bottom", juce::Colour(0xff1a1c20));
}
juce::Colour DesignTokens::rotaryCapRim() const {
    return parseColor("rotary-cap-rim", juce::Colour(0xff101114));
}
juce::Colour DesignTokens::rotaryRingTop() const {
    return parseColor("rotary-ring-top", juce::Colour(0xff565c69));
}
juce::Colour DesignTokens::rotaryRingBottom() const {
    return parseColor("rotary-ring-bottom", juce::Colour(0xff141518));
}

// ── Typography ─────────────────────────────────────────────────

float DesignTokens::fontSizeTiny() const {
    return parseFloat("typography", "font-size-tiny", 11.0f);
}
float DesignTokens::fontSizeSmall() const {
    return parseFloat("typography", "font-size-small", 12.0f);
}
float DesignTokens::fontSizeDefault() const {
    return parseFloat("typography", "font-size-default", 13.0f);
}
float DesignTokens::fontSizeLabel() const {
    return parseFloat("typography", "font-size-label", 14.0f);
}
float DesignTokens::fontSizeTitle() const {
    return parseFloat("typography", "font-size-title", 18.0f);
}
juce::String DesignTokens::fontWeightTitle() const {
    return parseString("typography", "font-weight-title", "bold");
}

// ── Border Radius ──────────────────────────────────────────────

float DesignTokens::borderRadiusDefault() const {
    return parseFloat("border-radius", "default", 6.0f);
}

// ── Spacing & Dimensions ───────────────────────────────────────

int DesignTokens::windowDefaultWidth() const {
    return parseInt("spacing", "window-default-width", 1180);
}
int DesignTokens::windowDefaultHeight() const {
    return parseInt("spacing", "window-default-height", 780);
}
int DesignTokens::windowMinWidth() const {
    return parseInt("spacing", "window-min-width", 980);
}
int DesignTokens::windowMinHeight() const {
    return parseInt("spacing", "window-min-height", 700);
}
int DesignTokens::windowMaxWidth() const {
    return parseInt("spacing", "window-max-width", 3840);
}
int DesignTokens::windowMaxHeight() const {
    return parseInt("spacing", "window-max-height", 2160);
}
int DesignTokens::statusBarHeight() const {
    return parseInt("spacing", "status-bar-height", 24);
}
int DesignTokens::settingsBtnWidth() const {
    return parseInt("spacing", "settings-btn-width", 36);
}

static juce::String formatCssHexColour(juce::Colour colour) {
    // 格式化为标准 CSS Hex: "#RRGGBB"（不透明时）或 "#RRGGBBAA"（含透明度时）。
    // 彻底消除由于 JUCE toDisplayString(false) 剥离高位导致的零前导与透明度异常。
    const auto r = juce::String::toHexString(colour.getRed()).toUpperCase().paddedLeft('0', 2);
    const auto g = juce::String::toHexString(colour.getGreen()).toUpperCase().paddedLeft('0', 2);
    const auto b = juce::String::toHexString(colour.getBlue()).toUpperCase().paddedLeft('0', 2);
    if (colour.getAlpha() == 255) {
        return "#" + r + g + b;
    }
    const auto a = juce::String::toHexString(colour.getAlpha()).toUpperCase().paddedLeft('0', 2);
    return "#" + r + g + b + a;
}

/// 选择平台首选 UI 字体族。
///
/// JUCE 在 Linux 上对字体族名做 FreeType 精确匹配（不走 fontconfig 别名），
/// 因此必须返回系统真实安装的字体名；候选链按平台优先级排列，并通过
/// getTypefacePtr() 探测存在性（FTTypefaceList 精确匹配成功才算存在），
/// 全部缺失时回退到 JUCE 的 "<System-UI>" 占位符（走 fontconfig system-ui）。
static juce::String resolveUiFontFamily() {
    juce::StringArray candidates;
#if JUCE_LINUX
    // Linux 主流发行版预装的高清 CJK 黑体（覆盖中文与西文字形）
    candidates = { "Noto Sans CJK SC",    "Source Han Sans SC", "Source Han Sans CN", "Noto Sans SC",
                   "WenQuanYi Micro Hei", "WenQuanYi Zen Hei",  "DejaVu Sans" };
#elif JUCE_MAC
    candidates = { "PingFang SC", "Hiragino Sans GB" };
#else
    candidates = { "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI" };
#endif

    for (const auto& name : candidates) {
        if (juce::Font(juce::FontOptions(name, 14.0f, juce::Font::plain)).getTypefacePtr().get() != nullptr) {
            return name;
        }
    }
    return juce::Font::getSystemUIFontName();
}

juce::String DesignTokens::resolveToken(const juce::String& name) const {
    // 颜色：经 getter 解析（JSON 未加载时回退到内置默认，与 shipped 文件一致）。
    if (name == "main-bg") {
        return formatCssHexColour(mainBg());
    }
    if (name == "panel-bg") {
        return formatCssHexColour(panelBg());
    }
    if (name == "control-bg") {
        return formatCssHexColour(controlBg());
    }
    if (name == "card-bg") {
        return formatCssHexColour(panelBg()); // card-bg 与 panel-bg 同值
    }
    if (name == "card-border" || name == "border") {
        return formatCssHexColour(cardBorder());
    }
    if (name == "primary") {
        return formatCssHexColour(primary());
    }
    if (name == "primary-alpha-30") {
        return formatCssHexColour(primaryAlpha30());
    }
    if (name == "record-active") {
        return formatCssHexColour(recordActive());
    }
    if (name == "play-active") {
        return formatCssHexColour(playActive());
    }
    if (name == "text-primary") {
        return formatCssHexColour(textPrimary());
    }
    if (name == "text-secondary") {
        return formatCssHexColour(textSecondary());
    }
    if (name == "text-disabled") {
        return formatCssHexColour(textDisabled());
    }
    if (name == "font-family-ui") {
        return resolveUiFontFamily();
    }
    // 字号：整数字符串以匹配 style_sheets.json 现有写法（"14" 而非 "14.0"）。
    if (name == "font-size-tiny") {
        return juce::String(static_cast<int>(fontSizeTiny()));
    }
    if (name == "font-size-default") {
        return juce::String(static_cast<int>(fontSizeDefault()));
    }
    if (name == "font-size-label") {
        return juce::String(static_cast<int>(fontSizeLabel()));
    }
    if (name == "font-size-title") {
        return juce::String(static_cast<int>(fontSizeTitle()));
    }
    if (name == "font-weight-title") {
        return fontWeightTitle();
    }
    return {};
}

} // namespace devpiano::jive
