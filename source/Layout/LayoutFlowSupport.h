#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

#include "Core/KeyMapTypes.h"

class MainComponent;

namespace devpiano::layout
{

class LayoutFlowSupport final
{
public:
    explicit LayoutFlowSupport(MainComponent& owner);
    ~LayoutFlowSupport();

    void handleLayoutChanged(const juce::String& newLayoutId);
    void handleSaveLayoutRequested();
    void handleResetLayoutToDefaultRequested();
    void handleImportLayoutRequested();
    void handleRenameLayoutRequested();
    void handleDeleteLayoutRequested();

private:
    void applyLayoutAndCommit(const devpiano::core::KeyboardLayout& layout);

    void runLayoutFileChooser(const juce::String& title,
                              const juce::File& startingDir,
                              juce::FileBrowserComponent::FileChooserFlags flags,
                              std::unique_ptr<juce::FileChooser>& chooser,
                              std::function<void(const juce::File&)> onResult);

    void runLayoutRenameDialog(const juce::String& layoutId,
                               const juce::File& file,
                               const juce::String& currentDisplayName);

    void runLayoutDeleteDialog(const juce::String& layoutId,
                               const juce::File& file,
                               const juce::String& displayName);

    MainComponent& owner;

    std::unique_ptr<juce::FileChooser> saveLayoutChooser;
    std::unique_ptr<juce::FileChooser> importLayoutChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayoutFlowSupport)
};

} // namespace devpiano::layout
