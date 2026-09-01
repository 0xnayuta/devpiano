//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#pragma once

namespace jive {
template <class... Variants> struct Visitor : Variants... {
    using Variants::operator()...;
};

template <class... Variants> Visitor(Variants...) -> Visitor<Variants...>;
} // namespace jive
