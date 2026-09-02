#pragma once

/** @file
 * SigilCompose Pattern — a repeating fill built from one tile, drawn by a
 * program or described as an Element tree. The mechanism — one bake,
 * a mapping, an explicit reseed — is SigilMaterial's Tile; this value
 * adds the element-tree tile, the bound pan, and the material a node
 * fills with.
 *
 * A Pattern is a RECIPE for one tile plus a mapping (scale, rotation,
 * offset). The tile bakes ONCE, memoized on shared state, wraps as a
 * repeating shader, and rides the Material path — so a pattern fill
 * caches and prunes, and regeneration is explicit: `.seed(n)` or
 * `retile()` drops the bake, the next `material()` re-renders the tile,
 * and the reconciler sees a changed recipe exactly once. Rotation, scale
 * and offset act on the shader matrix only — no rebake, and a rotated
 * repeat stays seamless.
 *
 * THE BAKE IS THE IDENTITY, and that decides where a Pattern is stored.
 * Hold one where you hold assets — a sketch member, a model field.
 * Re-describing with a freshly minted Pattern each frame mints a fresh
 * shared state with no bake in it, so every render re-renders the tile.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/skia/Paint.h>

#include <memory>
#include <optional>
#include <utility>

#include "sigilcompose/Compose.h"

namespace sigil::compose {

/** A repeating fill built from one tile. The tile is regenerated on
 *  demand from the pattern's seed, so restyling every use of a pattern is
 *  a matter of changing the seed rather than rebuilding it. */
class Pattern {
 public:
  Pattern() = default;
  /** A pattern over a SigilMaterial tile — what the stock generators and
   *  the kit's panels return. */
  Pattern(sigil::material::pattern::Tile
              tile)  // NOLINT(google-explicit-constructor)
      : m_tile(std::move(tile)) {}

  /** A generator tile (the procedural route). */
  static Pattern tile(SkSize size, sigil::material::pattern::Program draw) {
    return Pattern(sigil::material::pattern::Tile::of(size, std::move(draw)));
  }

  /** An element-tree tile (patterns are compositions too). The tree is
   *  forced to exactly the tile size so the repeat is seamless. */
  static Pattern tile(SkSize size, Element tileTree) {
    Pattern p;
    p.m_tile = sigil::material::pattern::Tile::of(size, {});
    tileTree.width(size.width()).height(size.height());
    p.m_tree = std::make_shared<Element>(std::move(tileTree));
    return p;
  }

  /** Change the seed → drop the bake → the next material() REGENERATES.
   *  The reconciler sees one changed recipe.
   *
   *  COPY-ON-WRITE. A Pattern is a value whose every other setter (scale,
   *  rotate, offset, sampling) is per-object, so re-rolling a COPY must not
   *  re-roll the pattern it was copied from: the tile's shared recipe is
   *  cloned before it is edited. Holding the ONE Pattern and re-seeding it
   *  copies nothing. This and `retile()` are the only copy-on-write
   *  points; the mapping setters never touch the shared state. */
  Pattern& seed(uint32_t s) {
    m_tile.seed(s);
    return *this;
  }
  /** Swap the element tile (element-tile patterns' regeneration).
   *  Copy-on-write for the same reason as seed(). */
  Pattern& retile(Element tileTree) {
    const SkSize size = m_tile.size();
    tileTree.width(size.width()).height(size.height());
    m_tree = std::make_shared<Element>(std::move(tileTree));
    m_tile.program({});
    return *this;
  }
  /** Mapping only — no rebake; a rotated repeat stays seamless. */
  Pattern& scale(float s) {
    m_tile.scale(s);
    return *this;
  }
  Pattern& rotate(float degrees) {
    m_tile.rotate(degrees);
    return *this;
  }
  /** Pan the repeat, in the node's own pixels — mapping only, no rebake.
   *  Phase is the defining property of a great many repeats: a twill
   *  advances one thread per pick, a conveyor belt moves, a barber pole
   *  turns.
   *
   *  Describe-time: this form moves only when the element is re-described.
   *  The BOUND overload below is the live sibling. */
  Pattern& offset(SkPoint px) {
    m_tile.offset(px);
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
  Pattern& offset(std::optional<motion::Animatable<float>> x,
                  std::optional<motion::Animatable<float>> y) {
    m_boundX = std::move(x);
    m_boundY = std::move(y);
    return *this;
  }
  /** How the baked tile samples. Defaults to linear, which is right for
   *  organic tiles and wrong for anything on a pixel grid — a woven
   *  cloth, a dither, a bitmap-font sheet all want nearest. */
  Pattern& sampling(SkSamplingOptions options) {
    m_sampling = options;
    return *this;
  }
  uint32_t currentSeed() const { return m_tile.currentSeed(); }
  /** The tile beneath: its bake, its mapping. */
  const sigil::material::pattern::Tile& tile() const { return m_tile; }

  /** Bake-once + wrap as a repeating Material. PROGRAM TILES ONLY: an
   *  element-tile Pattern has no font context here, so it draws nothing
   *  and returns an EMPTY material — use the overload below. */
  material::skia::Paint material() const { return bake(nullptr); }
  /** Element-tile overload, and the required one for element tiles: the
   *  tree is laid out and shaped during the bake, which needs the fonts. */
  material::skia::Paint material(sigil::weave::FontContext& fonts) const {
    return bake(&fonts);
  }

 private:
  material::skia::Paint bake(sigil::weave::FontContext* fonts) const {
    if (!m_tile.valid()) return {};
    if (m_tree && !m_tile.baked()) {
      if (!fonts) {
        SkDebugf(
            "Pattern::material(): an element tile needs the "
            "material(FontContext&) overload\n");
        return {};
      }
      // The element tile is the program, given the fonts it needs now;
      // set only when there is no bake so a settled tile is never
      // invalidated by asking for it again.
      std::shared_ptr<const Element> tree = m_tree;
      m_tile.program([tree, fonts](SkCanvas& canvas, SkSize, uint32_t) {
        // Wrap so the intrinsic-size root adopts the tile's forced dims.
        if (sk_sp<SkPicture> pic = snapshot(box().child(*tree), *fonts))
          canvas.drawPicture(pic);
      });
    }
    sk_sp<SkImage> baked = m_tile.image();
    if (!baked) return {};
    material::skia::Paint m = material::skia::Paint::image(
        std::move(baked), SkTileMode::kRepeat, SkTileMode::kRepeat,
        m_tile.mapping(), m_sampling);
    if (m_boundX || m_boundY)
      m.offset(m_boundX,
               m_boundY);  // the live pan rides material::skia::Paint's
                           // bound-matrix channel
    return m;
  }

  // Mutable because the element tile's program is installed at the first
  // bake, which is a const query on the value: the tile's shared state is
  // the bake's home, and setting its program there is the bake beginning.
  mutable sigil::material::pattern::Tile m_tile;
  std::shared_ptr<const Element> m_tree;
  std::optional<motion::Animatable<float>> m_boundX;
  std::optional<motion::Animatable<float>> m_boundY;
  SkSamplingOptions m_sampling{SkFilterMode::kLinear};
};

}  // namespace sigil::compose
