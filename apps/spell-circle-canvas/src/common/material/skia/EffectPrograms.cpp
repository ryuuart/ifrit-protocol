/** @file
 * THE BODIES AN EFFECT IS BUILT OUT OF, compiled once and shared: the
 * bright pass and the phosphor halo a bloom gathers, the tap that lays
 * that halo back over the source, the deepening and whitening of a
 * light, and the mix a parametric blur interpolates through.
 *
 * One list rather than one compiled behind each factory, because a
 * device backend that gives each of these a name it can write down
 * gives that name to the OBJECT and not to its source: the list has to
 * be whole, in one fixed order, and made of the very objects the
 * factories go on to use.
 */

#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilshaders/MaterialSkia.h>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "EffectInternal.h"

namespace sigil::material::skia {

namespace {

/** How a body is compiled: one that reads its own pixel and no
 *  neighbour is a colour map and is made for a colour filter; one that
 *  samples the layer around it is made for a shader. Which of the two a
 *  body is, is a property of how it is written, so it is stated beside
 *  the file rather than guessed from the source. */
enum class Reads { OnePixel, TheLayer };

struct Body {
  const char* file;
  Reads reads;
};

/** Every body, in EffectProgram's order. */
constexpr std::array<Body, (size_t)EffectProgram::Count> kBodies{{
    {"BrightPass.sksl", Reads::OnePixel},
    {"PhosphorHalo.sksl", Reads::TheLayer},
    {"PhosphorComposite.sksl", Reads::TheLayer},
    {"HaloDeepening.sksl", Reads::OnePixel},
    {"CoreWhitening.sksl", Reads::OnePixel},
    {"ParametricBlurMix.sksl", Reads::TheLayer},
}};

/** The compiled table, and the list without the holes.
 *
 *  A body that will not compile is null in `held` — the position still
 *  means what it meant, so a call site asking for it gets nothing and
 *  says so in its own words — and is simply absent from `compiled`,
 *  which is the list a backend is offered. A machine that loses a body
 *  therefore offers a SHORTER list, which is a different list: anything
 *  keyed on it is keyed on what is really there. */
struct Table {
  Table() {
    for (size_t at = 0; at < kBodies.size(); ++at) {
      const Body& body = kBodies[at];
      const SkString source(shaderSource(body.file));
      auto [program, error] = body.reads == Reads::OnePixel
                                  ? SkRuntimeEffect::MakeForColorFilter(source)
                                  : SkRuntimeEffect::MakeForShader(source);
      if (!program) {
        SkDebugf("[material] skia::Effect: %s failed: %s\n", body.file,
                 error.c_str());
        continue;
      }
      compiled.push_back(program);
      held[at] = std::move(program);
    }
  }

  std::array<sk_sp<SkRuntimeEffect>, kBodies.size()> held;
  std::vector<sk_sp<SkRuntimeEffect>> compiled;
};

const Table& table() {
  static const Table one;
  return one;
}

}  // namespace

const sk_sp<SkRuntimeEffect>& effectProgram(EffectProgram which) {
  return table().held[(size_t)which];
}

std::span<const sk_sp<SkRuntimeEffect>> everyEffectProgram() {
  return table().compiled;
}

}  // namespace sigil::material::skia
