/** @file
 * The effect value's members that carry a diagnostic: the pass factory,
 * which takes only a recipe-backed material, and the two declarations —
 * where a pass rests, whether a body displaces — that mean nothing on the
 * wrong kind of effect and say so once. Every other member of the value
 * is defined with it in its header, because the kernel compares and
 * evaluates effects and links no engine to do it.
 */

#include <include/core/SkTypes.h>
#include <sigilcompose/typography/TextEffect.h>
#include <sigilmaterial/core/Material.h>

#include <utility>

namespace sigil::compose {

TextEffect TextEffect::pass(material::skia::Paint material) {
  const sigil::material::Material* backing = material.recipeMaterial();
  if (!backing || !backing->recipe().has(sigil::material::Target::SkSL)) {
    // Once per process: the door takes only the recipe-backed form,
    // because the runtime specializes the recipe per unit count and needs
    // the SkSL body to do it.
    static bool warned = false;
    if (!warned) {
      warned = true;
      SkDebugf(
          "[compose] fx::pass: the material carries no SkSL recipe — a "
          "pass is compiled per unit count, which needs "
          "material::skia::Paint::recipe(...) over a recipe with an SkSL body. "
          "The "
          "effect is empty and the track draws its glyphs at rest.\n");
    }
    return {};
  }
  auto state = std::make_shared<State>();
  state->name = "pass";
  // The identity body keeps the value truthy (a track with an empty effect
  // is skipped) and keeps a pass harmless as a seq/mix/hold operand, where
  // only the deviation is consulted.
  state->fn = [](const GlyphInfo&, float, core::noise::Mix64Stream&) {
    return GlyphMod{};
  };
  // A pass paints where its material says it does; the material's declared
  // reserve is the effect's reach, and Track::reach overrides as ever.
  state->reach = material.bleed();
  // A PASS IS NOT A PLACEMENT. Its shader reads a layer whose glyphs were
  // rasterized at their RESTING origins — the pass moves pixels, not pen
  // positions — so putting those origins on the subpixel grid refines masks
  // the shader's own output does not follow, and pays the multiplied atlas
  // population for letters that are provably standing still. Whatever the
  // pass does with them, the layer is re-rendered every frame it runs.
  state->displaces = false;
  state->pass =
      std::make_shared<const material::skia::Paint>(std::move(material));
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

TextEffect TextEffect::withRests(std::initializer_list<float> phases) const {
  if (!m_state || !m_state->pass) {
    // Once per process: the declaration is about a pass's SkSL, and a
    // per-glyph effect has no shader to promise anything about.
    static bool warned = false;
    if (!warned) {
      warned = true;
      SkDebugf(
          "[compose] restsAt() declares where a PASS's shader is an exact "
          "pass-through; this effect carries no pass material, so the "
          "declaration says nothing and is dropped.\n");
    }
    return *this;
  }
  auto state = std::make_shared<State>(*m_state);
  state->params.insert(state->params.end(), phases.begin(), phases.end());
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

TextEffect TextEffect::restsAt(float phase) const { return withRests({phase}); }
TextEffect TextEffect::restsAt(float a, float b) const {
  return withRests({a, b});
}

TextEffect TextEffect::displacing(bool moves) const {
  if (!m_state) return *this;
  if (m_state->pass) {
    // Once per process: a pass runs over already-rasterized pixels, so it
    // has no pen position to move and the declaration says nothing. Its
    // params slot is the rest declaration, which this must not write into.
    static bool warned = false;
    if (!warned) {
      warned = true;
      SkDebugf(
          "[compose] displacing() declares whether a body moves glyphs off "
          "their pen positions; a pass runs over pixels already rasterized "
          "at the resting origins, so the declaration is dropped.\n");
    }
    return *this;
  }
  auto state = std::make_shared<State>(*m_state);
  state->displaces = moves;
  // The declaration JOINS THE PARAMS, which is where an effect's identity
  // already lives — no second equality lane, and two bodies under one key
  // that disagree about placement compare unequal and re-patch. Every
  // library-built effect answers at construction and never comes through
  // here, so nothing appends twice.
  state->params.push_back(moves ? 1.0f : 0.0f);
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

}  // namespace sigil::compose
