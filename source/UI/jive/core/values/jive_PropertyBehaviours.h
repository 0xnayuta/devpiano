//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

namespace jive {
enum class Inheritance {
    inheritFromParent,
    inheritFromAncestors,
    doNotInherit,
};

enum class Accumulation {
    accumulate,
    doNotAccumulate,
};

enum class Responsiveness { respondToChanges, ignoreChanges };
} // namespace jive
