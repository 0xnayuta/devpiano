//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================

#include "jive_ComponentFactory.h"

#include "jive_components.h"

namespace jive {
ComponentFactory::ComponentFactory() {
    creators.insert({
        "Button",
        []() { return std::make_unique<juce::TextButton>(); },
    });
    creators.insert({
        "Checkbox",
        []() { return std::make_unique<juce::ToggleButton>(); },
    });
    creators.insert({
        "ComboBox",
        []() { return std::make_unique<juce::ComboBox>(); },
    });
    creators.insert({
        "Component",
        []() { return std::make_unique<IgnoredComponent>(); },
    });
    creators.insert({
        "Editor",
        []() { return std::make_unique<IgnoredComponent>(); },
    });
    creators.insert({
        "Label",
        []() { return std::make_unique<juce::Label>(); },
    });
    creators.insert({
        "ProgressBar",
        []() { return std::make_unique<NormalisedProgressBar>(); },
    });
    creators.insert({
        "Slider",
        []() { return std::make_unique<juce::Slider>(); },
    });
    creators.insert({
        "Text",
        []() { return std::make_unique<TextComponent>(); },
    });
}

std::unique_ptr<juce::Component> ComponentFactory::create(juce::Identifier name) const {
    auto nameFactoryPair = creators.find(name);

    if (nameFactoryPair == std::end(creators)) {
        return nullptr;
    }

    return nameFactoryPair->second();
}

void ComponentFactory::set(juce::Identifier name, ComponentCreator creator) {
    creators.insert({ name, creator });
}
} // namespace jive
