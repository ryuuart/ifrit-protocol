#pragma once

/** @file
 * SigilCompose STUDIO tier — the file prelude, extracted.
 *
 * This header exists because of one measurement. Across 64 study and
 * gallery files (60,477 lines) the hex-to-colour conversion is written out
 * **24 times under three names** — `hex` in 12 sketches, `rgb` in 7, and
 * `C` in five gallery headers (`ScenesCosmati.h:48`, `ScenesGerstner.h:51`,
 * `ScenesInventory.h:69`, `ScenesVeloren.h:55`, `ScenesY2k.h:51`) — with
 * byte-identical bodies and *no shared brief between the groups*. The
 * `weave::TextStyle` factory is written 41 times over one six-statement
 * core. That is not convergence caused by a common instruction; that is a
 * missing primitive.
 *
 * ## What this header is, and the line it may not cross
 *
 * The library has already run this experiment twice with opposite results.
 * `<sigilcompose/Util.h>` names value SPELLINGS and is used 166 times for
 * `util::stroke` alone. `<sigilcompose/Layouts.h>` names a design DECISION
 * — where things go — and has **zero** uses across 33 studies.
 *
 * So: everything here converts a value you were going to write anyway into
 * the value the library wanted. Nothing here decides anything.
 *
 *  - `type()` builds a TextStyle out of numbers you supply. It has no
 *    notion of "caption" or "display", and there is no type scale.
 *  - `hex()` converts an integer. There is no palette type, because a
 *    palette is the study's primary research output (`xcom_battlescape`
 *    carries 256 hex literals; every one was sampled off a reference).
 *  - `pickFace()` returns the first family that resolves. It does not know
 *    that Herculanum should fall back to Optima.
 *  - Nothing here places, sizes, or arranges anything. See `Element::rect`
 *    (Compose.h) for placement, and `Layouts.h`'s adoption record for why
 *    a placement *policy* is not on offer.
 *
 * Two things deliberately NOT here, both of which the corpus asked for and
 * neither of which passes the test above:
 *
 *  - `prog(PaintProgram) -> Decoration`. Three sketch sites, two gallery
 *    sites. Extracting it would make the NON-pruning path one word shorter
 *    than the pruning path (`Compose.h`'s value-decoration guidance), which
 *    is an incentive pointed the wrong way.
 *  - A reveal-timeline DSL. 168 `withFrom(0, 1, ramp(...))` sites, and a
 *    `Timeline`/`cue` value saves zero lines — it is a rename with a new
 *    type to learn. `ramp()` ships; the grammar does not.
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
// Colour — every source palette in the corpus is a list of hex integers.

/** `0xRRGGBB` (+ alpha) as an SkColor4f, sRGB byte values divided by 255.
 *
 *  The most-duplicated function in the corpus: 24 definitions, three names,
 *  one body. `hex` wins over `rgb` because `rgb(0xRRGGBB)` reads as "three
 *  channels" when the argument is one integer, and over `C` because `C` is
 *  unsearchable. constexpr, so palette constants stay constexpr. */
constexpr SkColor4f hex(uint32_t rrggbb, float a = 1.0f) {
  return {(float)((rrggbb >> 16) & 0xffu) / 255.0f,
          (float)((rrggbb >> 8) & 0xffu) / 255.0f,
          (float)(rrggbb & 0xffu) / 255.0f, a};
}

/** The same colour at a different alpha — `{c.fR, c.fG, c.fB, a}`, which
 *  the gallery writes out **45 times in 10 files** and never once wrapped
 *  (20 of those sites wrap onto a second physical line). Kept separate from
 *  mul() deliberately: 45 sites overriding alpha and 16 scaling channels
 *  are two different operations and should not share one signature. */
constexpr SkColor4f alpha(SkColor4f c, float a) {
  return {c.fR, c.fG, c.fB, a};
}

/** The brightness ladder: scale RGB by @p k, optionally replacing alpha
 *  (`a < 0` keeps it). 16 gallery sites plus every study that built a tone
 *  ramp off one sampled base colour.
 *
 *  Deliberately does NOT clamp. SkColor4f is float and a channel above 1
 *  is legal (and meaningful under a wide-gamut or OCIO view); Skia clamps
 *  when it lands in an 8-bit surface. One gallery lambda clamps
 *  (`ScenesCosmati.h:102`); the other 15 sites do not. */
constexpr SkColor4f mul(SkColor4f c, float k, float a = -1.0f) {
  return {c.fR * k, c.fG * k, c.fB * k, a < 0 ? c.fA : a};
}

/** Linear interpolation between two colours, alpha included. Component-wise
 *  in whatever space the colours are already in — this is the arithmetic
 *  the studies wrote, not a colour-managed blend. */
constexpr SkColor4f mix(SkColor4f a, SkColor4f b, float t) {
  return {a.fR + (b.fR - a.fR) * t, a.fG + (b.fG - a.fG) * t,
          a.fB + (b.fB - a.fB) * t, a.fA + (b.fA - a.fA) * t};
}

// ---------------------------------------------------------------------------
// Type — a designated-init facade over weave::TextStyle.

/** The parameters of a text style, as a struct **because the positional
 *  version already failed**.
 *
 *  `gallery/GalleryCore.h:35` ships `styleAt(float size, SkColor color)` —
 *  two positional parameters — and *sixteen of the gallery's own scene
 *  headers wrote their own `type()` anyway*, because they needed a face, or
 *  tracking, or condensation, or a `wght` variation
 *  (`ScenesInventory.h:99`), or `slnt` instead (`ScenesSkillTree.h:122`).
 *  A positional helper cannot grow without breaking every call site; a
 *  designated-init aggregate can, and `Transition{.duration = …}` and
 *  `Grid{.columns = …}` already set that precedent here.
 *
 *      text(toU8(s), studio::type({.face = faceMono, .size = 10.5f,
 *                                  .color = kInk, .track = 1.2f}))
 *
 *  Anything not here is added to the RETURNED style, which is already how
 *  the studies do it — `lain_navi.cpp:588` hangs a mask-filter blur and a
 *  kPlus blend off its own; `ScenesVeloren.h:106` adds a mandatory black
 *  underlay. Those are per-artefact decisions and stay at the call site. */
struct Type {
  /** null → the FontContext's default family (plus its fallback chain). */
  sk_sp<SkTypeface> face;
  float size = 16.0f;
  SkColor4f color = {0, 0, 0, 1};
  /** px of tracking added after each cluster (ShapingStyle::letterSpacing).
   *  NOT per-mille — `twoadvanced_v4.cpp:168` reads its reference in
   *  per-mille and converts at the call site, which is where a unit
   *  belonging to a source document belongs. */
  float track = 0.0f;
  /** Horizontal condensation (ShapingStyle::scaleX) — the ATLUS voice for
   *  faces with no `wdth` axis. */
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

/** Type{} → weave::TextStyle. The six-statement core that 41 files wrote. */
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
 *  Fifteen sketches wrote this loop and 49 further lines of
 *  `if (!faceX) faceX = …` after it. Reference reconstruction always names
 *  a face that may not be installed (Herculanum, FOT-Rodin, JetBrains
 *  Mono), so the list is the point.
 *
 *  The last resort is the default family AT THE REQUESTED STYLE, not at
 *  Normal — the hand-rolled versions fell back to `SkFontStyle::Normal()`
 *  and silently lost the weight they had asked for.
 *
 *  `matchFamilyStyle` is not free: hold the result in a `static`, which is
 *  what every study already does. */
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

/** A delayed ramp, in MILLISECONDS as floats — six sketches wrote this
 *  seven-line body byte-identically.
 *
 *  Float ms rather than `std::chrono::milliseconds` on purpose. Every call
 *  site computes its delay (`ramp(tTicks * 1000 + 300 + i * 25, 400)`), and
 *  a chrono parameter would put a cast at all 168 of them. This is the
 *  `styleAt` lesson: the signature the corpus converged on is the
 *  signature, or the helper is ignored. `Transition{.duration = 400ms}`
 *  remains the house spelling wherever the numbers are literals. */
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

/** A wrapping phase in [0, 1): `t` seconds over a `period`-second loop.
 *
 *  Twenty gallery sites spell `std::fmod(t * k, 1.0)` inline — the
 *  marching-ants offset, the orbiting comet, the scrolling marquee, the
 *  scanline creep. One parameter set and one right answer. Non-positive
 *  periods give 0 rather than a NaN.
 *
 *  The other two signals inside those same lambdas — `0.5 + 0.5·sin(t·k)`
 *  (5 sites) and `min(1, t/k)` (2 sites) — are deliberately NOT here. Five
 *  sites and two sites are below the bar every other candidate in this
 *  extraction was held to, and both are already one short expression. */
inline float phase(double t, double period) {
  if (!(period > 0))
    return 0.0f;
  const double p = std::fmod(t / period, 1.0);
  return (float)(p < 0 ? p + 1.0 : p);
}

// NOT here, deliberately: a wrapper for the `ticker.add([t = 0.0](double dt)
// mutable { t += dt; …; return true; })` lambda that opens 36 files (21 of
// 35 sketches, 15 of 21 gallery scenes).
//
// Two reasons, and the second is the real one.
//
// 1. The `return true` is a DECISION, not a spelling — it opts the scene out
//    of the event-driven redraw contract, so the host can never idle. Behind
//    a neutral name that choice becomes invisible and cheap, which is how a
//    helper starts costing more than it saves.
// 2. Those 36 files were never converging on an idiom; they were working
//    around an unreachable accessor. `FrameClock::elapsed()` existed,
//    `PaintContext::elapsedSeconds` existed, and `Sketch::update` was handed
//    elapsed — but `Ticker::add` passed only `dt`, and a steppable is
//    exactly where Outputs get written. A helper over an unreachable clock
//    is how you get a thirty-seventh copy.
//
// **`sigil::motion::Ticker::elapsed()` is the fix and it has shipped.** Read
// the clock instead of accumulating one:
//
//     ticker.add([&](double) { spin = phase(ticker.elapsed(), 6.0); return true; });

// ---------------------------------------------------------------------------
// Strings.

/** printf into a std::string — seven sketches declared this, all with a
 *  fixed stack buffer (160 to 512 bytes) that silently truncated. This one
 *  sizes the result. */
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
