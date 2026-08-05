#pragma once

/** @file
 * SigilCompose × OpenColorIO — color management for the Composer's output
 * stage. Compiled in only when the build found OpenColorIO; guarded by
 * SIGILCOMPOSE_ENABLE_OCIO.
 *
 * How it works, and why not the obvious way: OCIO's own GPU codegen emits
 * GLSL/HLSL/MSL/OSL and never SkSL, so it cannot be used directly here.
 * Instead each factory below builds a CPU processor for the requested
 * transform, bakes it into a 3D LUT once at construction, uploads the LUT as
 * an SkImage, and applies it per frame through a small SkSL trilinear sampler
 * wrapped in an SkImageFilter. The result is an ordinary `Effect` — hand it
 * to `Composer::setView()` for the whole output, or to any node's
 * `.effect()`. Nothing of OCIO proper (config parsing, processors, CPU
 * evaluation) runs per frame; the frame carries only the baked LUT texture
 * and one sample per pixel.
 *
 * Color contract: authored colors are treated as the transform's INPUT space.
 * For a display/view transform, author in the config's scene-linear role and
 * the view maps linear → display. No view is set by default; setting one
 * costs a saveLayer over the output for as long as it is set.
 *
 * LUTs bake to F16 because F32 textures are not linearly filterable on Apple
 * GPUs — a trilinear sampler over an F32 LUT would fall back to point
 * sampling and band.
 */

#include <string_view>

#include "sigilcompose/Compose.h"

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

}  // namespace sigil::compose::ocio
