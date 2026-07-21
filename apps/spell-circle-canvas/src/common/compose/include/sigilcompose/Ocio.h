#pragma once

/** @file
 * SigilCompose × OpenColorIO — VFX-grade color management for the Composer's
 * output stage. Available when the build found OpenColorIO (already in this
 * repo's dependency closure via OpenImageIO); guarded by
 * SIGILCOMPOSE_ENABLE_OCIO.
 *
 * The pattern (the sanctioned real-time OCIO path — its GPU codegen emits
 * GLSL/HLSL/MSL/OSL, not SkSL, so we sidestep it): build a CPU processor for
 * the requested transform, bake it into a 3D LUT once at setup, upload the
 * LUT as an SkImage, and apply it per frame with a tiny SkSL trilinear
 * sampler wrapped in an SkImageFilter. The result is an ordinary `Effect` —
 * hand it to `Composer::setView()` (the whole-output view transform) or to
 * any node's `.effect()`.
 *
 * Color contract: authored colors are treated as the transform's INPUT space.
 * For a display/view transform, author in the config's scene-linear role and
 * the view maps linear → display. Off by default; costs one saveLayer while
 * set.
 */

#include "sigilcompose/Compose.h"

#include <string_view>

namespace sigil::compose::ocio {

/** True when OCIO support was compiled in AND the runtime can create its
 *  built-in raw config. */
bool available();

/**
 * An OCIO display/view, baked to a LUT Effect. `config` is a filesystem path
 * to a .ocio/.ocioz config, or an "ocio://" URI for the ASWF built-in configs
 * ("ocio://default", "ocio://studio-config-latest", "ocio://cg-config-latest").
 * `display`/`view` name a (display, view) pair from that config (e.g.
 * "sRGB - Display" / "ACES 1.0 - SDR Video" in the built-ins). Input is the
 * config's scene_linear role. Returns a pass-through Effect (null filter) and
 * logs on failure — a bad config name must not take the canvas down.
 */
Effect display(std::string_view config, std::string_view display,
               std::string_view view, int lutSize = 33);

/**
 * A color-space conversion `src` → `dst` from the same config sources —
 * "ACEScg" → "sRGB - Texture" and friends. Same failure contract.
 */
Effect convert(std::string_view config, std::string_view src,
               std::string_view dst, int lutSize = 33);

/**
 * A plain exponent (gamma) transform baked through OCIO's raw config —
 * needs no config file. Useful as a quick grade and as the plumbing test.
 */
Effect exponent(float gamma, int lutSize = 33);

} // namespace sigil::compose::ocio
