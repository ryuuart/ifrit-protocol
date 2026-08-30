#pragma once

/** @file
 * SigilCompose paint — the feature umbrella: pattern tiles and the stock
 * generators, SDF materials, layer styles, and the OCIO view transform
 * where the build found OpenColorIO.
 */

#include "sigilcompose/paint/LayerStyles.h"
#include "sigilcompose/paint/Pattern.h"
#include "sigilcompose/paint/Patterns.h"
#include "sigilcompose/paint/Sdf.h"
#ifdef SIGILCOMPOSE_ENABLE_OCIO
#include "sigilcompose/paint/Ocio.h"
#endif
