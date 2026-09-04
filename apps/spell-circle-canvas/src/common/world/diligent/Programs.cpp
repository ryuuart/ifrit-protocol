/** @file
 * The programs this backend draws with: the scaffold in its two builds,
 * the sky, the mesh painter, the post stages, and the compiler
 * registration that appends a recipe's body to the scaffold.
 */

#include "Programs.h"

#include <sigilio/hub/Hub.h>
#include <sigilworld/diligent/Runtime.h>

#include <mutex>
#include <string>
#include <string_view>

namespace sigil::world::diligent {

namespace {

using material::slang::Compiled;

constexpr char kShaderPrefix[] = "shader://world/diligent/";

struct ShaderResources {
  ShaderResources() {
    hub.mount(kShaderPrefix, SIGIL_WORLD_SHADER_DIR);
    retained = hub.retain(kShaderPrefix);
  }

  io::Hub hub;
  io::ResourceLease retained;
};

ShaderResources& shaders() {
  static ShaderResources resources;
  return resources;
}

std::string shaderSource(std::string_view name) {
  return shaders()
      .hub.text(std::string(kShaderPrefix) + std::string(name))
      .value_or("");
}

}  // namespace

const Compiled& scaffold(bool lit) {
  const auto build = [](bool shaded) {
    Compiled built;
    std::string error;
    if (!material::slang::compileModule(shaderSource("Surface.slang"), "vsMain",
                                        "fsFlat", shaded, &built, &error))
      material::reportOnce("world.diligent.scaffold",
                           "the surface scaffold did not compile: " + error);
    return built;
  };
  static const Compiled shaded = build(true);
  static const Compiled plain = build(false);
  return lit ? shaded : plain;
}

const Compiled& backdropProgram() {
  static const Compiled built = [] {
    Compiled program;
    std::string error;
    // The LIT build, always: the sky's uniforms are the lit scaffold's,
    // and a build with no lighting in it does not declare them.
    if (!material::slang::compileModule(shaderSource("Surface.slang"),
                                        "vsBackdrop", "fsBackdrop",
                                        /*lit=*/true, &program, &error))
      material::reportOnce("world.diligent.backdrop",
                           "the sky pass did not compile: " + error);
    return program;
  }();
  return built;
}

const Compiled& painterProgram() {
  static const Compiled built = [] {
    Compiled program;
    std::string error;
    // Compiled once and unspecialised: the painter's modes and its
    // "does light reach this" answer are fields of a style, which is a
    // value a caller changes between two draws, so specialising on one
    // would compile a program per draw rather than per material.
    if (!material::slang::compileModule(shaderSource("Painter.slang"),
                                        "vsPaint", "fsPaint", /*lit=*/false,
                                        &program, &error))
      material::reportOnce("world.diligent.painter",
                           "the mesh painter did not compile: " + error);
    return program;
  }();
  return built;
}

const PostPrograms& postPrograms() {
  static const PostPrograms programs = [] {
    PostPrograms built;
    const auto one = [](const char* entry, Compiled* into) {
      std::string error;
      if (!material::slang::compileModule(shaderSource("Post.slang"),
                                          "vsFullscreen", entry, /*lit=*/false,
                                          into, &error))
        material::reportOnce(std::string("world.diligent.post.") + entry,
                             std::string("the post stage ") + entry +
                                 " did not compile: " + error);
    };
    one("fsCopy", &built.copy);
    one("fsBlur", &built.blur);
    one("fsLevels", &built.levels);
    one("fsMasked", &built.masked);
    return built;
  }();
  return programs;
}

void installSlangCompiler() {
  static std::once_flag once;
  std::call_once(once, [] {
    material::registerCompiler(
        material::Target::Slang,
        [](std::shared_ptr<const material::Recipe> recipe,
           material::Variant variant,
           std::string& error) -> std::shared_ptr<material::Program> {
          // The scaffold first, so its uniforms and its VSOut stand
          // before the recipe's text; the recipe's declarations and body
          // next; the fragment entry that calls the body last, because
          // `surface` is not visible until the body has defined it.
          std::string source = shaderSource("Surface.slang");
          source += '\n';
          source += recipe->source(material::Target::Slang);
          source += shaderSource("MaterialEntry.slang");
          Compiled built;
          if (!material::slang::compileModule(source, "vsMain", "fsMaterial",
                                              variant.has(kVariantLit), &built,
                                              &error))
            return nullptr;
          return std::make_shared<material::slang::SlangProgram>(
              std::move(recipe), variant, std::move(built));
        });
  });
}

}  // namespace sigil::world::diligent
