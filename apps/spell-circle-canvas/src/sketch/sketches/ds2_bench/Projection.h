#pragma once

// THE PROJECTION: what light thrown onto nothing does to a picture. Its
// three primaries leave the emitter a little apart, so each is read
// `uFringe` px from the others across the picture's width and every edge
// carries red on one side and blue on the other; the throw is swept in
// rows, so every `uRowPitch` px the lower half of a row gives up
// `uRowDepth` of its light; and the light is uneven, a grain of
// `uGrain` in how much of it each pixel keeps. It runs over the picture
// in the `content` slot, which is light on nothing: premultiplied, and
// clear where nothing is lit.

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>

#include <cmath>
#include <string>
#include <string_view>

namespace ds2_bench {

struct ProjectionParameters {
  float uFringe = 1.5f;
  float uRowPitch = 3.0f;
  float uRowDepth = 0.3f;
  float uGrain = 0.08f;
};

inline constexpr std::string_view kProjectionSkSL = R"SHADER(
half4 main(float2 p) {
  half4 middle = content.eval(p);
  half4 left = content.eval(p - float2(uFringe, 0));
  half4 right = content.eval(p + float2(uFringe, 0));
  // Each primary where its own beam put it; the light covers what any of
  // the three covers.
  half alpha = max(middle.a, max(left.a, right.a));
  half3 light = half3(left.r, middle.g, right.b);
  float row = mod(p.y, uRowPitch) < uRowPitch * 0.5 ? 0.0 : uRowDepth;
  float grain = (fract(sin(dot(floor(p), float2(12.9898, 78.233))) * 43758.5453)
                 - 0.5) * uGrain;
  half keep = half(clamp(1.0 - row + grain, 0.0, 1.0));
  return half4(min(light, half3(alpha)), alpha) * keep;
}
)SHADER";

inline sigil::material::Material projection(
    const ProjectionParameters& parameters = {}) {
  return sigil::material::shader(std::string(kProjectionSkSL), parameters,
                                 {.key = "ds2_bench.projection",
                                  .textures = {{"content", {}}}});
}

/** How far the projection reads away from the pixel it paints. */
inline float projectionReach(const ProjectionParameters& parameters) {
  return std::abs(parameters.uFringe);
}

}  // namespace ds2_bench
