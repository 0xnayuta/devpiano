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

} // namespace devpiano::test
