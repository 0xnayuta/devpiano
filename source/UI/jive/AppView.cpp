#include "AppView.h"

#include "LayoutModel.h"
#include "UI/jive/DesignTokens.h"

namespace devpiano::ui::jive {

// ═══════════════════════════════════════════════════════════════════════════
// Icon helpers (DrawablePath factories, shared by createComponent below)
// ═══════════════════════════════════════════════════════════════════════════

namespace {

std::unique_ptr<juce::Drawable> createGearIcon() {
    juce::Path p;
    p.addEllipse(-8, -8, 16, 16);
    for (int i = 0; i < 4; ++i) {
        auto angle = juce::MathConstants<float>::halfPi * static_cast<float>(i) - juce::MathConstants<float>::pi / 4.0f;
        auto cx = 9.0f * std::cos(angle);
        auto cy = 9.0f * std::sin(angle);
        p.addRectangle(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }
    p.setUsingNonZeroWinding(true);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    return d;
}

std::unique_ptr<juce::Drawable> createRecordIcon() {
    juce::Path p;
    p.addEllipse(-6, -6, 12, 12);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    return d;
}

std::unique_ptr<juce::Drawable> createPlayIcon() {
    juce::Path p;
    p.addTriangle(-5, -6, -5, 6, 5, 0);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    return d;
}

std::unique_ptr<juce::Drawable> createStopIcon() {
    juce::Path p;
    p.addRectangle(-5, -5, 10, 10);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    return d;
}

std::unique_ptr<juce::Drawable> createBackIcon() {
    juce::Path p;
    p.addTriangle(-4, -5, 6, 0, -4, 5);
    p.addRectangle(-6, -5, 3, 10);
    auto d = std::make_unique<juce::DrawablePath>();
    d->setPath(p);
    return d;
}

/// Create a DrawableButton with icon images.
std::unique_ptr<juce::DrawableButton> makeDrawableButton(const juce::String& name,
                                                         std::unique_ptr<juce::Drawable> normalIcon,
                                                         std::unique_ptr<juce::Drawable> overIcon) {
    auto btn = std::make_unique<juce::DrawableButton>(name, juce::DrawableButton::ImageFitted);
    btn->setImages(normalIcon.get(), overIcon.get(), nullptr);
    // The button takes ownership of the images via internal copies.
    // We pass pointers to the Drawable objects; JUCE copies them internally.
    return btn;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
// AppView implementation
// ═══════════════════════════════════════════════════════════════════════════

AppView::AppView() = default;

juce::ValueTree AppView::initialise() {
    return makeRootLayout();
}

std::unique_ptr<juce::Component> AppView::createComponent(const juce::ValueTree& tree) {
    auto id = tree["id"].toString();
    const auto& tokens = ::devpiano::jive::DesignTokens::get();

    // Settings gear button (HeaderPanel)
    if (id == "settings-btn") {
        auto normal = createGearIcon();
        auto over = createGearIcon();
        static_cast<juce::DrawablePath*>(normal.get())->setFill(tokens.textSecondary());
        static_cast<juce::DrawablePath*>(over.get())->setFill(tokens.primary());
        auto btn = makeDrawableButton("settings", std::move(normal), std::move(over));
        btn->setTooltip("Settings");
        return btn;
    }

    // Transport buttons (ControlsPanel)
    if (id == "record-btn") {
        auto normal = createRecordIcon();
        auto over = createRecordIcon();
        static_cast<juce::DrawablePath*>(normal.get())->setFill(tokens.recordActive());
        static_cast<juce::DrawablePath*>(over.get())->setFill(tokens.recordActive().brighter(0.3f));
        auto btn = makeDrawableButton("record", std::move(normal), std::move(over));
        btn->setTooltip("Record");
        return btn;
    }

    if (id == "play-btn") {
        auto normal = createPlayIcon();
        auto over = createPlayIcon();
        static_cast<juce::DrawablePath*>(normal.get())->setFill(tokens.playActive());
        static_cast<juce::DrawablePath*>(over.get())->setFill(tokens.playActive().brighter(0.3f));
        auto btn = makeDrawableButton("play", std::move(normal), std::move(over));
        btn->setTooltip("Play");
        return btn;
    }

    if (id == "stop-btn") {
        auto normal = createStopIcon();
        auto over = createStopIcon();
        static_cast<juce::DrawablePath*>(normal.get())->setFill(tokens.textSecondary());
        static_cast<juce::DrawablePath*>(over.get())->setFill(tokens.textPrimary());
        auto btn = makeDrawableButton("stop", std::move(normal), std::move(over));
        btn->setTooltip("Stop");
        return btn;
    }

    if (id == "back-btn") {
        auto normal = createBackIcon();
        auto over = createBackIcon();
        static_cast<juce::DrawablePath*>(normal.get())->setFill(tokens.textSecondary());
        static_cast<juce::DrawablePath*>(over.get())->setFill(tokens.textPrimary());
        auto btn = makeDrawableButton("back", std::move(normal), std::move(over));
        btn->setTooltip("Back to Start");
        return btn;
    }

    // Let JIVE choose the default for everything else
    return nullptr;
}
void AppView::setup(::jive::GuiItem& /*item*/) {
    // Phase 11d: callback wiring goes here.
    // e.g. use jive::findItemWithID(root, "scan-btn") to get a GuiItem*
}

// ═══════════════════════════════════════════════════════════════════════════

} // namespace devpiano::ui::jive
