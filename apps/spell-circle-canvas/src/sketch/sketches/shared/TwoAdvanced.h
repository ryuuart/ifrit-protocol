#pragma once
// TwoAdvanced.h — one studio's house vocabulary, across three eras.
//
// twoadvanced_v3, twoadvanced_v4 and twoadvanced_equipment reconstruct
// three different artefacts — a 2024 Rive rebuild, a 2003 Flash stage and
// a 2003 HTML frameset store — so they are three sketches with three
// subjects, three canvases and three declared moments. What is one thing
// across all three is the studio's own chassis, and it has no business
// being stated three times.
//
//   THE FACES the sites embedded, substituted with the nearest the
//   platform ships. Each is resolved once behind a function, because the
//   fallback chain walks the system font list and that is not a per-frame
//   cost.
//
//   THE TRACKING UNIT. Every one of these interfaces was set in
//   Illustrator or Flash, where tracking is 1/1000 em — so a tracking
//   value is a property of the VOICE and stays put when the size moves.
//   `sigil::weave::kit::tracked()` is that conversion; each sketch builds
//   its own register on top, because the register is what its artefact
//   sounded like.
//
//   THE SECTION CYCLE. Two of the three are simulated VISITORS rather
//   than stills: the home view holds, then every tab is walked in order
//   and the view returns home. The schedule is a pure function of the
//   clock, which is what makes a capture land on the same frame every
//   run, and it is the same schedule in both — only the stop count and
//   the durations differ.
//
// What is deliberately NOT here: each sketch's palette, its type
// register, its chrome and its boot overlay. Those are what the three
// artefacts DIFFER by, and a shared header that held them would be
// asserting a house style the sites do not actually share.

#include <include/core/SkColor.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/core/Factories.h>
#include <sigilweave/kit/Labels.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Style.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace twoadvanced {

// ---------------------------------------------------------------------------
// The faces. Resolved once; the list is the substitution, in order.

/** v3's workhorse: Akzidenz-Grotesk medium, substituted. */
inline const sk_sp<SkTypeface>& grot() {
  static const sk_sp<SkTypeface> f = sigil::weave::ports::pickTypeface(
      {"Helvetica Neue", "Arial"}, SkFontStyle::kMedium_Weight);
  return f;
}
/** v3's headline weight. */
inline const sk_sp<SkTypeface>& grotBold() {
  static const sk_sp<SkTypeface> f = sigil::weave::ports::pickTypeface(
      {"Helvetica Neue", "Arial"}, SkFontStyle::kBold_Weight);
  return f;
}
/** v4's chrome voice: Helvetica CondensedBlack, the face the SWF
 *  embedded and the one thing the whole interface is lettered in. */
inline const sk_sp<SkTypeface>& condBlack() {
  static const sk_sp<SkTypeface> f = sigil::weave::ports::pickTypeface(
      {"Helvetica Neue", "Avenir Next Condensed"},
      SkFontStyle(SkFontStyle::kBlack_Weight, SkFontStyle::kCondensed_Width,
                  SkFontStyle::kUpright_Slant));
  return f;
}
/** v4's heavier, wider register: Arial Black. */
inline const sk_sp<SkTypeface>& blackFace() {
  static const sk_sp<SkTypeface> f = sigil::weave::ports::pickTypeface(
      {"Arial Black", "Helvetica Neue"}, SkFontStyle::kBlack_Weight);
  return f;
}
/** The body face both Flash-era pages set their prose in. */
inline const sk_sp<SkTypeface>& arial() {
  static const sk_sp<SkTypeface> f =
      sigil::weave::ports::pickTypeface({"Arial", "Helvetica"});
  return f;
}
/** The store's face: Verdana, which is what an HTML `size=1` cell was
 *  set in and what macOS still ships. */
inline const sk_sp<SkTypeface>& verdanaFace(bool bold) {
  static const sk_sp<SkTypeface> regular =
      sigil::weave::ports::pickTypeface({"Verdana", "Arial"});
  static const sk_sp<SkTypeface> heavy =
      sigil::weave::ports::pickTypeface({"Verdana", "Arial"}, SkFontStyle::kBold_Weight);
  return bold ? heavy : regular;
}

// ---------------------------------------------------------------------------
// The one text alias.

/** A narrow-string label, which is what every caption on these pages is. */
inline sigil::compose::Element t(const char* s, sigil::weave::TextStyle style) {
  return sigil::compose::text(sigil::compose::toU8(s), std::move(style));
}

// ---------------------------------------------------------------------------
// The simulated visitor's clock.

/** THE SECTION CYCLE, as a pure function of the scene clock.
 *
 *  The home view holds until `start`, and from then on the visitor walks
 *  `stops` stops in order, forever: each stop is shown for `hold`
 *  seconds, of which the first `transition` seconds are the change into
 *  it. Which SECTION a stop selects is the caller's own mapping, because
 *  the two sites number their tabs differently and both reserve one stop
 *  for going home.
 *
 *  Nothing here reads a wall clock, which is what makes a capture land on
 *  the same frame every run. */
struct SectionCycle {
  double start = 8.0;
  double hold = 5.0;
  double transition = 0.7;
  int stops = 7;

  /** Where the visitor is at scene time `elapsed`. Before `start`,
   *  `running` is false and the caller shows its home view; during a
   *  change, `phase` runs 0→1 and `previous` is the stop being left. */
  struct At {
    bool running = false;
    int stop = 0;
    int previous = -1;
    /** <0 outside a change; 0→1 across one. */
    float phase = -1.0f;
  };

  [[nodiscard]] At at(double elapsed) const {
    At a;
    if (elapsed < start) return a;
    a.running = true;
    const double u = std::fmod(elapsed - start, (double)stops * hold);
    a.stop = (int)(u / hold);
    a.previous = a.stop == 0 ? -1 : a.stop - 1;
    const double within = u - (double)a.stop * hold;
    if (within < transition) a.phase = (float)(within / transition);
    return a;
  }

  /** The same change quantised to `steps` discrete frames — what a
   *  stepped shape-wipe advances through. -1 outside a change. */
  [[nodiscard]] static int step(float phase, int steps) {
    if (phase < 0.0f) return -1;
    return std::min(steps, 1 + (int)(phase * (float)steps));
  }
};

}  // namespace twoadvanced
