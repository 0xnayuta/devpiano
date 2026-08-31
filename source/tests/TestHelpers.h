#pragma once

#include <JuceHeader.h>
#include <cmath>

namespace devpiano::test {

// ============================================================================
// ScopedTempDir: RAII temporary directory manager for unit tests.
//
// Guaranteed cleanup on scope exit (destruction, exceptions, early returns).
// Always created under the system temporary directory (/tmp on Linux), never
// polluting the user's home directory.
// ============================================================================
class ScopedTempDir final {
public:
    explicit ScopedTempDir(const juce::String& tag) {
        const auto randSuffix = juce::String(std::abs(juce::Random::getSystemRandom().nextInt64()));
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                  .getChildFile("devpiano-test-" + tag + "-" + randSuffix);
        dir.createDirectory();
    }

    ~ScopedTempDir() {
        if (dir.exists()) {
            dir.deleteRecursively();
        }
    }

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;

    ScopedTempDir(ScopedTempDir&& other) noexcept
        : dir(other.dir) {
        other.dir = juce::File();
    }

    ScopedTempDir& operator=(ScopedTempDir&& other) noexcept {
        if (this != &other) {
            if (dir.exists()) {
                dir.deleteRecursively();
            }
            dir = other.dir;
            other.dir = juce::File();
        }
        return *this;
    }

    [[nodiscard]] const juce::File& get() const noexcept {
        return dir;
    }

    [[nodiscard]] juce::File getChildFile(const juce::String& name) const {
        return dir.getChildFile(name);
    }

    [[nodiscard]] operator const juce::File&() const noexcept {
        return dir;
    }

private:
    juce::File dir;
};

// ============================================================================
// findRepoRoot: Robustly locates repository root directory across all environments.
//
// Multi-tier lookup:
// 1. Absolute __FILE__ up-traversal (works in standard builds)
// 2. Current working directory up-traversal (works in CTest / CLI runs)
// 3. Current executable directory up-traversal (works when run from build artefact folders)
// ============================================================================
inline juce::File findRepoRoot() {
    auto isRepoRoot = [](const juce::File& dir) {
        return dir.isDirectory() && dir.getChildFile("tests/fixtures").isDirectory()
            && dir.getChildFile("source").isDirectory();
    };

    // 1. __FILE__ when absolute
    if (juce::File::isAbsolutePath(__FILE__)) {
        const juce::File sourceFile(__FILE__);
        auto root = sourceFile.getParentDirectory().getParentDirectory().getParentDirectory();
        if (isRepoRoot(root)) {
            return root;
        }
    }

    // 2. Upwards from current working directory
    for (auto dir = juce::File::getCurrentWorkingDirectory(); dir.exists() && dir.getParentDirectory() != dir;
         dir = dir.getParentDirectory()) {
        if (isRepoRoot(dir)) {
            return dir;
        }
    }

    // 3. Upwards from current executable
    for (auto dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
         dir.exists() && dir.getParentDirectory() != dir; dir = dir.getParentDirectory()) {
        if (isRepoRoot(dir)) {
            return dir;
        }
    }

    // 4. Fallback directly to CWD if tests/fixtures exists right there
    if (auto direct = juce::File::getCurrentWorkingDirectory(); isRepoRoot(direct)) {
        return direct;
    }

    return {};
}

inline juce::File getFixturesDir() {
    return findRepoRoot().getChildFile("tests/fixtures");
}

} // namespace devpiano::test
