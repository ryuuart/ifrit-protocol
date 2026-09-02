/** @file
 * The trilinear 3D-LUT recipe, which needs nothing of OpenColorIO and so
 * is compiled whether or not the build found it.
 */

#include "sigilmaterial/ocio/Ocio.h"

namespace sigil::material::ocio {

const std::shared_ptr<const Recipe>& lutRecipe() {
  static const auto recipe =
      std::make_shared<const Recipe>(Recipe::of<LutParams>("color.lut3d")
                                         .child("content")
                                         .child("lut")
                                         .body(Target::SkSL, R"(
half4 main(float2 xy) {
  half4 c = content.eval(xy);
  float a = max(float(c.a), 0.0001);
  float3 rgb = clamp(float3(c.rgb) / a, 0.0, 1.0);
  float n = lutSize;
  float3 p = rgb * (n - 1.0);
  float zf = floor(p.z);
  float zc = min(zf + 1.0, n - 1.0);
  float t = p.z - zf;
  float2 uv0 = float2(zf * n + p.x + 0.5, p.y + 0.5);
  float2 uv1 = float2(zc * n + p.x + 0.5, p.y + 0.5);
  float3 m = mix(float3(lut.eval(uv0).rgb), float3(lut.eval(uv1).rgb), t);
  return half4(half3(m * a), c.a);
}
)"));
  return recipe;
}

}  // namespace sigil::material::ocio
