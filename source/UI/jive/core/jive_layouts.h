//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================
#pragma once

#include "jive_components.h"

#if JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS
#include "jive_style_sheets.h"
#endif

namespace juce {
class AudioProcessor;
}

namespace jive {
class GuiItem;
}

#include "jive_ComponentFactory.h"
#include "jive_Display.h"
#include "jive_LayoutStrategy.h"
#include "jive_Overflow.h"

#include "jive_View.h"

#include "jive_CommonGuiItem.h"
#include "jive_ContainerItem.h"
#include "jive_GuiItem.h"
#include "jive_GuiItemDecorator.h"

#include "jive_BlockContainer.h"
#include "jive_BlockItem.h"
#include "jive_FlexContainer.h"
#include "jive_FlexItem.h"

#include "jive_Button.h"
#include "jive_ComboBox.h"
#include "jive_Label.h"
#include "jive_ProgressBar.h"
#include "jive_Slider.h"
#include "jive_Text.h"

#include "jive_Interpreter.h"
