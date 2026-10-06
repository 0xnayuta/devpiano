#pragma once

#include "../Audio/AcousticSnapshot.h"
#include "../Layout/PerformancePreset.h"

namespace devpiano::recording {

struct RecordedPreset {
    devpiano::layout::PerformancePreset preset;
    devpiano::audio::AcousticSnapshot acoustic;
};

} // namespace devpiano::recording
