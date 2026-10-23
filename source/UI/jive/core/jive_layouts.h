//==============================================================================
// This file is derived from JIVE (https://github.com/ImJimmi/JIVE)
// Copyright (c) 2021 James Johnson
// Licensed under the MIT License.
// Adapted and maintained as part of the devpiano UI Infrastructure (ADR-014).
//==============================================================================
#pragma once

#if defined(DEVPIANO_UI_WITH_STYLES) && !defined(JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS)
#define JIVE_GUI_ITEMS_HAVE_STYLE_SHEETS DEVPIANO_UI_WITH_STYLES
#endif
#if defined(DEVPIANO_UI_ENABLE_GRID) && !defined(JIVE_ENABLE_GRID)
#define JIVE_ENABLE_GRID DEVPIANO_UI_ENABLE_GRID
#endif
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

#include "UI/jive/core/layout/jive_ComponentFactory.h"
#include "UI/jive/core/layout/jive_Display.h"
#include "UI/jive/core/layout/jive_LayoutStrategy.h"
#include "UI/jive/core/layout/jive_Overflow.h"

#include "UI/jive/core/layout/jive_CommonGuiItem.h"
#include "UI/jive/core/layout/jive_ContainerItem.h"
#include "UI/jive/core/layout/jive_GuiItem.h"
#include "UI/jive/core/layout/jive_GuiItemDecorator.h"

#include "UI/jive/core/layout/jive_BlockContainer.h"
#include "UI/jive/core/layout/jive_BlockItem.h"
#include "UI/jive/core/layout/jive_FlexContainer.h"
#include "UI/jive/core/layout/jive_FlexItem.h"

#include "UI/jive/core/layout/jive_Button.h"
#include "UI/jive/core/layout/jive_ComboBox.h"
#include "UI/jive/core/layout/jive_ProgressBar.h"
#include "UI/jive/core/layout/jive_Slider.h"
#include "UI/jive/core/layout/jive_Text.h"

#include "UI/jive/core/layout/jive_Interpreter.h"
