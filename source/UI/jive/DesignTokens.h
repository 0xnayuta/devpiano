#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace devpiano::jive {

/// Single source of truth for design tokens (colors, fonts, spacing).
///
/// Loaded once at startup from design_tokens.json; falls back to hardcoded
/// defaults matching the shipped design_tokens.json if the file is missing
/// or malformed.
class DesignTokens {
public:
    DesignTokens(const DesignTokens&) = delete;
    DesignTokens& operator=(const DesignTokens&) = delete;

    /// Returns the global singleton.
    static DesignTokens& get();

    /// Parse the design_tokens.json root object. Safe to call once.
    /// On parse failure or missing keys, keeps the built-in defaults.
    void loadFromJSON(const juce::var& json);

    // ── Colors ──────────────────────────────────────────
    [[nodiscard]] juce::Colour mainBg() const;
    [[nodiscard]] juce::Colour panelBg() const;
    [[nodiscard]] juce::Colour controlBg() const;
    [[nodiscard]] juce::Colour cardBorder() const;
    [[nodiscard]] juce::Colour primary() const;
    [[nodiscard]] juce::Colour primaryAlpha30() const;
    [[nodiscard]] juce::Colour recordActive() const;
    [[nodiscard]] juce::Colour playActive() const;
    [[nodiscard]] juce::Colour textPrimary() const;
    [[nodiscard]] juce::Colour textSecondary() const;
    [[nodiscard]] juce::Colour textDisabled() const;
    [[nodiscard]] juce::Colour highlightOverlay() const;
    [[nodiscard]] juce::Colour pressOverlay() const;
    [[nodiscard]] juce::Colour rotaryBgTrack() const;
    [[nodiscard]] juce::Colour rotaryCapTop() const;
    [[nodiscard]] juce::Colour rotaryCapBottom() const;
    [[nodiscard]] juce::Colour rotaryCapRim() const;
    [[nodiscard]] juce::Colour rotaryRingTop() const;
    [[nodiscard]] juce::Colour rotaryRingBottom() const;

    // ── Typography ──────────────────────────────────────
    [[nodiscard]] float fontSizeTiny() const;
    [[nodiscard]] float fontSizeSmall() const;
    [[nodiscard]] float fontSizeDefault() const;
    [[nodiscard]] float fontSizeLabel() const;
    [[nodiscard]] float fontSizeTitle() const;
    [[nodiscard]] juce::String fontWeightTitle() const;
    // ── Border Radius ───────────────────────────────────
    [[nodiscard]] float borderRadiusDefault() const;

    // ── Spacing & Dimensions ────────────────────────────
    [[nodiscard]] int windowDefaultWidth() const;
    [[nodiscard]] int windowDefaultHeight() const;
    [[nodiscard]] int windowMinWidth() const;
    [[nodiscard]] int windowMinHeight() const;
    [[nodiscard]] int windowMaxWidth() const;
    [[nodiscard]] int windowMaxHeight() const;
    [[nodiscard]] int statusBarHeight() const;
    [[nodiscard]] int settingsBtnWidth() const;

    // ── Token lookup for style sheets (DOC-007) ──────────
    // Resolve a token name (without the leading '@') to its style-sheet
    // representation: colours as "#RRGGBB", typography as integer strings
    // ("14"), font-weight as-is.  Resolves through the getters so the
    // built-in defaults apply when design_tokens.json has not been loaded.
    // Returns empty string for unknown token names.
    [[nodiscard]] juce::String resolveToken(const juce::String& name) const;

    // Test hook: snapshot the current token root for save/restore across
    // unit-test cases (the singleton is shared process-wide).
    [[nodiscard]] juce::var currentRootForTest() const {
        return root.get();
    }

    // Reset to the pristine empty state (TEST-013): clears the loaded JSON so
    // every getter falls back to its built-in default.  Lets a test file
    // establish an independent baseline instead of depending on which other
    // file loaded design_tokens.json first.
    void reset() {
        root = new juce::DynamicObject();
    }

private:
    DesignTokens() = default;

    juce::DynamicObject::Ptr root { new juce::DynamicObject() };

    [[nodiscard]] juce::DynamicObject::Ptr colorsNode() const;
    [[nodiscard]] juce::DynamicObject::Ptr typographyNode() const;
    [[nodiscard]] juce::DynamicObject::Ptr borderRadiusNode() const;
    [[nodiscard]] juce::DynamicObject::Ptr spacingNode() const;
    [[nodiscard]] juce::Colour parseColor(juce::StringRef key, juce::Colour fallback) const;
    [[nodiscard]] float parseFloat(juce::StringRef section, juce::StringRef key, float fallback) const;
    [[nodiscard]] int parseInt(juce::StringRef section, juce::StringRef key, int fallback) const;
    [[nodiscard]] juce::String parseString(juce::StringRef section, juce::StringRef key, juce::String fallback) const;
};

} // namespace devpiano::jive
