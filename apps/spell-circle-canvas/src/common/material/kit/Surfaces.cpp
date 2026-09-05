/** @file
 * The gold, chrome and glass bodies in SkSL — the shared prelude of
 * normal decode, equirect lookup and value noise, then each reflection
 * model — and the builders that fill their texture slots.
 */

#include "sigilmaterial/kit/Surfaces.h"

#include <sigilshaders/MaterialKit.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace sigil::material::kit {

namespace {

glm::vec2 sizeOf(const EnvironmentMap& env) {
  const SkISize s = env.size();
  return {(float)std::max(s.width(), 1), (float)std::max(s.height(), 1)};
}

}  // namespace

const std::shared_ptr<const Recipe>& goldRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GoldParams>("gold").child("normals").child("env").body(
          Target::SkSL, std::string(shaderSource("NoisePrelude.sksl"))
                            .append(shaderSource("ReflectivePrelude.sksl"))
                            .append(shaderSource("ReflectiveGold.sksl"))));
  return recipe;
}

const std::shared_ptr<const Recipe>& chromeRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<ChromeParams>("chrome").child("normals").child("env").body(
          Target::SkSL, std::string(shaderSource("NoisePrelude.sksl"))
                            .append(shaderSource("ReflectivePrelude.sksl"))
                            .append(shaderSource("ReflectiveChrome.sksl"))));
  return recipe;
}

const std::shared_ptr<const Recipe>& glassRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GlassParams>("glass")
          .child("normals")
          .child("env")
          .child("backdrop")
          .body(Target::SkSL,
                std::string(shaderSource("NoisePrelude.sksl"))
                    .append(shaderSource("ReflectivePrelude.sksl"))
                    .append(shaderSource("ReflectiveGlass.sksl"))));
  return recipe;
}

Material gold(Texture normals, const EnvironmentMap& env,
              const GoldParams& params) {
  GoldParams p = params;
  p.envSize = sizeOf(env);
  Material m(goldRecipe(), p);
  m.child("normals", std::move(normals));
  m.child("env", env.texture(params.roughness));
  return m;
}

Material chrome(Texture normals, const EnvironmentMap& env,
                const ChromeParams& params) {
  ChromeParams p = params;
  p.envSize = sizeOf(env);
  Material m(chromeRecipe(), p);
  m.child("normals", std::move(normals));
  m.child("env", env.texture(params.roughness));
  return m;
}

Material glass(Texture normals, const EnvironmentMap& env, Texture backdrop,
               const GlassParams& params) {
  GlassParams p = params;
  p.envSize = sizeOf(env);
  Material m(glassRecipe(), p);
  m.child("normals", std::move(normals));
  m.child("env", env.texture(params.roughness));
  m.child("backdrop", std::move(backdrop));
  return m;
}

}  // namespace sigil::material::kit
