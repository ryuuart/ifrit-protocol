/** @file
 * The programs this backend draws with: the scaffold in its two builds,
 * the sky, the post stages, and the compiler registration that appends a
 * recipe's body to the scaffold.
 */

#include "Programs.h"

#include <sigilshaders/WorldDiligent.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/diligent/Runtime.h>

#include <mutex>
#include <string>
#include <string_view>

namespace sigil::world::diligent {

namespace {

using material::slang::Compiled;

/** THE LOBES A BODY STATES, as the definitions that compile the scaffold's
 *  terms for them in: a body that writes a lobe's variable states that
 *  lobe, and one that writes none of them is compiled against exactly the
 *  scaffold every other body is. The variables are the scaffold's, so this
 *  is the one place that knows which name opens which lobe. */
std::string lobeDefinitions(std::string_view body) {
  struct Lobe {
    std::string_view variable;
    std::string_view definition;
  };
  static constexpr Lobe kLobes[] = {
      {"gSurfaceSheenColor", "#define SIGIL_SHEEN 1\n"},
      {"gSurfaceAnisotropy", "#define SIGIL_ANISOTROPY 1\n"},
      {"gSurfaceIridescence", "#define SIGIL_IRIDESCENCE 1\n"},
      {"gSurfaceSpecular", "#define SIGIL_SPECULAR 1\n"},
  };
  std::string out;
  for (const Lobe& lobe : kLobes)
    if (body.find(lobe.variable) != std::string_view::npos)
      out += lobe.definition;
  return out;
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
    one("fsFinal", &built.final);
    one("fsFill", &built.fill);
    one("fsThreshold", &built.threshold);
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
          const std::string body = recipe->source(material::Target::Slang);
          std::string source = lobeDefinitions(body);
          source += shaderSource("Surface.slang");
          source += '\n';
          source += body;
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
