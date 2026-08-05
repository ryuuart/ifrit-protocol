#pragma once

/** @file
 * SigilCompose Pattern — procedural, regenerable tiled textures: an
 * Islamic-tessellation ground, a halftone field, a speckled paper —
 * parameterized, seeded, and cheap to re-roll at runtime.
 *
 * A Pattern is a RECIPE for one tile plus a mapping (scale, rotation,
 * offset). The tile bakes ONCE into an SkImage memoized on the shared
 * state, wraps as a repeating shader, and rides the Material path — so a
 * pattern fill caches and prunes, and regeneration is explicit: `.seed(n)`
 * or `retile()` drops the bake, the next `material()` re-renders the tile,
 * and the reconciler sees a changed recipe exactly once. Rotation, scale
 * and offset act on the shader matrix only — no rebake, and a rotated
 * repeat stays seamless.
 *
 * Two tile sources:
 *  - a PatternProgram (seeded raw drawing — the generator route; the stock
 *    generators in <sigilcompose/Patterns.h> are these), or
 *  - an ELEMENT TREE: a tile built from boxes/text/materials, baked via
 *    snapshot().
 *
 * THE BAKE IS THE IDENTITY, and that decides where a Pattern is stored.
 * Hold one where you hold assets — a sketch member, a model field.
 * Re-describing with a freshly minted Pattern each frame mints a fresh
 * shared state with no bake in it, so every render re-renders the tile.
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
   *  The reconciler sees one changed recipe.
   *
   *  COPY-ON-WRITE. A Pattern is a value whose every other setter (scale,
   *  rotate, offset, sampling) is per-object, so re-rolling a COPY must not
   *  re-roll the pattern it was copied from: without the detach, re-seeding
   *  a copy would drop the original's bake and silently regenerate every
   *  element still drawing the old tile. Holding the ONE Pattern and
   *  re-seeding it copies nothing — no other reference to the state exists.
   *
   *  This and `retile()` are the only copy-on-write points; the mapping
   *  setters never touch the shared state. */
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
   *  Phase is the defining property of a great many repeats: a twill
   *  advances one thread per pick, a conveyor belt moves, a barber pole
   *  turns.
   *
   *  Describe-time: this form moves only when the element is re-described.
   *  The BOUND overload below is the live sibling. */
  Pattern &offset(SkPoint px) {
    m_offset = px;
    return *this;
  }
  /** Pan the repeat LIVE — the bound form of the same word. Assign the
   *  Outputs and the conveyor moves, the twill marches, with NO
   *  re-describe and no rebake; either axis may be null. Adds to the
   *  static offset(), which is then the phase origin.
   *
   *  Rides `Material::offset`'s bound-matrix channel, so the node carries
   *  content volatility while the values move; once they hold still the
   *  library's stability detection releases it back to the cached tier,
   *  and it re-declares volatile on the frame the pan resumes. */
  Pattern &offset(const choreograph::Output<float> *x,
                  const choreograph::Output<float> *y) {
    m_boundX = x;
    m_boundY = y;
    return *this;
  }
  /** How the baked tile samples. Defaults to linear, which is right for
   *  organic tiles and wrong for anything on a pixel grid — a woven
   *  cloth, a dither, a bitmap-font sheet all want nearest. */
  Pattern &sampling(SkSamplingOptions options) {
    m_sampling = options;
    return *this;
  }
  uint32_t currentSeed() const { return m_state ? m_state->seed : 0; }

  /** Bake-once + wrap as a repeating Material. PROGRAM TILES ONLY: an
   *  element-tile Pattern has no font context here, so it draws nothing
   *  and returns an EMPTY material — use the overload below. */
  Material material() const { return bake(nullptr); }
  /** Element-tile overload, and the required one for element tiles: the
   *  tree is laid out and shaped during the bake, which needs the fonts. */
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

  /** The copy-on-write step: a recipe shared with another Pattern is
   *  cloned before it is edited. The clone keeps the bake — both callers
   *  drop it immediately afterwards, and seed()/retile() are the only
   *  editors of the shared state. */
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
      m.offset(m_boundX, m_boundY); // the live pan rides Material's
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
