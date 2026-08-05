#pragma once

/** @file
 * SigilCompose STUDIO tier — the file prelude a sketch or gallery scene
 * would otherwise write for itself: the handful of small conversions that
 * every study needs before it can say anything, gathered under one name so
 * they are not re-declared, differently spelled, in each file.
 *
 * ## What this header is, and the line it may not cross
 *
 * Everything here converts a value you were going to write anyway into the
 * value the library wanted. Nothing here decides anything — a helper that
 * makes a design decision is one the next study has to argue with, and a
 * study's decisions are its whole point.
 *
 *  - `type()` builds a TextStyle out of numbers you supply. It has no
 *    notion of "caption" or "display", and there is no type scale.
 *  - `hex()` converts an integer. There is no palette type, because a
 *    palette is a study's own output — sampled off whatever it is
 *    reconstructing — and not something a library can offer.
 *  - `pickFace()` returns the first family that resolves. It has no
 *    opinion about which face should stand in for which.
 *  - Nothing here places, sizes, or arranges anything. See `Element::rect`
 *    (Compose.h) for placement; a placement *policy* is deliberately not
 *    on offer.
 *
 * Two things deliberately NOT here:
 *
 *  - `prog(PaintProgram) -> Decoration`. A named shorthand for it would
 *    make the raw-callable spelling — the one the reconciler cannot prune
 *    — shorter to write than the comparable-value spelling that prunes,
 *    which points the incentive the wrong way.
 *  - A reveal-timeline DSL. `animate(from(0.f).to(1.f), ramp(...))` is
 *    already the whole grammar; a `Timeline`/`cue` value would save no
 *    lines and add a type to learn. `ramp()` ships; the grammar does not.
 */

#include "sigilcompose/Compose.h"

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkColor.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkTypeface.h>

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace sigil::compose::studio {

// ---------------------------------------------------------------------------
// Colour — source palettes arrive as lists of hex integers.

/** `0xRRGGBB` (+ alpha) as an SkColor4f, sRGB byte values divided by 255.
 *
 *  Named `hex` rather than `rgb`, because `rgb(0xRRGGBB)` reads as "three
 *  arguments" when there is only one, and rather than a single letter,
 *  which is unsearchable. constexpr, so palette constants stay constexpr. */
constexpr SkColor4f hex(uint32_t rrggbb, float a = 1.0f) {
  return {(float)((rrggbb >> 16) & 0xffu) / 255.0f,
          (float)((rrggbb >> 8) & 0xffu) / 255.0f,
          (float)(rrggbb & 0xffu) / 255.0f, a};
}

/** The same colour at a different alpha — `{c.fR, c.fG, c.fB, a}`.
 *
 *  Kept separate from mul() deliberately: replacing alpha and scaling the
 *  colour channels are different operations, and folding both into one
 *  signature would leave a defaulted argument deciding which the caller
 *  meant. */
constexpr SkColor4f alpha(SkColor4f c, float a) {
  return {c.fR, c.fG, c.fB, a};
}

/** The brightness ladder: scale RGB by @p k, optionally replacing alpha
 *  (`a < 0` keeps it) — a tone ramp off one sampled base colour.
 *
 *  Deliberately does NOT clamp. SkColor4f is float and a channel above 1
 *  is legal (and meaningful under a wide-gamut or OCIO view); Skia clamps
 *  when it lands in an 8-bit surface. A caller who needs the clamped value
 *  is asking for a different operation and writes it at the call site. */
constexpr SkColor4f mul(SkColor4f c, float k, float a = -1.0f) {
  return {c.fR * k, c.fG * k, c.fB * k, a < 0 ? c.fA : a};
}

/** Linear interpolation between two colours, alpha included. Component-wise
 *  in whatever space the colours are already in — plain arithmetic, not a
 *  colour-managed blend. */
constexpr SkColor4f mix(SkColor4f a, SkColor4f b, float t) {
  return {a.fR + (b.fR - a.fR) * t, a.fG + (b.fG - a.fG) * t,
          a.fB + (b.fB - a.fB) * t, a.fA + (b.fA - a.fA) * t};
}

// ---------------------------------------------------------------------------
// Type — a designated-init facade over weave::TextStyle.

/** The parameters of a text style, as a designated-init aggregate rather
 *  than a positional signature.
 *
 *  Every caller needs a face, a size and a colour, and then some subset of
 *  tracking, condensation and variable-font axes — and which subset differs
 *  per call site. A positional helper cannot grow another parameter without
 *  breaking every existing call; an aggregate can, and
 *  `Transition{.duration = …}` and `Grid{.columns = …}` are already spelled
 *  that way here.
 *
 *      text(toU8(s), studio::type({.face = faceMono, .size = 10.5f,
 *                                  .color = kInk, .track = 1.2f}))
 *
 *  Anything not here is added to the RETURNED style — a mask-filter blur, a
 *  kPlus blend, a mandatory underlay. Those are per-artefact decisions and
 *  stay at the call site. */
struct Type {
  /** null → the FontContext's default family (plus its fallback chain). */
  sk_sp<SkTypeface> face;
  float size = 16.0f;
  SkColor4f color = {0, 0, 0, 1};
  /** px of tracking added after each cluster (ShapingStyle::letterSpacing).
   *  NOT per-mille: a reference that quotes tracking in per-mille is
   *  converted at the call site, where the unit's own em size is known. */
  float track = 0.0f;
  /** Horizontal condensation (ShapingStyle::scaleX) — how to condense a
   *  face that has no `wdth` axis to ask instead. */
  float condense = 1.0f;
  /** > 0 → a `wght` variable-font axis. Weight changes advances, so this
   *  participates in shaping identity. */
  float weight = 0.0f;
  /** != 0 → a `slnt` axis (negative leans right, per the OpenType sign). */
  float slant = 0.0f;
  /** Hard-edged glyph rasterisation (ShapingStyle::aliased). Skia takes
   *  edging from the SkFont, never from the paint, so this is the only way
   *  to ask — `paint.setAntiAlias(false)` is silently ignored on text. */
  bool aliased = false;
  /** The glyph paint's own antialias flag (edges of strokes/decorations on
   *  the paint, not the glyph edging above). */
  bool antiAlias = true;
  /** Anything else in design space — appended after weight/slant, so the
   *  order is stable and two styles built the same way share one
   *  varied-face memo entry. */
  std::vector<sigil::weave::FontVariation> variations;
};

/** Type{} → weave::TextStyle. */
inline sigil::weave::TextStyle type(const Type &t) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = t.face;
  s.shaping.fontSize = t.size;
  s.shaping.letterSpacing = t.track;
  s.shaping.scaleX = t.condense;
  s.shaping.aliased = t.aliased;
  s.paint.foreground.setColor4f(t.color, nullptr);
  s.paint.foreground.setAntiAlias(t.antiAlias);
  if (t.weight > 0)
    s.variation("wght", t.weight);
  if (t.slant != 0)
    s.variation("slnt", t.slant);
  for (const sigil::weave::FontVariation &v : t.variations)
    s.shaping.variations.push_back(v);
  return s;
}

// ---------------------------------------------------------------------------
// Faces — the fallback chain, once.

/** The first of @p families the system font manager resolves, at @p style.
 *
 *  The list is the point: reconstructing a reference names a face that may
 *  not be installed on the machine running the code, so callers pass the
 *  face they want followed by the stand-ins they will accept.
 *
 *  The last resort is the default family AT THE REQUESTED STYLE, not at
 *  `SkFontStyle::Normal()` — falling back to Normal would silently drop the
 *  weight the caller asked for.
 *
 *  `matchFamilyStyle` walks the system font list; hold the result in a
 *  `static` rather than calling this per frame. */
inline sk_sp<SkTypeface> pickFace(std::initializer_list<const char *> families,
                                  SkFontStyle style = SkFontStyle::Normal()) {
  sk_sp<SkFontMgr> mgr = sigil::weave::ports::systemFontManager();
  if (!mgr)
    return nullptr;
  for (const char *family : families)
    if (sk_sp<SkTypeface> face = mgr->matchFamilyStyle(family, style))
      return face;
  return mgr->matchFamilyStyle(nullptr, style);
}

/** `pickFace` spelled with a weight and a slant, for the (common) case
 *  where the caller has those two numbers and not an SkFontStyle. */
inline sk_sp<SkTypeface> pickFace(std::initializer_list<const char *> families,
                                  int weight,
                                  SkFontStyle::Slant slant =
                                      SkFontStyle::kUpright_Slant) {
  return pickFace(families,
                  SkFontStyle(weight, SkFontStyle::kNormal_Width, slant));
}

// ---------------------------------------------------------------------------
// Transitions.

/** A delayed ramp, in MILLISECONDS as floats.
 *
 *  Float ms rather than `std::chrono::milliseconds` on purpose: a staggered
 *  reveal computes its delay arithmetically
 *  (`ramp(tTicks * 1000 + 300 + i * 25, 400)`), and a chrono parameter
 *  would put a cast at every such site. `Transition{.duration = 400ms}`
 *  remains the spelling wherever the numbers are literals. */
inline Transition ramp(float delayMs, float durationMs,
                       choreograph::EaseFn ease = &choreograph::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durationMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(ease);
  return t;
}

// ---------------------------------------------------------------------------
// Time.

/** A wrapping phase in [0, 1): `t` seconds over a `period`-second loop —
 *  the marching-ants offset, the orbiting comet, the scrolling marquee,
 *  the scanline creep.
 *
 *  A non-positive period gives 0 rather than the NaN the bare `fmod` would
 *  produce, and a negative `t` wraps forward instead of returning a
 *  negative phase, so the result is always in range whatever the caller
 *  hands in.
 *
 *  Deliberately narrow. The two neighbouring signals, `0.5 + 0.5·sin(t·k)`
 *  and `min(1, t/k)`, are one short expression each and are not here. */
inline float phase(double t, double period) {
  if (!(period > 0))
    return 0.0f;
  const double p = std::fmod(t / period, 1.0);
  return (float)(p < 0 ? p + 1.0 : p);
}

// NOT here, deliberately: a wrapper for the `ticker.add([t = 0.0](double dt)
// mutable { t += dt; …; return true; })` lambda that opens most scenes.
//
// The `return true` is a DECISION, not a spelling — it opts the scene out of
// the event-driven redraw contract, so the host can never idle. Behind a
// neutral name that choice becomes invisible and cheap, which is how a helper
// starts costing more than it saves.
//
// There is also nothing left to wrap: `sigil::motion::Ticker::elapsed()` is
// the same clock the accumulator was reconstructing, so read it instead of
// carrying a mutable total in the capture.
//
//     ticker.add([&](double) { spin = phase(ticker.elapsed(), 6.0); return true; });

// ---------------------------------------------------------------------------
// Strings.

/** printf into a std::string. The result is sized by a measuring
 *  `vsnprintf` pass rather than written into a fixed buffer, so long output
 *  is never silently truncated. Returns an empty string when the format
 *  itself fails. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 1, 2)))
#endif
inline std::string
fmt(const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list probe;
  va_copy(probe, args);
  const int needed = std::vsnprintf(nullptr, 0, format, probe);
  va_end(probe);
  if (needed < 0) {
    va_end(args);
    return {};
  }
  std::string out((size_t)needed, '\0');
  std::vsnprintf(out.data(), (size_t)needed + 1, format, args);
  va_end(args);
  return out;
}

} // namespace sigil::compose::studio
