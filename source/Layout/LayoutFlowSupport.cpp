#include "LayoutFlowSupport.h"

#include "Diagnostics/DebugLog.h"

#include "Layout/LayoutDirectoryScanner.h"
#include "Layout/LayoutPreset.h"
#include "MainComponent.h"

namespace devpiano::layout
{
namespace
{
[[nodiscard]] std::optional<std::pair<juce::File, devpiano::core::KeyboardLayout>>
findUserLayoutFileById(const juce::String& layoutId)
{
    if (layoutId.isEmpty() || layoutId.startsWith("default."))
        return std::nullopt;

    auto dir = devpiano::layout::getUserLayoutDirectory();
    for (const auto& entry : dir.findChildFiles(juce::File::TypesOfFileToFind::findFiles, false, "*.freepiano.layout"))
    {
        auto loaded = devpiano::layout::loadLayoutPreset(entry);
        if (!loaded.has_value())
            continue;

        loaded->id = devpiano::layout::getUserLayoutIdForFile(entry);
        if (loaded->name.trim().isEmpty())
            loaded->name = devpiano::layout::getLayoutPresetDisplayNameForFile(entry);

        if (loaded->id == layoutId)
            return std::make_pair(entry, *loaded);
    }

    return std::nullopt;
}
} // namespace

LayoutFlowSupport::LayoutFlowSupport(MainComponent& ownerIn)
    : owner(ownerIn)
{
}

LayoutFlowSupport::~LayoutFlowSupport() = default;

void LayoutFlowSupport::handleLayoutChanged(const juce::String& newLayoutId)
{
    const auto nextLayoutId = newLayoutId.trim();
    if (nextLayoutId.isEmpty() || nextLayoutId == owner.keyboardMidiMapper.getLayout().id)
    {
        owner.restoreKeyboardFocus();
        return;
    }

    applyLayoutAndCommit(SettingsModel::keyMapToLayout({}, nextLayoutId));
}

void LayoutFlowSupport::handleSaveLayoutRequested()
{
    runLayoutFileChooser("Save Layout",
                         devpiano::layout::getUserLayoutDirectory(),
                         juce::FileBrowserComponent::saveMode,
                         saveLayoutChooser,
                         [this](const juce::File& file)
    {
        auto currentLayout = owner.keyboardMidiMapper.getLayout();
        const auto targetLayoutId = devpiano::layout::getUserLayoutIdForFile(file);
        currentLayout.id = targetLayoutId;
        if (currentLayout.name.trim().isEmpty())
            currentLayout.name = devpiano::layout::getLayoutPresetDisplayNameForFile(file);

        auto saved = devpiano::layout::saveLayoutPreset(currentLayout, file);
        if (saved)
            DP_LOG_INFO("[Layout] saved: " + file.getFullPathName());
        else
            DP_LOG_ERROR("[Layout] save FAILED: " + file.getFullPathName());
        if (saved)
            applyLayoutAndCommit(currentLayout);
    });
}

void LayoutFlowSupport::handleResetLayoutToDefaultRequested()
{
    const auto currentLayoutId = owner.keyboardMidiMapper.getLayout().id.trim();
    const auto layoutId = currentLayoutId.isNotEmpty() ? currentLayoutId
                                                       : owner.appSettings.getInputMappingSettingsView().layoutId;

    applyLayoutAndCommit(SettingsModel::keyMapToLayout({}, layoutId));
}

void LayoutFlowSupport::handleImportLayoutRequested()
{
    runLayoutFileChooser("Import Layout",
                         juce::File{},
                         juce::FileBrowserComponent::openMode,
                         importLayoutChooser,
                         [this](const juce::File& file)
    {
        auto optLayout = devpiano::layout::loadLayoutPreset(file);
        if (!optLayout.has_value())
            return;

        auto layout = *optLayout;
        auto userDir = devpiano::layout::getUserLayoutDirectory();

        auto originalName = file.getFileName();
        auto destFile = userDir.getChildFile(originalName);
        layout.id = devpiano::layout::getUserLayoutIdForFile(destFile);
        if (layout.name.trim().isEmpty())
            layout.name = devpiano::layout::getLayoutPresetDisplayNameForFile(destFile);

        if (!devpiano::layout::saveLayoutPreset(layout, destFile))
            return;

        applyLayoutAndCommit(layout);
    });
}

void LayoutFlowSupport::handleRenameLayoutRequested()
{
    const auto layoutId = owner.controlsPanel.getSelectedLayoutId().trim();
    if (layoutId.isEmpty() || layoutId.startsWith("default."))
        return;

    auto layoutFile = findUserLayoutFileById(layoutId);
    if (!layoutFile.has_value())
        return;

    const auto& [fileToRename, loadedLayout] = *layoutFile;
    auto currentDisplayName = loadedLayout.name.trim();
    if (currentDisplayName.isEmpty())
        currentDisplayName = devpiano::layout::getLayoutPresetDisplayNameForFile(fileToRename);

    runLayoutRenameDialog(layoutId, fileToRename, currentDisplayName);
}

void LayoutFlowSupport::handleDeleteLayoutRequested()
{
    auto layoutId = owner.controlsPanel.getSelectedLayoutId();
    if (layoutId.isEmpty() || layoutId.startsWith("default."))
        return;

    auto layoutName = layoutId;
    juce::File fileToDelete;
    if (auto layoutFile = findUserLayoutFileById(layoutId); layoutFile.has_value())
    {
        layoutName = layoutFile->second.name.isNotEmpty() ? layoutFile->second.name : layoutId;
        fileToDelete = layoutFile->first;
    }

    runLayoutDeleteDialog(layoutId, fileToDelete, layoutName);
}

void LayoutFlowSupport::applyLayoutAndCommit(const devpiano::core::KeyboardLayout& layout)
{
    owner.audioEngine.getKeyboardState().allNotesOff(1);
    owner.keyboardMidiMapper.setLayout(layout);
    owner.appSettings.applyInputMappingSettingsView(
        { .layoutId = owner.keyboardMidiMapper.getLayout().id,
          .keyMap = SettingsModel::layoutToKeyMap(owner.keyboardMidiMapper.getLayout()) });
    owner.syncUiFromSettings();
    owner.saveSettingsSoon();
    owner.restoreKeyboardFocus();
}

void LayoutFlowSupport::runLayoutFileChooser(const juce::String& title,
                                              const juce::File& startingDir,
                                              juce::FileBrowserComponent::FileChooserFlags chooserFlags,
                                              std::unique_ptr<juce::FileChooser>& chooser,
                                              std::function<void(const juce::File&)> onResult)
{
    chooser = std::make_unique<juce::FileChooser>(title, startingDir, "*.freepiano.layout");
    chooser->launchAsync(chooserFlags, [&chooser, resultCallback = std::move(onResult)](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;

        resultCallback(file);
        chooser.reset();
    });
}

void LayoutFlowSupport::runLayoutRenameDialog(const juce::String& layoutId,
                                               const juce::File& /*file*/,
                                               const juce::String& currentDisplayName)
{
    auto* renameWindow = new juce::AlertWindow("Rename Layout",
                                               "Set the display name shown in the layout dropdown.",
                                               juce::AlertWindow::NoIcon);
    renameWindow->addTextEditor("displayName", currentDisplayName, "Display Name:");
    renameWindow->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    renameWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeOwner = juce::Component::SafePointer<MainComponent>(&owner);
    juce::Component::SafePointer<juce::AlertWindow> safeWindow(renameWindow);
    renameWindow->enterModalState(true,
                                  juce::ModalCallbackFunction::create([safeOwner, safeWindow, layoutId](int result)
                                  {
                                      if (result != 1 || safeOwner == nullptr || safeWindow == nullptr)
                                          return;

                                      auto updatedDisplayName = safeWindow->getTextEditorContents("displayName").trim();
                                      if (updatedDisplayName.isEmpty())
                                          return;

                                      auto layoutFileToUpdate = findUserLayoutFileById(layoutId);
                                      if (!layoutFileToUpdate.has_value())
                                          return;

                                      auto [resolvedFile, layout] = *layoutFileToUpdate;
                                      layout.name = updatedDisplayName;
                                      if (!devpiano::layout::saveLayoutPreset(layout, resolvedFile))
                                          return;

                                      if (safeOwner->keyboardMidiMapper.getLayout().id == layoutId)
                                      {
                                          safeOwner->keyboardMidiMapper.setLayoutDisplayName(updatedDisplayName);
                                          safeOwner->appSettings.applyInputMappingSettingsView(
                                              { .layoutId = layoutId,
                                                .keyMap = SettingsModel::layoutToKeyMap(safeOwner->keyboardMidiMapper.getLayout()) });
                                      }

                                      safeOwner->syncUiFromSettings();
                                      safeOwner->saveSettingsSoon();
                                      safeOwner->restoreKeyboardFocus();
                                  }),
                                  true);
}

void LayoutFlowSupport::runLayoutDeleteDialog(const juce::String& layoutId,
                                               const juce::File& file,
                                               const juce::String& displayName)
{
    auto safeOwner = juce::Component::SafePointer<MainComponent>(&owner);
    auto capturedLayoutId = layoutId;
    auto capturedFileToDelete = file;

    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions::makeOptionsOkCancel(
            juce::AlertWindow::WarningIcon,
            "Delete Layout",
            "Are you sure you want to delete \"" + displayName + "\"?\nThis action cannot be undone.",
            "Delete",
            "Cancel"),
        [safeOwner, capturedLayoutId, capturedFileToDelete](int result)
        {
            if (result != 1 || safeOwner == nullptr)
                return;

            if (capturedFileToDelete.exists())
                capturedFileToDelete.deleteFile();

            if (safeOwner->keyboardMidiMapper.getLayout().id == capturedLayoutId)
            {
                safeOwner->audioEngine.getKeyboardState().allNotesOff(1);
                safeOwner->keyboardMidiMapper.setLayout(devpiano::core::makeDefaultKeyboardLayout());
                safeOwner->appSettings.applyInputMappingSettingsView(
                    { .layoutId = safeOwner->keyboardMidiMapper.getLayout().id,
                      .keyMap = SettingsModel::layoutToKeyMap(safeOwner->keyboardMidiMapper.getLayout()) });
            }
            safeOwner->syncUiFromSettings();
            safeOwner->saveSettingsSoon();
            safeOwner->restoreKeyboardFocus();
        });
}

} // namespace devpiano::layout
