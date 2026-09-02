#pragma once
// Support for compose_paint_test: SigilMaterial's tiles, fields and SDF
// surfaces as a node's fill, layer styles and colour management, over the
// brush support.

#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Gel.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/sdf/Sdf.h>
#ifdef SIGILMATERIAL_ENABLE_OCIO
#include <sigilmaterial/color/Ocio.h>
#endif

#include "BrushTestSupport.h"
