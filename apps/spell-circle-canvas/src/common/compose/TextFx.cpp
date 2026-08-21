/** @file
 * The text-fx seam's VALUES and their resolution against a laid-out
 * paragraph: the comparable `TextEffect`, `Selector` and `Stagger`, the
 * per-walk glyph structure every track shares, and the cascade arithmetic
 * that turns one master progress into a local time per glyph.
 *
 * Nothing here touches an Instance or a canvas. Paint.cpp drives it: build
 * the structure once per frame, resolve each track's selection (cached on
 * the element), then walk the glyphs asking this file for each one's local
 * time and composing the deviations.
 */

#include <sigilweave/Choreograph.h>
#include <sigilweave/Query.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <unordered_set>

#include "ComposeRuntime.h"

namespace sigil::compose {

// ---------------------------------------------------------------------------
// TextEffect

TextEffect::TextEffect(std::string name, std::vector<float> params,
                       GlyphModFn fn, float reach) {
  auto state = std::make_shared<State>();
  state->name = std::move(name);
  state->params = std::move(params);
  state->fn = std::move(fn);
  state->reach = reach;
  m_state = std::move(state);
}

TextEffect TextEffect::composite(std::string name, std::vector<float> params,
                                 std::vector<TextEffect> operands,
                                 GlyphModFn fn, float reach) {
  auto state = std::make_shared<State>();
  state->name = std::move(name);
  state->params = std::move(params);
  state->operands = std::move(operands);
  state->fn = std::move(fn);
  state->reach = reach;
  TextEffect out;
  out.m_state = std::move(state);
  return out;
}

const std::string& TextEffect::name() const {
  static const std::string kEmpty;
  return m_state ? m_state->name : kEmpty;
}

std::span<const float> TextEffect::params() const {
  return m_state ? std::span<const float>(m_state->params)
                 : std::span<const float>();
}

std::span<const TextEffect> TextEffect::operands() const {
  return m_state ? std::span<const TextEffect>(m_state->operands)
                 : std::span<const TextEffect>();
}

bool TextEffect::operator==(const TextEffect& other) const {
  if (m_state == other.m_state) return true;  // copies of one value
  if (!m_state || !other.m_state) return false;
  return m_state->name == other.m_state->name &&
         m_state->params == other.m_state->params &&
         m_state->operands == other.m_state->operands;
}

Phase TextEffect::until(float t) const { return Phase(*this, t); }

// ---------------------------------------------------------------------------
// Selector

Selector Selector::of(State s) {
  Selector out;
  out.m_state = std::make_shared<const State>(std::move(s));
  return out;
}

bool Selector::operator==(const Selector& other) const {
  if (m_state == other.m_state) return true;
  if (!m_state || !other.m_state) return false;  // one is "everything"
  return *m_state == *other.m_state;
}

Selector Selector::take(int n) const {
  State s = m_state ? *m_state : State{};
  s.take = n;
  return of(std::move(s));
}

Selector Selector::drop(int n) const {
  State s = m_state ? *m_state : State{};
  s.drop = n;
  return of(std::move(s));
}

namespace {
Selector combine(Selector::Kind kind, const Selector& a, const Selector& b) {
  Selector::State s;
  s.kind = kind;
  s.operands = {a, b};
  return Selector::of(std::move(s));
}
}  // namespace

Selector Selector::operator|(const Selector& other) const {
  return combine(Kind::Union, *this, other);
}
Selector Selector::operator&(const Selector& other) const {
  return combine(Kind::Intersect, *this, other);
}
Selector Selector::operator!() const {
  State s;
  s.kind = Kind::Complement;
  s.operands = {*this};
  return of(std::move(s));
}

namespace sel {
namespace {
Selector indexed(Selector::Kind kind, uint32_t lo, uint32_t hi) {
  Selector::State s;
  s.kind = kind;
  s.lo = lo;
  s.hi = hi;
  return Selector::of(std::move(s));
}
Selector needle(Selector::Kind kind, std::u8string_view pattern) {
  Selector::State s;
  s.kind = kind;
  s.pattern = std::u8string(pattern);
  return Selector::of(std::move(s));
}
}  // namespace

Selector word(uint32_t index) {
  return indexed(Selector::Kind::Word, index, index + 1);
}
Selector words(uint32_t lo, uint32_t hi) {
  return indexed(Selector::Kind::Words, lo, hi);
}
Selector line(uint32_t index) {
  return indexed(Selector::Kind::Line, index, index + 1);
}
Selector sentence(uint32_t index) {
  return indexed(Selector::Kind::Sentence, index, index + 1);
}
Selector range(sigil::weave::CharRange chars) {
  return indexed(Selector::Kind::Range, chars.start, chars.end);
}
Selector regex(std::u8string_view utf8Pattern) {
  return needle(Selector::Kind::Regex, utf8Pattern);
}
Selector text(std::u8string_view utf8Substring) {
  return needle(Selector::Kind::Text, utf8Substring);
}
Selector each(Unit granularity) {
  Selector::State s;
  s.kind = Selector::Kind::Each;
  s.each = granularity;
  return Selector::of(std::move(s));
}
}  // namespace sel

// ---------------------------------------------------------------------------
// Stagger

Stagger& Stagger::then(Unit granularity, Stagger nested) {
  nested.over = granularity;
  inner = std::make_shared<const Stagger>(std::move(nested));
  return *this;
}

Stagger stagger(Unit granularity, Stagger spec) {
  spec.over = granularity;
  return spec;
}

// ---------------------------------------------------------------------------
// The per-walk structure every track shares

namespace detail {

namespace {
/** A new unit of `granularity` begins here. Draw order is the enumeration
 *  order forEachPlacedGlyph guarantees, so "changed since the previous
 *  glyph" is the whole test — no map, no sort, and a cluster's glyphs stay
 *  together because a cluster's glyphs are adjacent by construction. */
bool startsUnit(Unit granularity, const sigil::weave::PlacedGlyph& glyph,
                const sigil::weave::PlacedGlyph& previous, bool first) {
  if (first) return true;
  switch (granularity) {
    case Unit::Glyph:
      return true;
    case Unit::Cluster:
      // The text offset, not the shaped-run-local cluster: a base and a
      // combining mark that fell back to a second font are two runs and one
      // cluster, and a stagger that separated them would leave the accent
      // behind in mid-air.
      return glyph.wordIndex != previous.wordIndex ||
             glyph.textIndex != previous.textIndex;
    case Unit::Word:
      return glyph.wordIndex != previous.wordIndex;
    case Unit::Line:
      return glyph.lineIndex != previous.lineIndex;
    case Unit::Sentence:
      return glyph.sentenceIndex != previous.sentenceIndex;
  }
  return true;
}
}  // namespace

void GlyphStructure::build(const sigil::weave::ParagraphLayout& layout,
                           const sigil::weave::Paragraph& paragraph) {
  glyphs.clear();
  for (auto& lane : unitOf) lane.clear();
  unitCounts = {};

  sigil::weave::PlacedGlyph previous;
  bool first = true;
  sigil::weave::forEachPlacedGlyph(
      layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        GlyphInfo info;
        info.index = glyphs.size();
        info.rest = placed.rest;
        info.advance = placed.advance;
        info.fontSize = placed.shaped ? placed.shaped->fontSize : 0.0f;
        info.cluster = placed.cluster;
        info.textIndex = placed.textIndex;
        info.wordIndex = placed.wordIndex;
        info.lineIndex = (uint32_t)std::max(placed.lineIndex, 0);
        info.styleIndex = placed.styleIndex;
        info.sentenceIndex = placed.sentenceIndex;
        glyphs.push_back(info);

        for (size_t lane = 0; lane < kUnits; ++lane) {
          if (startsUnit((Unit)lane, placed, previous, first))
            ++unitCounts[lane];
          unitOf[lane].push_back(unitCounts[lane] - 1);
        }
        previous = placed;
        first = false;
      });

  // The two per-word facts an effect reads (which letter of its word, of
  // how many) need the word's size, which is only known once the word has
  // been walked — so they are a second pass over the finished runs.
  const uint32_t total = (uint32_t)glyphs.size();
  const std::vector<uint32_t>& wordUnits = unitOf[(size_t)Unit::Word];
  for (uint32_t begin = 0; begin < total;) {
    uint32_t end = begin + 1;
    while (end < total && wordUnits[end] == wordUnits[begin]) ++end;
    for (uint32_t i = begin; i < end; ++i) {
      glyphs[i].glyphInWord = i - begin;
      glyphs[i].wordGlyphCount = end - begin;
      glyphs[i].count = total;
    }
    begin = end;
  }
}

// ---------------------------------------------------------------------------
// Selection

namespace {

/** Marks every glyph whose cluster falls inside one of `ranges`. */
void markRanges(const std::vector<sigil::weave::CharRange>& ranges,
                const GlyphStructure& structure, std::vector<uint8_t>& out) {
  for (const sigil::weave::CharRange& r : ranges)
    for (size_t i = 0; i < structure.glyphs.size(); ++i)
      if (structure.glyphs[i].textIndex >= r.start &&
          structure.glyphs[i].textIndex < r.end)
        out[i] = 1;
}

void resolveInto(const Selector& selector, const GlyphStructure& structure,
                 const sigil::weave::Paragraph& paragraph,
                 std::vector<uint8_t>& out) {
  const size_t count = structure.glyphs.size();
  out.assign(count, 0);
  const Selector::State* s = selector.state();
  if (!s) {  // default-constructed: everything
    std::fill(out.begin(), out.end(), (uint8_t)1);
    return;
  }
  const auto byIndex = [&](auto&& field, uint32_t lo, uint32_t hi) {
    for (size_t i = 0; i < count; ++i) {
      const uint32_t v = field(structure.glyphs[i]);
      if (v >= lo && v < hi) out[i] = 1;
    }
  };
  switch (s->kind) {
    case Selector::Kind::All:
      std::fill(out.begin(), out.end(), (uint8_t)1);
      break;
    case Selector::Kind::Word:
    case Selector::Kind::Words:
      byIndex([](const GlyphInfo& g) { return g.wordIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Line:
      byIndex([](const GlyphInfo& g) { return g.lineIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Sentence:
      byIndex([](const GlyphInfo& g) { return g.sentenceIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Range:
      byIndex([](const GlyphInfo& g) { return g.textIndex; }, s->lo, s->hi);
      break;
    case Selector::Kind::Text:
      markRanges(sigil::weave::findAllOccurrences(paragraph, s->pattern),
                 structure, out);
      break;
    case Selector::Kind::Regex: {
      std::optional<std::vector<sigil::weave::CharRange>> matches =
          sigil::weave::findRegexMatches(paragraph, s->pattern);
      if (!matches) {
        warnBadSelectorPattern(s->pattern);
        break;  // an unresolvable pattern selects nothing
      }
      markRanges(*matches, structure, out);
      break;
    }
    case Selector::Kind::Each: {
      // Every unit sliced the same way, at GLYPH granularity inside it.
      // `drop(n)` and `take(n)` partition a unit exactly: the two answer
      // opposite sides of the same cut, so no glyph is in both and none is
      // in neither.
      const std::vector<uint32_t>& units = structure.unitOf[(size_t)s->each];
      const int drop = std::max(s->drop, 0);
      int within = 0;
      for (size_t i = 0; i < count; ++i) {
        if (i > 0 && units[i] != units[i - 1]) within = 0;
        const bool afterDrop = within >= drop;
        const bool beforeTake =
            s->take < 0 || within < drop + std::max(s->take, 0);
        if (afterDrop && beforeTake) out[i] = 1;
        ++within;
      }
      break;
    }
    case Selector::Kind::Union:
    case Selector::Kind::Intersect: {
      std::vector<uint8_t> lhs, rhs;
      resolveInto(s->operands[0], structure, paragraph, lhs);
      resolveInto(s->operands[1], structure, paragraph, rhs);
      for (size_t i = 0; i < count; ++i)
        out[i] = s->kind == Selector::Kind::Union ? (lhs[i] | rhs[i])
                                                  : (lhs[i] & rhs[i]);
      break;
    }
    case Selector::Kind::Complement: {
      std::vector<uint8_t> inner;
      resolveInto(s->operands[0], structure, paragraph, inner);
      for (size_t i = 0; i < count; ++i) out[i] = inner[i] ? 0 : 1;
      break;
    }
  }
}

}  // namespace

void warnBadSelectorPattern(const std::u8string& pattern) {
  // Once per distinct pattern: a selector resolved every reflow would
  // otherwise scroll the same line past the author forever.
  static thread_local std::unordered_set<std::string> seen;
  std::string key((const char*)pattern.data(), pattern.size());
  if (!seen.insert(key).second) return;
  std::fprintf(stderr,
               "SigilCompose: sel::regex(\"%s\") does not compile — this "
               "track selects no glyphs\n",
               key.c_str());
}

std::vector<uint8_t> resolveSelection(
    const Selector& selector, const GlyphStructure& structure,
    const sigil::weave::Paragraph& paragraph) {
  std::vector<uint8_t> out;
  resolveInto(selector, structure, paragraph, out);
  return out;
}

// ---------------------------------------------------------------------------
// The cascade

namespace {
/** SplitMix64's finalizer over one key — the same mixer Rng steps, used
 *  here to order units rather than to shape a glyph. */
uint64_t mix64Value(uint64_t z) {
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}
}  // namespace

void cascadeOrder(Stagger::From from, uint32_t count,
                  std::vector<float>& order) {
  order.assign(count, 0.0f);
  // A cascade of ONE is a cascade with no spread, whichever end it claims
  // to start from: every shape below must put that single member at 0.
  const float last = count > 1 ? (float)(count - 1) : 0.0f;
  switch (from) {
    case Stagger::From::Start:
      for (uint32_t i = 0; i < count; ++i) order[i] = (float)i;
      break;
    case Stagger::From::End:
      for (uint32_t i = 0; i < count; ++i) order[i] = (float)(count - 1 - i);
      break;
    case Stagger::From::Center:
      for (uint32_t i = 0; i < count; ++i)
        order[i] = std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case Stagger::From::Edges:
      for (uint32_t i = 0; i < count; ++i)
        order[i] = last - std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case Stagger::From::Random: {
      // Rank each unit by a hash of its index: deterministic, so the same
      // text scatters the same way on every frame and after a relayout.
      std::vector<uint32_t> indices(count);
      std::iota(indices.begin(), indices.end(), 0u);
      std::stable_sort(indices.begin(), indices.end(),
                       [count](uint32_t a, uint32_t b) {
                         return mix64Value(a * 2654435761ull + count) <
                                mix64Value(b * 2654435761ull + count);
                       });
      for (uint32_t rank = 0; rank < count; ++rank)
        order[indices[rank]] = (float)rank;
      break;
    }
  }
}

namespace {
/** The per-unit spacing this cascade asks for, in ms. Amount-mode divides
 *  a fixed total across however many units there are; otherwise the
 *  spacing is fixed and the total grows. */
float spacingMs(const Stagger& spec, uint32_t count) {
  if (spec.amountMs > 0 && count > 1) return spec.amountMs / (float)(count - 1);
  return std::max(spec.eachMs, 0.0f);
}
}  // namespace

void Cascade::build(const Stagger& spec, uint32_t outerCount,
                    uint32_t innerCount) {
  duration = std::max(spec.durationMs, 1.0f);
  const uint32_t outer = std::max(outerCount, 1u);
  cascadeOrder(spec.from, outer, outerOrder);
  outerEach = spacingMs(spec, outer);

  if (spec.inner) {
    const uint32_t inner = std::max(innerCount, 1u);
    cascadeOrder(spec.inner->from, inner, innerOrder);
    innerEach = spacingMs(*spec.inner, inner);
    // A NESTED cascade owns the beat: its own duration is what one unit's
    // motion lasts, and a beat is exactly as long as the inner ladder
    // needs. The outer durationMs would otherwise be a second, conflicting
    // statement about the same span.
    duration = std::max(spec.inner->durationMs, 1.0f);
    beatMs = duration + innerEach * (float)(inner - 1);
    innerDistribution = spec.inner->distribution;
  } else {
    innerOrder.clear();
    innerEach = 0.0f;
    beatMs = duration;
    innerDistribution = nullptr;
  }
  outerDistribution = spec.distribution;
  totalMs = beatMs + outerEach * (float)(outer - 1);
}

float Cascade::localTime(float master, uint32_t outerUnit,
                         uint32_t innerUnit) const {
  // Without a distribution curve the delay is the plain product the flat
  // cascade has always been — NOT the same product routed through a
  // normalise-and-rescale, which would differ in the last bit and move
  // every pixel of a settled reveal.
  const auto delayOf = [](const std::vector<float>& order, uint32_t index,
                          float each, const choreograph::EaseFn& shape) {
    if (order.empty()) return 0.0f;
    const uint32_t clamped = std::min<uint32_t>(index, order.size() - 1);
    if (!shape) return order[clamped] * each;
    const float last = order.size() > 1 ? (float)(order.size() - 1) : 1.0f;
    return shape(order[clamped] / last) * (each * last);
  };
  const float delay =
      delayOf(outerOrder, outerUnit, outerEach, outerDistribution) +
      delayOf(innerOrder, innerUnit, innerEach, innerDistribution);
  return std::clamp((master * totalMs - delay) / duration, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Composition

void compose(GlyphMod& into, const GlyphMod& next) {
  into.dx += next.dx;
  into.dy += next.dy;
  into.rotateDeg += next.rotateDeg;
  into.scale *= next.scale;
  into.alpha *= next.alpha;
}

GlyphMod lerpMod(const GlyphMod& a, const GlyphMod& b, float w) {
  GlyphMod out;
  out.dx = a.dx + (b.dx - a.dx) * w;
  out.dy = a.dy + (b.dy - a.dy) * w;
  out.rotateDeg = a.rotateDeg + (b.rotateDeg - a.rotateDeg) * w;
  out.scale = a.scale + (b.scale - a.scale) * w;
  out.alpha = a.alpha + (b.alpha - a.alpha) * w;
  return out;
}

uint64_t glyphSeed(const GlyphInfo& g, uint32_t lane) {
  // The glyph's identity, and only that: its position in the walk and the
  // text offset it came from. Both hold across relayouts while the text is
  // unchanged, which is what makes a seeded scatter cacheable. `lane`
  // separates the operands of a composite, so mixing two scatters gives two
  // scatters rather than one drawn twice — and it is the OPERAND's index,
  // never a counter, so a phase draws the same numbers whether it is being
  // crossfaded into or playing alone.
  return mix64Value(((uint64_t)g.textIndex << 32) ^ (uint64_t)g.index) +
         mix64Value(lane);
}

}  // namespace detail

// ---------------------------------------------------------------------------
// The combinators

namespace fx {

TextEffect seq(std::vector<Phase> phases) {
  if (phases.empty()) return TextEffect();
  // The last phase always runs to the end of local time, whatever it was
  // declared with — otherwise the tail of every sequence is undefined.
  phases.back() = Phase(phases.back().effect(), 1.0f).xfade(0.0f);

  std::vector<TextEffect> operands;
  std::vector<float> params;
  operands.reserve(phases.size());
  params.reserve(phases.size() * 2);
  float reach = 0;
  for (const Phase& p : phases) {
    operands.push_back(p.effect());
    params.push_back(p.endsAt());
    params.push_back(p.overlap());
    reach = std::max(reach, p.effect().reach());
  }
  return TextEffect::composite(
      "seq", params, operands,
      [phases](const GlyphInfo& g, float t, Rng&) {
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
        Rng own(detail::glyphSeed(g, (uint32_t)index));
        GlyphMod mod = phases[index].effect()(g, local, own);
        // The crossfade window sits at the END of this phase, so at the
        // joint the blend has already reached the next phase's own start.
        const float overlap = phases[index].overlap();
        if (overlap > 0 && index + 1 < phases.size() && t > end - overlap) {
          const float w =
              std::clamp((t - (end - overlap)) / overlap, 0.0f, 1.0f);
          Rng nextRng(detail::glyphSeed(g, (uint32_t)index + 1));
          const GlyphMod next = phases[index + 1].effect()(g, 0.0f, nextRng);
          mod = detail::lerpMod(mod, next, w);
        }
        return mod;
      },
      reach);
}

TextEffect mix(std::vector<TextEffect> effects) {
  if (effects.empty()) return TextEffect();
  float reach = 0;
  for (const TextEffect& e : effects) reach += e.reach();
  std::vector<TextEffect> operands = effects;
  return TextEffect::composite(
      "mix", {}, std::move(operands),
      [effects = std::move(effects)](const GlyphInfo& g, float t, Rng&) {
        GlyphMod out;
        for (size_t i = 0; i < effects.size(); ++i) {
          Rng own(detail::glyphSeed(g, (uint32_t)i));
          detail::compose(out, effects[i](g, t, own));
        }
        return out;
      },
      reach);
}

}  // namespace fx

}  // namespace sigil::compose
