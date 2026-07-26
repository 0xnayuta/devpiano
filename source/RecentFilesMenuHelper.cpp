void MainComponent::showRecentFilesMenu() {
    juce::PopupMenu menu;
    recentFiles.removeNonExistentFiles();

    const auto numFiles = recentFiles.getNumFiles();
    int itemId = 1;

    if (numFiles == 0) {
        menu.addItem(0, TRANS("(no recent files)"), false, false);
    } else {
        for (int i = 0; i < numFiles; ++i) {
            auto file = recentFiles.getFile(i);
            auto name = file.getFileName();
            auto ext = file.getFileExtension().toLowerCase();
            juce::String prefix;
            if (ext == ".devpiano")
                prefix = juce::String::fromUTF8("\xe2\x99\xaa "); // ♪
            else if (ext == ".mid" || ext == ".midi")
                prefix = juce::String::fromUTF8("\xe2\x99\xab "); // ♫
            else
                prefix = "? ";

            menu.addItem(itemId, prefix + name);
            ++itemId;
        }
    }

    int clearId = itemId;
    if (numFiles > 0) {
        menu.addSeparator();
        clearId = itemId;
        menu.addItem(clearId, TRANS("Clear Recent Files"));
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(getRecentFilesButtonScreenBounds()),
                       [safe = juce::Component::SafePointer<MainComponent>(this), numFiles, clearId](int result) {
                           if (safe == nullptr)
                               return;

                           if (result == 0)
                               return;

                           if (result == clearId) {
                               safe->recentFiles.clear();
                               safe->saveRecentFiles();
                               return;
                           }

                           const auto index = result - 1;
                           if (!juce::isPositiveAndBelow(index, numFiles))
                               return;

                           auto file = safe->recentFiles.getFile(index);
                           if (!file.exists())
                               return;

                           auto ext = file.getFileExtension().toLowerCase();
                           if (ext == ".devpiano")
                               safe->recordingSessionController->handleOpenPerformanceFile(file);
                           else if (ext == ".mid" || ext == ".midi")
                               safe->recordingSessionController->handleImportMidiFile(file);
                       });
}
