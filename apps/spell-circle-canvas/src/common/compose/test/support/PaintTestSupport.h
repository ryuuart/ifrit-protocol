#pragma once
// Support for compose_paint_test: patterns, SDF materials, layer styles
// and colour management, over the brush support.

#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Patterns.h>
#include <sigilcompose/core/Sdf.h>
#ifdef SIGILMATERIAL_ENABLE_OCIO
#include <sigilmaterial/color/Ocio.h>
#endif

#include "BrushTestSupport.h"
