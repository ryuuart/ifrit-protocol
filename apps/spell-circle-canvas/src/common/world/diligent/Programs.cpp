/** @file
 * The programs this backend draws with: the scaffold in its two builds,
 * the sky, the post stages, and the compiler registration that appends a
 * recipe's body to the scaffold.
 */

#include "Programs.h"

#include <sigilmaterial/surface/Surface.h>
#include <sigilshaders/WorldDiligent.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/diligent/Runtime.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sigil::world::diligent {

namespace {

using material::slang::Compiled;

/** WHICH VARIABLE OPENS WHICH LOBE: a body that writes a lobe's variable
 *  states that lobe. The variables are the scaffold's, so this is the one
 *  place that knows the names. */
struct LobeVariable {
  std::string_view variable;
  std::string_view definition;
  bool material::Lobes::* lobe;
};
constexpr LobeVariable kLobeVariables[] = {
    {"gSurfaceSheenColor", "#define SIGIL_SHEEN 1\n", &material::Lobes::sheen},
    {"gSurfaceAnisotropy", "#define SIGIL_ANISOTROPY 1\n",
     &material::Lobes::anisotropy},
    {"gSurfaceIridescence", "#define SIGIL_IRIDESCENCE 1\n",
     &material::Lobes::iridescence},
    {"gSurfaceSpecular", "#define SIGIL_SPECULAR 1\n",
     &material::Lobes::specular},
    {"gSurfaceTransmission", "#define SIGIL_TRANSMISSION 1\n",
     &material::Lobes::transmission},
    {"gSurfaceDistortion", "#define SIGIL_DISTORTION 1\n",
     &material::Lobes::distortion},
    {"gSurfaceClearcoat", "#define SIGIL_CLEARCOAT 1\n",
     &material::Lobes::clearcoat},
};

/** THE LOBES A BODY STATES, as the definitions that compile the scaffold's
 *  terms for them in; one that writes none of them is compiled against
 *  exactly the scaffold every other body is. */
std::string lobeDefinitions(std::string_view body) {
  std::string out;
  for (const LobeVariable& lobe : kLobeVariables)
    if (body.find(lobe.variable) != std::string_view::npos)
      out += lobe.definition;
  return out;
}

/** THE LIGHTS A LIT BUILD IS SPECIALISED ON, as the definitions that
 *  compile the scaffold's terms for them in. */
std::string lightDefinitions(uint32_t variant) {
  std::string out;
  if (variant & kVariantAreaLights) out += "#define SIGIL_AREA_LIGHTS 1\n";
  if (variant & kVariantCascadedShadows)
    out += "#define SIGIL_CASCADED_SHADOWS 1\n";
  if (variant & kVariantPointShadows) out += "#define SIGIL_POINT_SHADOWS 1\n";
  if (variant & kVariantTintedShadows)
    out += "#define SIGIL_TINTED_SHADOWS 1\n";
  return out;
}

/** The variant bits that name the lights a lit build is specialised on. */
constexpr uint32_t kLightVariants =
    kVariantAreaLights | kVariantCascadedShadows | kVariantPointShadows |
    kVariantTintedShadows;

}  // namespace

material::Lobes statedLobes(const material::Material& material) {
  if (!material.hasProgram()) return {};
  const std::shared_ptr<const material::Recipe>& recipe =
      material.recipePointer();
  struct Classified {
    std::weak_ptr<const material::Recipe> recipe;
    material::Lobes lobes;
  };
  static std::mutex mutex;
  static std::unordered_map<const material::Recipe*, Classified> classified;
  const std::lock_guard lock(mutex);
  // A recipe is told from a later one at the same address by its owner,
  // so an entry outliving its recipe is classified again.
  const auto held = classified.find(recipe.get());
  if (held != classified.end() && !held->second.recipe.owner_before(recipe) &&
      !recipe.owner_before(held->second.recipe))
    return held->second.lobes;
  material::Lobes stated = material::surface::lobes(material);
  if (const std::string* body = recipe->body(material::Target::Slang))
    for (const LobeVariable& lobe : kLobeVariables)
      if (body->find(lobe.variable) != std::string::npos)
        stated.*lobe.lobe = true;
  if (held == classified.end() && classified.size() >= 256)
    std::erase_if(classified, [](const auto& entry) {
      return entry.second.recipe.expired();
    });
  classified[recipe.get()] = {recipe, stated};
  return stated;
}

const Compiled& scaffold(bool lit, uint32_t lights) {
  const auto build = [](bool shaded, uint32_t reaching) {
    Compiled built;
    std::string error;
    const std::string source =
        lightDefinitions(reaching) + std::string(shaderSource("Surface.slang"));
    if (!material::slang::compileModule(source, "vsMain", "fsFlat", shaded,
                                        &built, &error))
      material::reportOnce("world.diligent.scaffold",
                           "the surface scaffold did not compile: " + error);
    return built;
  };
  static const Compiled plain = build(false, 0u);
  if (!lit) return plain;
  // One lit build per set of lights, made on first asking and kept for
  // the process: there are only as many as the light bits can spell.
  static std::mutex mutex;
  static std::array<std::unique_ptr<Compiled>, kLightVariants + 1> shaded;
  const uint32_t reaching = lights & kLightVariants;
  const std::lock_guard lock(mutex);
  std::unique_ptr<Compiled>& made = shaded[reaching];
  if (!made) made = std::make_unique<Compiled>(build(true, reaching));
  return *made;
}

const Compiled& shadowScaffold(bool coverage) {
  const auto build = [](const char* entry) {
    Compiled built;
    std::string error;
    if (!material::slang::compileModule(shaderSource("Surface.slang"), "vsMain",
                                        entry, /*lit=*/false, &built, &error))
      material::reportOnce(std::string("world.diligent.shadow.") + entry,
                           "the shadow stage did not compile: " + error);
    return built;
  };
  static const Compiled depthOnly = build("fsShadow");
  static const Compiled covered = build("fsShadowFlat");
  return coverage ? covered : depthOnly;
}

const Compiled& tintScaffold() {
  static const Compiled built = [] {
    Compiled program;
    std::string error;
    if (!material::slang::compileModule(shaderSource("Surface.slang"), "vsMain",
                                        "fsShadowTint", /*lit=*/false, &program,
                                        &error))
      material::reportOnce("world.diligent.shadow.tint",
                           "the shadow stage's tint did not compile: " + error);
    return program;
  }();
  return built;
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
    one("fsDown", &built.down);
    one("fsDownThreshold", &built.downThreshold);
    one("fsUp", &built.up);
    one("fsBloom", &built.bloom);
    one("fsComposite", &built.composite);
    one("fsMaskedLevels", &built.maskedLevels);
    one("fsPrefilter", &built.prefilter);
    return built;
  }();
  return programs;
}

namespace {

/** Whether every fixed program has been made. */
std::atomic<bool>& fixedReady() {
  static std::atomic<bool> ready{false};
  return ready;
}

}  // namespace

void prepareFixedPrograms() {
  // ON THE THREAD THAT BRINGS THE DEVICE UP, and not on a worker of its
  // own: the compiler and its store are statics a worker still compiling
  // when the process ends would outlive.
  static std::once_flag once;
  std::call_once(once, [] {
    (void)scaffold(false);
    for (const uint32_t lights :
         std::array<uint32_t, 4>{0u, kVariantAreaLights,
                                 kVariantCascadedShadows, kVariantPointShadows})
      (void)scaffold(true, lights);
    (void)shadowScaffold(false);
    (void)shadowScaffold(true);
    (void)tintScaffold();
    (void)backdropProgram();
    (void)postPrograms();
    fixedReady() = true;
  });
}

bool fixedProgramsReady() { return fixedReady().load(); }

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
          const bool shadow = variant.has(kVariantShadow);
          const bool lit = !shadow && variant.has(kVariantLit);
          std::string source = lobeDefinitions(body);
          if (lit) source += lightDefinitions(variant.bits);
          source += shaderSource("Surface.slang");
          source += '\n';
          source += body;
          source += shaderSource("MaterialEntry.slang");
          // The shadow stage's build asks the body only where it covers,
          // and is never lit.
          Compiled built;
          if (!material::slang::compileModule(
                  source, "vsMain", shadow ? "fsMaterialShadow" : "fsMaterial",
                  lit, &built, &error))
            return nullptr;
          return std::make_shared<material::slang::SlangProgram>(
              std::move(recipe), variant, std::move(built));
        });
  });
}

}  // namespace sigil::world::diligent
