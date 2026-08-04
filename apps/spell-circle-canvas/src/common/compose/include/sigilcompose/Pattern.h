#pragma once

/** @file
 * SigilCompose Pattern — runtime-PROCEDURAL, REGENERABLE tiled textures.
 * The user-model: "generate different pattern backgrounds at runtime and
 * apply them to different elements" — an Islamic-tessellation ground, a
 * halftone field, a speckled paper — parameterized, seeded, and cheap to
 * re-roll.
 *
 * A Pattern is a RECIPE for one tile plus a mapping (scale/rotation). The
 * tile bakes ONCE into an SkImage (memoized on the shared state), wraps as
 * a repeating shader, and rides the Material path — so a pattern fill
 * caches, prunes (same bake → pointer-equal recipe), and re-generates by
 * DESIGN: `.seed(n)` (or `retile()`) drops the bake, the next `material()`
 * re-renders the tile, and the reconciler sees a changed recipe exactly
 * once. Rotation/scale act on the shader matrix only — no rebake, and a
 * rotated repeat stays seamless.
 *
 * Two tile sources, same discipline as everything else here:
 *  - a PatternProgram (seeded raw drawing — the generator route; the stock
 *    generators in <sigilcompose/Patterns.h> are these), or
 *  - an ELEMENT TREE (patterns are compositions too: a tile built from
 *    boxes/text/materials, baked via snapshot()).
 *
 * Hold a Pattern where you hold assets (a sketch member, a model field);
 * re-describing it each frame with a fresh Pattern object would re-bake
 * per render — the shared bake is the identity.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Material.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace sigil::compose {

/** Draw ONE tile into [0,0 .. size); `seed` is the pattern's current seed —
 *  same seed, same tile (determinism is what makes regeneration a choice). */
using PatternProgram = std::function<void(SkCanvas &, SkSize, uint32_t seed)>;

class Pattern {
public:
  Pattern() = default;

  /** A generator tile (the procedural route). */
  static Pattern tile(SkSize size, PatternProgram draw) {
    Pattern p;
    p.m_state = std::make_shared<State>();
    p.m_state->size = size;
    p.m_state->draw = std::move(draw);
    return p;
  }

  /** An element-tree tile (patterns are compositions too). The tree is
   *  forced to exactly the tile size so the repeat is seamless. */
  static Pattern tile(SkSize size, Element tileTree) {
    Pattern p;
    p.m_state = std::make_shared<State>();
    p.m_state->size = size;
    tileTree.width(size.width()).height(size.height());
    p.m_state->tree = std::move(tileTree);
    return p;
  }

  /** Change the seed → drop the bake → the next material() REGENERATES.
   *  Identity flows through: the reconciler sees one changed recipe.
   *
   *  COPY-ON-WRITE, like `Material::uniform`: a Pattern is a VALUE whose
   *  every other setter (scale/rotate/offset/sampling) is per-object, so
   *  re-rolling a COPY must not re-roll the pattern it was copied from —
   *  which is what a shared recipe did, silently dropping the original's
   *  bake and re-generating every element that still drew the old tile
   *  (audit E5). Holding the ONE Pattern and re-seeding it is unchanged:
   *  nothing else references the state, so nothing is copied. */
  Pattern &seed(uint32_t s) {
    if (m_state && m_state->seed != s) {
      detachState();
      m_state->seed = s;
      m_state->baked.reset();
    }
    return *this;
  }
  /** Swap the element tile (element-tile patterns' regeneration).
   *  Copy-on-write for the same reason as seed(). */
  Pattern &retile(Element tileTree) {
    if (m_state) {
      detachState();
      tileTree.width(m_state->size.width()).height(m_state->size.height());
      m_state->tree = std::move(tileTree);
      m_state->baked.reset();
    }
    return *this;
  }
  /** Mapping only — no rebake; a rotated repeat stays seamless. */
  Pattern &scale(float s) {
    m_scale = s;
    return *this;
  }
  Pattern &rotate(float degrees) {
    m_rotate = degrees;
    return *this;
  }
  /** Pan the repeat, in the node's own pixels — mapping only, no rebake.
   *
   *  This is plumbing that already existed: `bake()` hands its matrix to
   *  `Material::image`, whose `localMatrix` has always taken a
   *  translation, so `Pattern` was exposing two thirds of a matrix its
   *  own backend takes whole. Phase is the defining property of a
   *  surprising number of repeats — a twill advances one thread per pick,
   *  a conveyor belt moves, a barber pole turns — and two studies wrote
   *  the pattern twice for want of it.
   *
   *  Describe-time: this form re-describes to move. The BOUND overload
   *  below is the live sibling. */
  Pattern &offset(SkPoint px) {
    m_offset = px;
    return *this;
  }
  /** Pan the repeat LIVE (ROADMAP §14-a): the same word, the bound form —
   *  `fill(&output)`'s grammar on the pan. Assign the Outputs and the
   *  conveyor moves, the twill marches, with NO re-describe and no
   *  rebake; either axis may be null. Adds to the static offset() (the
   *  phase origin). Rides `Material::offset`'s bound-matrix channel:
   *  content volatility while moving, §20's measured-stability release
   *  once it holds still — a parked conveyor costs what a static pattern
   *  costs, promotion included — and the re-declare the frame it resumes. */
  Pattern &offset(const choreograph::Output<float> *x,
                  const choreograph::Output<float> *y) {
    m_boundX = x;
    m_boundY = y;
    return *this;
  }
  /** How the baked tile samples. Defaults to linear, which is right for
   *  organic tiles and wrong for anything on a pixel grid — a woven
   *  cloth, a dither, a bitmap-font sheet. `Material::image()` has always
   *  taken this; `Pattern` did not, which is the same signature-diff
   *  discoverability trap `Element::sampling()` closed on the image
   *  leaf. */
  Pattern &sampling(SkSamplingOptions options) {
    m_sampling = options;
    return *this;
  }
  uint32_t currentSeed() const { return m_state ? m_state->seed : 0; }

  /** Bake-once + wrap as a repeating Material (program tiles). */
  Material material() const { return bake(nullptr); }
  /** Element-tile overload — text in tiles needs the fonts. */
  Material material(sigil::weave::FontContext &fonts) const {
    return bake(&fonts);
  }

private:
  struct State {
    SkSize size = {32, 32};
    PatternProgram draw;
    std::optional<Element> tree;
    uint32_t seed = 1;
    sk_sp<SkImage> baked; // the memoized tile; reset regenerates
  };

  /** The copy-on-write step (Material::detachLive's shape): a recipe
   *  shared with another Pattern is cloned before it is edited. The clone
   *  keeps the bake — the caller drops it right after, and retile()/seed()
   *  are the only editors. */
  void detachState() {
    if (m_state && m_state.use_count() > 1)
      m_state = std::make_shared<State>(*m_state);
  }

  Material bake(sigil::weave::FontContext *fonts) const {
    if (!m_state)
      return {};
    State &st = *m_state;
    if (!st.baked) {
      const int w = std::max(1, (int)std::ceil(st.size.width()));
      const int h = std::max(1, (int)std::ceil(st.size.height()));
      sk_sp<SkSurface> surface =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
      if (!surface)
        return {};
      SkCanvas *canvas = surface->getCanvas();
      canvas->clear(SK_ColorTRANSPARENT);
      if (st.tree) {
        if (!fonts) {
          SkDebugf("Pattern::material(): an element tile needs the "
                   "material(FontContext&) overload\n");
          return {};
        }
        // Wrap so the intrinsic-size root adopts the tile's forced dims.
        if (sk_sp<SkPicture> pic =
                snapshot(box().child(*st.tree), *fonts))
          canvas->drawPicture(pic);
      } else if (st.draw) {
        st.draw(*canvas, {(float)w, (float)h}, st.seed);
      }
      st.baked = surface->makeImageSnapshot();
    }
    SkMatrix local = SkMatrix::RotateDeg(m_rotate);
    local.preScale(m_scale, m_scale);
    local.postTranslate(m_offset.fX, m_offset.fY);
    Material m = Material::image(st.baked, SkTileMode::kRepeat,
                                 SkTileMode::kRepeat, local, m_sampling);
    if (m_boundX || m_boundY)
      m.offset(m_boundX, m_boundY); // §14-a: the live pan rides Material's
                                    // bound-matrix channel
    return m;
  }

  std::shared_ptr<State> m_state;
  float m_scale = 1.0f;
  float m_rotate = 0.0f;
  SkPoint m_offset = {0, 0};
  const choreograph::Output<float> *m_boundX = nullptr;
  const choreograph::Output<float> *m_boundY = nullptr;
  SkSamplingOptions m_sampling{SkFilterMode::kLinear};
};

} // namespace sigil::compose
