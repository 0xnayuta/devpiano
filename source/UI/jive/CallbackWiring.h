#pragma once

#include <jive_layouts/jive_layouts.h>

class MainComponent;

namespace devpiano::ui::jive {

/// Wire all application UI callbacks to JIVE-rendered components.
///
/// Finds components by their "id" attribute in the JIVE GuiItem tree
/// and connects onClick / onChange / onNoteOn / etc. to MainComponent.
///
/// Called once from MainComponent::initialiseUi() after interpretation.
void wireAllCallbacks(::jive::GuiItem& rootItem, MainComponent& mc);

} // namespace devpiano::ui::jive
