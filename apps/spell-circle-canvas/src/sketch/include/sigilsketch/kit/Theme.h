#pragma once

/** @file
 * The look a sheet is set in, as one comparable value: the five colours,
 * the registers the four lines are set in, and the distances between
 * things. It is bound for a scope and read where a component is
 * described, so a component four levels down obeys it without being
 * handed it.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/reconcile/Env.h>
#include <sigilweave/style/TextStyle.h>

namespace sigil::sketch::kit {

/** THE FIVE COLOURS A SHEET IS SET IN.
 *
 *  A comparable VALUE, and the comparison is exact: two palettes are
 *  equal when everything described under them describes the same, which
 *  is what lets the reconciler's structural prune stand in for a
 *  dependency tracker. Nothing here is perceptual and nothing is
 *  epsilon'd. */
struct Palette {
  /** Behind the whole sheet. */
  SkColor4f ground{0.07f, 0.07f, 0.085f, 1};
  /** Behind one specimen — the well a subject is shown in. */
  SkColor4f cellGround{0.105f, 0.11f, 0.125f, 1};
  /** The title, and the call a cell names. */
  SkColor4f ink{0.90f, 0.90f, 0.92f, 1};
  /** The subtitle, the footer, and the remark under a cell. */
  SkColor4f ash{0.55f, 0.56f, 0.62f, 1};
  /** The hairline under the header and over the footer. */
  SkColor4f rule{0.20f, 0.21f, 0.25f, 1};
  bool operator==(const Palette&) const = default;
};

/** ONE LINE OF TYPE: which of the theme's two faces it is set in, how
 *  large, and how far apart its clusters stand.
 *
 *  `track` is px added after each cluster, which is what a text style
 *  takes; a reference quoting tracking per mille converts where the em
 *  size is known, which is the call site and not here. */
struct Register {
  float size = 11;
  float track = 0;
  /** false sets the line in the theme's first face, true in its second —
   *  the one a CALL is set in, which is monospaced on the house sheet and
   *  whatever a theme puts there on another. */
  bool mono = false;
  bool operator==(const Register&) const = default;
};

/** THE REGISTERS A SHEET'S LINES ARE SET IN, and the two faces they are
 *  set with.
 *
 *  TWO FACES: `sans`, which a line is set in unless it says otherwise,
 *  and `mono`, the one a CALL is set in — monospaced on the house sheet,
 *  and a book serif on a sheet about body type.
 *
 *  A null face is the FontContext's own default plus its fallback chain
 *  — which is what a style that names no face gets, so leaving `sans`
 *  null and setting a line in it is the same style as setting it in
 *  nothing. A face is resolved once and held, because it is compared by
 *  POINTER wherever a style or an inherited value is compared, and a
 *  theme that resolved one per read would never compare equal to
 *  itself. */
struct TypeScale {
  Register title{14, 2.4f};
  Register subtitle{11.5f, 0.8f};
  Register footer{11, 0.4f};
  /** The call a cell names, over its specimen. */
  Register captionLabel{10.5f, 0, true};
  /** The remark read after the picture. */
  Register captionNote{10, 0.2f};
  sk_sp<SkTypeface> sans;
  sk_sp<SkTypeface> mono;
  bool operator==(const TypeScale&) const = default;
};

/** THE DISTANCES BETWEEN THINGS, px. */
struct Spacing {
  /** The page margins: the two sides, the top and the bottom. */
  float marginX = 24, marginTop = 20, marginBottom = 16;
  /** Between the title and its subtitle. */
  float subtitleGap = 6;
  /** Between the header and the content, and the content and the footer;
   *  a rule bisects it rather than adding to it. */
  float contentGap = 18;
  /** Between a caption line and the body it describes. */
  float captionGap = 7;
  /** Between a caption's two lines where they stand together. */
  float captionNoteGap = 4;
  /** Between neighbouring cells in a run. */
  float cellGap = 20;
  /** Inside a well, around its specimen. */
  float wellPadding = 0;
  bool operator==(const Spacing&) const = default;
};

/** THE LOOK, as one value.
 *
 *  A SEAM, not a privilege: a sketch's own `Theme` is indistinguishable
 *  from the house one at every call site, and every component here has
 *  correct behaviour with none bound at all — it falls back to
 *  `houseTheme()`, which is a value among peers rather than the value.
 *  Nothing in this library decides a look; it decides only the
 *  arrangement, and the house sheet is what most of this repository's
 *  sheets already happen to be set in. */
struct Theme {
  Palette palette;
  TypeScale type;
  Spacing spacing;
  /** Where a cell's caption lines stand relative to its body. */
  compose::kit::Caption::Where captionWhere =
      compose::kit::Caption::Where::Split;
  bool operator==(const Theme&) const = default;

  /** @p line in @p color, set in whichever of the two faces it names. */
  [[nodiscard]] weave::TextStyle style(const Register& line,
                                       SkColor4f color) const;
  /** A line in the sans face at an arbitrary size — for the text a sheet
   *  sets that is none of the four registers. */
  [[nodiscard]] weave::TextStyle sans(float size, SkColor4f color,
                                      float track = 0) const;
  /** The same in the mono face. */
  [[nodiscard]] weave::TextStyle mono(float size, SkColor4f color,
                                      float track = 0) const;

  /** THE SHEET'S VOICE: how every cell on it is captioned. @p noteMeasure
   *  is the width the remark wraps at, which is the cell's own width —
   *  the one distance a caption cannot inherit, because it is a fact
   *  about the specimen and not about the look. */
  [[nodiscard]] compose::kit::Caption voice(float noteMeasure) const;
};

/** THE HOUSE SHEET — the values most of this repository's specimen
 *  sheets already carry, stated once. Its mono face is resolved on the
 *  first call and held, so every sheet under it shares one face and
 *  every memo keyed on a style below it keeps hitting.
 *
 *  A sketch that wants another look builds its own `Theme` and binds it;
 *  a sketch that wants this one with two colours moved copies this and
 *  writes the two. */
[[nodiscard]] const Theme& houseTheme();

/** THE THEME IN SCOPE, or the house one where nothing bound a theme —
 *  which is what makes every component here correct on its own. */
[[nodiscard]] const Theme& theme();

/** BIND A THEME for everything described while this object lives, the
 *  way a provider does: an alias for the reconciler's own inherited
 *  value, so a sketch needs one include and one word.
 *
 *      sketch::kit::Provide look(sheetTheme());
 *      ctx.composer.render(sketch::kit::page({...}, content));
 *
 *  RAII and LIFO, and it costs one empty vector where nothing binds one.
 *  BIND IT WHERE THE TREE IS DESCRIBED: a sketch that describes again
 *  when its data changes does so outside the scope its setup opened, and
 *  a theme bound only there is not in scope for the second description.
 *  A callable the KERNEL invokes later — a `custom()` paint program, a
 *  memo's deferred describe — runs outside this scope, so such a lambda
 *  captures the colours it needs by value here, where the scope still
 *  stands. */
using Provide = sigil::core::env::Provide<Theme>;

}  // namespace sigil::sketch::kit
