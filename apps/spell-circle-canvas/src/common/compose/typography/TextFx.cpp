/** @file
 * THE EFFECTS THE RUNTIME EVALUATES BY STRUCTURE — the sequence, the
 * keyframe table, the hold, the scramble, the mix and the pass. Each is a
 * comparable `TextEffect` carrying its operands, so a composite compares
 * by structure and a re-described track prunes; the value type itself is
 * defined with its header.
 *
 * Nothing here touches an Instance or a canvas. TextFxPainting.cpp drives
 * it: build the structure once per frame, resolve each track's selection,
 * then walk the glyphs asking for each one's local time and composing the
 * deviations.
 */

#include <sigilcompose/typography/TextEffect.h>
#include <sigilweave/choreograph/Choreograph.h>  // the ease the tables take

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "TextEngine.h"

namespace sigil::compose {

// ---------------------------------------------------------------------------
// The combinators

namespace fx {

namespace {
/** A combinator's placement fact, derived from what it may evaluate: it
 *  moves glyphs when any operand does. Deriving rather than declaring is
 *  what keeps the answer exact through nesting — a `fx::mix` of three tints
 *  and one `fx::rise` displaces, the same mix without the rise does not. */
template <typename Range>
bool anyDisplaces(const Range& operands) {
  for (const auto& operand : operands)
    if (operand.displaces()) return true;
  return false;
}
}  // namespace

TextEffect sequence(std::vector<Phase> phases) {
  if (phases.empty()) return TextEffect();
  // The last phase always runs to the end of local time, whatever it was
  // declared with — otherwise the tail of every sequence is undefined.
  phases.back() = Phase(phases.back().effect(), 1.0f).crossfade(0.0f);

  std::vector<TextEffect> operands;
  std::vector<float> parameters;
  operands.reserve(phases.size());
  parameters.reserve(phases.size() * 2);
  float reach = 0;
  for (const Phase& p : phases) {
    operands.push_back(p.effect());
    parameters.push_back(p.endsAt());
    parameters.push_back(p.overlap());
    reach = std::max(reach, p.effect().reach());
  }
  const bool displaces = anyDisplaces(operands);
  return TextEffect::composite(
      "sequence", parameters, operands,
      [phases](const GlyphInfo& g, float t, core::noise::Mix64Stream&) {
        // Which window `t` falls in, and where inside it.
        const auto windowAt = [&](size_t i) {
          const float begin = i == 0 ? 0.0f : phases[i - 1].endsAt();
          const float end = std::max(phases[i].endsAt(), begin);
          return std::pair<float, float>(begin, end);
        };
        size_t index = phases.size() - 1;
        for (size_t i = 0; i < phases.size(); ++i)
          if (t < phases[i].endsAt()) {
            index = i;
            break;
          }
        const auto [begin, end] = windowAt(index);
        const float width = end - begin;
        const float local =
            width > 0 ? std::clamp((t - begin) / width, 0.0f, 1.0f) : 1.0f;
        core::noise::Mix64Stream own(
            compose::detail::glyphSeed(g, (uint32_t)index));
        GlyphModifier modifier = phases[index].effect()(g, local, own);
        // The crossfade window sits at the END of this phase, so at the
        // joint the blend has already reached the next phase's own start.
        const float overlap = phases[index].overlap();
        if (overlap > 0 && index + 1 < phases.size() && t > end - overlap) {
          const float w =
              std::clamp((t - (end - overlap)) / overlap, 0.0f, 1.0f);
          core::noise::Mix64Stream nextRng(
              compose::detail::glyphSeed(g, (uint32_t)index + 1));
          const GlyphModifier next =
              phases[index + 1].effect()(g, 0.0f, nextRng);
          modifier = compose::detail::lerpModifier(modifier, next, w);
        }
        return modifier;
      },
      reach, displaces);
}

namespace {

/** One entry's numbers, laid end to end. Every field of a GlyphModifier is
 * here, at a fixed stride, so two tables compare exactly when they say the same
 *  thing — structural equality over the whole table rather than a digest of
 *  it. The two substitutions ride along as numbers: a code point IS one, and
 *  an axis is its four tag bytes, its value, and whether it was set at all. */
void appendKeyParameters(std::vector<float>& out, const Key& key) {
  const GlyphModifier& m = key.modifier;
  out.insert(out.end(), {key.at,
                         m.dx,
                         m.dy,
                         m.scale,
                         m.rotateDeg,
                         m.alpha,
                         m.colorMultiplier.fR,
                         m.colorMultiplier.fG,
                         m.colorMultiplier.fB,
                         m.colorMultiplier.fA,
                         m.colorAdd.fR,
                         m.colorAdd.fG,
                         m.colorAdd.fB,
                         m.colorAdd.fA,
                         m.colorScreen.fR,
                         m.colorScreen.fG,
                         m.colorScreen.fB,
                         m.colorScreen.fA,
                         m.scaleX,
                         m.scaleY,
                         m.skewXDeg,
                         m.skewYDeg,
                         (float)m.codepoint,
                         m.axis ? 1.0f : 0.0f});
  const sigil::weave::FontVariation axis =
      m.axis.value_or(sigil::weave::FontVariation());
  for (const char byte : axis.tag) out.push_back((float)(unsigned char)byte);
  out.push_back(axis.value);
}

/** How far past its box a table may throw a glyph. Every published entry is
 *  read: the offsets outright, and a growth or a lean as the fraction of the
 *  glyph it displaces, against the nominal display size no effect knows at
 *  construction. Over-reporting is safe, so the two are added rather than
 *  reasoned about. */
float keysReach(const std::vector<Key>& table) {
  float reach = 0;
  for (const Key& key : table) {
    const GlyphModifier& m = key.modifier;
    const float grown = std::max({std::abs(m.scale * m.scaleX),
                                  std::abs(m.scale * m.scaleY), 1.0f}) -
                        1.0f;
    const bool leans = m.rotateDeg != 0 || m.skewXDeg != 0 || m.skewYDeg != 0;
    reach = std::max(reach,
                     std::abs(m.dx) + std::abs(m.dy) +
                         (grown + (leans ? 0.5f : 0.0f)) * fx::kNominalSizePx);
  }
  return reach;
}

/** Whether a table moves its glyphs off their pen positions — read off the
 *  entries, because the mods ARE the data here and no author needs to say
 *  twice what the table already says. Any offset, any lean, any growth: a
 *  glyph under it lands somewhere other than where the layout put it, and
 *  interpolation between two such entries only ever lands between them, so
 *  a table of entries that all leave the pen alone can never move it. The
 *  colour terms, the fade and the two substitutions are not placement. */
bool keysDisplace(const std::vector<Key>& table) {
  for (const Key& key : table) {
    const GlyphModifier& m = key.modifier;
    if (m.dx != 0 || m.dy != 0 || m.rotateDeg != 0 || m.skewXDeg != 0 ||
        m.skewYDeg != 0 || m.scale != 1 || m.scaleX != 1 || m.scaleY != 1)
      return true;
  }
  return false;
}

}  // namespace

TextEffect keys(std::vector<Key> table, choreograph::EaseFn ease) {
  if (table.empty()) return TextEffect();
  std::vector<float> parameters;
  parameters.reserve(table.size() * 29);
  // The table-wide curve first, then one slot per entry whether or not that
  // entry overrode it: equal tables then always compare curve lists of equal
  // length, and a curve moved from one entry to another is a difference.
  std::vector<choreograph::EaseFn> curves;
  curves.reserve(table.size() + 1);
  curves.push_back(ease);
  for (const Key& key : table) {
    appendKeyParameters(parameters, key);
    curves.push_back(key.ease);
  }
  const float reach = keysReach(table);
  const bool displaces = keysDisplace(table);
  return TextEffect(
      "keys", std::move(parameters),
      [table = std::move(table), ease = std::move(ease)](
          const GlyphInfo&, float t, core::noise::Mix64Stream&) {
        t = std::clamp(t, 0.0f, 1.0f);
        if (t <= table.front().at) return table.front().modifier;
        for (size_t i = 1; i < table.size(); ++i) {
          if (t > table[i].at) continue;
          const Key& from = table[i - 1];
          const Key& to = table[i];
          const float span = to.at - from.at;
          // A zero-width segment is a STEP, and the later entry is what a
          // step lands on.
          const float u = span > 0 ? (t - from.at) / span : 1.0f;
          // The curve is the one named on the segment's OPENING entry, which
          // is where a keyframe list states it.
          const choreograph::EaseFn& curve = from.ease ? from.ease : ease;
          return compose::detail::lerpModifier(from.modifier, to.modifier,
                                               curve ? curve(u) : u);
        }
        return table.back().modifier;
      },
      reach, std::move(curves), displaces);
}

TextEffect hold(TextEffect effect) {
  const float reach = effect.reach();
  // The veto is alpha, which moves nothing: a hold places its glyphs exactly
  // where the effect it wraps places them.
  const bool displaces = effect.displaces();
  std::vector<TextEffect> operands{effect};
  return TextEffect::composite(
      "hold", {}, std::move(operands),
      [effect = std::move(effect)](const GlyphInfo& g, float t,
                                   core::noise::Mix64Stream& rng) {
        // Local time is CLAMPED at both ends, so a unit whose beat has not
        // opened is handed 0 and one whose beat is over is handed 1 — which
        // makes t at its floor the whole signal there is that a beat is
        // still to come. A LOOPING cascade has no such state: its fold
        // keeps every unit somewhere in its cycle and touches 0 only at
        // the instant of re-opening, so there this veto blanks that one
        // instant and nothing else — a looping effect gates its own
        // arrival instead of borrowing this one.
        if (t <= 0.0f) {
          GlyphModifier modifier;
          modifier.alpha = 0.0f;
          return modifier;
        }
        // The glyph's OWN stream, not a lane of its own: a held effect draws
        // exactly the numbers it would have drawn unheld.
        return effect(g, t, rng);
      },
      reach, displaces);
}

TextEffect scramble(std::u32string charset, int steps) {
  // The charset rides the effect's PARAMETERS, one code point per float:
  // every code point is exactly representable, so two scrambles over the
  // same characters compare equal and two over different ones do not —
  // structural equality, not a hash that could collide.
  std::vector<float> parameters;
  parameters.reserve(charset.size() + 1);
  parameters.push_back((float)steps);
  for (char32_t point : charset) parameters.push_back((float)(uint32_t)point);
  const uint32_t ticks = (uint32_t)std::max(steps, 1);
  return TextEffect(
      "scramble", std::move(parameters),
      [charset = std::move(charset), ticks](const GlyphInfo&, float t,
                                            core::noise::Mix64Stream& rng) {
        GlyphModifier modifier;
        if (charset.empty()) return modifier;
        // ONE draw from the glyph's own stream, and everything below is
        // derived from it — so a glyph churns through the same characters
        // at the same moments on every frame and after every relayout,
        // which is what lets a settled scramble cache instead of boiling
        // forever.
        const uint32_t seed = rng.bits();
        // Each glyph resolves at its own moment, and every one of them has
        // resolved by t = 1: the point of the effect is that the true text
        // is what the reader is left with.
        const float settle = 0.35f + (float)(seed >> 24u) * (0.6f / 255.0f);
        if (t >= settle) return modifier;
        const uint32_t tick =
            (uint32_t)(std::clamp(t, 0.0f, 1.0f) * (float)ticks);
        modifier.codepoint =
            charset[compose::detail::mix64Value(seed + tick * 0x9e3779b9u) %
                    charset.size()];
        return modifier;
      },
      // A substitution draws a different outline AT THE ORIGINAL'S PEN
      // POSITION — that is the whole condition the runtime enforces on it —
      // so a churning glyph never moves.
      0.0f, {}, /*displaces=*/false);
}

TextEffect mix(std::vector<TextEffect> effects) {
  if (effects.empty()) return TextEffect();
  float reach = 0;
  for (const TextEffect& e : effects) reach += e.reach();
  const bool displaces = anyDisplaces(effects);
  std::vector<TextEffect> operands = effects;
  return TextEffect::composite(
      "mix", {}, std::move(operands),
      [effects = std::move(effects)](const GlyphInfo& g, float t,
                                     core::noise::Mix64Stream&) {
        GlyphModifier out;
        for (size_t i = 0; i < effects.size(); ++i) {
          core::noise::Mix64Stream own(
              compose::detail::glyphSeed(g, (uint32_t)i));
          compose::detail::compose(out, effects[i](g, t, own));
        }
        return out;
      },
      reach, displaces);
}

TextEffect pass(material::skia::Paint material) {
  return TextEffect::pass(std::move(material));
}

}  // namespace fx

}  // namespace sigil::compose
