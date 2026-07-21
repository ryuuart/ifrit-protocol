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
   *  Identity flows through: the reconciler sees one changed recipe. */
  Pattern &seed(uint32_t s) {
    if (m_state && m_state->seed != s) {
      m_state->seed = s;
      m_state->baked.reset();
    }
    return *this;
  }
  /** Swap the element tile (element-tile patterns' regeneration). */
  Pattern &retile(Element tileTree) {
    if (m_state) {
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
    return Material::image(st.baked, SkTileMode::kRepeat, SkTileMode::kRepeat,
                           local, SkSamplingOptions(SkFilterMode::kLinear));
  }

  std::shared_ptr<State> m_state;
  float m_scale = 1.0f;
  float m_rotate = 0.0f;
};

} // namespace sigil::compose
