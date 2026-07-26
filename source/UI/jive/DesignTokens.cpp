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

juce::Colour DesignTokens::parseColor(juce::StringRef key,
                                      juce::Colour fallback) const {
    if (auto node = colorsNode()) {
        const auto v = node->getProperty(juce::Identifier(juce::String{ key }));
        if (!v.isVoid())
            return juce::Colour::fromString(v.toString());
    }
    return fallback;
}

float DesignTokens::parseFloat(juce::StringRef section,
                                juce::StringRef key,
                                float fallback) const {
    juce::DynamicObject::Ptr node;
    const juce::String sec{ section };
    if (sec == "typography") {
        node = typographyNode();
    } else if (sec == "border-radius") {
        node = borderRadiusNode();
    } else if (sec == "spacing") {
        node = spacingNode();
    }
    if (node != nullptr) {
        const auto v = node->getProperty(juce::Identifier(juce::String{ key }));
        if (!v.isVoid()) {
            return static_cast<float>(v);
        }
    }
    return fallback;
}

int DesignTokens::parseInt(juce::StringRef /*section*/,
                            juce::StringRef key,
                            int fallback) const {
    if (auto node = spacingNode()) {
        const auto v = node->getProperty(juce::Identifier(juce::String{ key }));
        if (!v.isVoid()) {
            return static_cast<int>(v);
        }
    }
    return fallback;
}

juce::String DesignTokens::parseString(juce::StringRef /*section*/,
                                        juce::StringRef key,
                                        juce::String fallback) const {
    if (auto node = typographyNode()) {
        const auto v = node->getProperty(juce::Identifier(juce::String{ key }));
        if (!v.isVoid()) {
            return v.toString();
        }
    }
    return fallback;
}

// ── Colors ─────────────────────────────────────────────────────

juce::Colour DesignTokens::mainBg() const {
    return parseColor("main-bg", juce::Colour(0xff1a1c1e));
}
juce::Colour DesignTokens::panelBg() const {
    return parseColor("panel-bg", juce::Colour(0xff24262a));
}
juce::Colour DesignTokens::controlBg() const {
    return parseColor("control-bg", juce::Colour(0xff2d3035));
}
juce::Colour DesignTokens::primary() const {
    return parseColor("primary", juce::Colour(0xff00b4d8));
}
juce::Colour DesignTokens::primaryAlpha30() const {
    return parseColor("primary-alpha-30",
                      juce::Colour(0xff00b4d8).withAlpha(0.3f));
}
juce::Colour DesignTokens::recordActive() const {
    return parseColor("record-active", juce::Colour(0xffe07b3c));
}
juce::Colour DesignTokens::playActive() const {
    return parseColor("play-active", juce::Colour(0xff4ecdc4));
}
juce::Colour DesignTokens::textPrimary() const {
    return parseColor("text-primary", juce::Colour(0xffeeeeee));
}
juce::Colour DesignTokens::textSecondary() const {
    return parseColor("text-secondary", juce::Colour(0xff999999));
}
juce::Colour DesignTokens::textDisabled() const {
    return parseColor("text-disabled", juce::Colour(0xff555555));
}
juce::Colour DesignTokens::highlightOverlay() const {
    return parseColor("highlight-overlay",
                      juce::Colours::white.withAlpha(0.08f));
}
juce::Colour DesignTokens::pressOverlay() const {
    return parseColor("press-overlay",
                      juce::Colours::black.withAlpha(0.18f));
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
    return parseFloat("border-radius", "default", 4.0f);
}

// ── Spacing & Dimensions ───────────────────────────────────────

int DesignTokens::windowDefaultWidth() const {
    return parseInt("spacing", "window-default-width", 1120);
}
int DesignTokens::windowDefaultHeight() const {
    return parseInt("spacing", "window-default-height", 760);
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
    return parseInt("spacing", "status-bar-height", 22);
}
int DesignTokens::settingsBtnWidth() const {
    return parseInt("spacing", "settings-btn-width", 36);
}

} // namespace devpiano::jive
