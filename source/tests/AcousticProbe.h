#pragma once

#include <juce_core/juce_core.h>

namespace devpiano::test {

// 47-1 offline acoustic probe. Renders deterministic single-note WAV cases
// through the production offline export path and writes a manifest for the
// Python analyzer. This is a test-side development tool, not a product entry.
int runAcousticProbe(const juce::StringArray& args);

} // namespace devpiano::test