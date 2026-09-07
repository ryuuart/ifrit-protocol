/** @file
 * Pixel art: the character grid read into marks, the three ways one sprite
 * is presented, and the sheet many of them are packed onto.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkTileMode.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
#include <utility>

namespace sigil::compose::kit {

std::optional<Sprite> pixelMap(std::span<const std::string> rows,
                               const SpriteKey& key) {
  if (key.chars.size() != key.colours.size()) return std::nullopt;
  if (rows.empty()) return Sprite{};
  const size_t w = rows.front().size();
  for (const std::string& row : rows)
    if (row.size() != w) return std::nullopt;
  const size_t h = rows.size();

  // The grid as indices first, so the merge below compares one number per
  // cell rather than a character it would have to look up again.
  std::vector<int> index(w * h, -1);
  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x) {
      const size_t at = key.chars.find(rows[y][x]);
      if (at == std::string_view::npos) return std::nullopt;
      index[y * w + x] = (int)at;
    }

  Sprite out;
  out.grid = {(int)w, (int)h};
  out.colours.assign(key.colours.begin(), key.colours.end());
  std::vector<char> taken(w * h, 0);
  for (size_t y = 0; y < h; ++y)
    for (size_t x = 0; x < w; ++x) {
      const size_t seed = y * w + x;
      if (taken[seed]) continue;
      const int idx = index[seed];
      taken[seed] = 1;
      if (out.colourOf(idx).fA <= 0) continue;  // the grid's own blank
      // Widest first, then as deep as that whole width stays this entry —
      // the largest rectangle anchored here, which is what keeps a flat
      // field of one colour to a single mark.
      size_t rw = 1;
      while (x + rw < w && index[seed + rw] == idx && !taken[seed + rw]) ++rw;
      size_t rh = 1;
      while (y + rh < h) {
        bool whole = true;
        for (size_t k = 0; k < rw; ++k) {
          const size_t at = (y + rh) * w + x + k;
          if (index[at] != idx || taken[at]) {
            whole = false;
            break;
          }
        }
        if (!whole) break;
        ++rh;
      }
      for (size_t j = 0; j < rh; ++j)
        for (size_t k = 0; k < rw; ++k) taken[(y + j) * w + x + k] = 1;
      out.rect((float)x, (float)y, (float)rw, (float)rh, idx);
    }
  return out;
}

namespace {
SkColor4f faded(const SkColor4f& colour, float alpha) {
  return alpha >= 1.0f
             ? colour
             : SkColor4f{colour.fR, colour.fG, colour.fB, colour.fA * alpha};
}
}  // namespace

void drawSprite(SkCanvas& canvas, const Sprite& sprite, SkPoint at,
                const SpriteStyle& style) {
  const PixelInk ink{canvas, style.cell, at};
  for (const SpriteRun& run : sprite.runs)
    ink.rect(run.x, run.y, run.w, run.h,
             faded(sprite.colourOf(run.index), style.alpha));
}

Element pixelSprite(const Sprite& sprite, const SpriteStyle& style) {
  Element root = stack()
                     .width(Dim((float)sprite.grid.width() * style.cell))
                     .height(Dim((float)sprite.grid.height() * style.cell));
  for (const SpriteRun& run : sprite.runs) {
    const SkColor4f colour = faded(sprite.colourOf(run.index), style.alpha);
    if (colour.fA <= 0) continue;
    root.child(box()
                   .left(Dim(run.x * style.cell))
                   .top(Dim(run.y * style.cell))
                   .width(Dim(run.w * style.cell))
                   .height(Dim(run.h * style.cell))
                   .fill(colour));
  }
  return root;
}

sk_sp<SkImage> spriteImage(const Sprite& sprite, const SpriteStyle& style) {
  const int w = (int)std::lround((float)sprite.grid.width() * style.cell);
  const int h = (int)std::lround((float)sprite.grid.height() * style.cell);
  if (w <= 0 || h <= 0) return nullptr;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  if (!surface) return nullptr;
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  drawSprite(*surface->getCanvas(), sprite, {0, 0}, style);
  return surface->makeImageSnapshot();
}

sk_sp<SkImage> indexImage(const Sprite& sprite, float cell) {
  const int w = (int)std::lround((float)sprite.grid.width() * cell);
  const int h = (int)std::lround((float)sprite.grid.height() * cell);
  if (w <= 0 || h <= 0) return nullptr;
  SkBitmap plane;
  plane.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  plane.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(plane);
  const PixelInk ink{canvas, cell, {0, 0}};
  for (const SpriteRun& run : sprite.runs) {
    if (run.index <= 0 || run.index > 255) continue;
    ink.rect(run.x, run.y, run.w, run.h,
             SkColor4f{(float)run.index / 255.0f, 0.0f, 0.0f, 1.0f});
  }
  plane.setImmutable();
  return plane.asImage();
}

// ---------------------------------------------------------------------------

void SpriteSheet::add(std::string name, Sprite sprite) {
  m_sheet.reset();
  for (Entry& entry : m_entries)
    if (entry.name == name) {
      entry.sprite = std::move(sprite);
      entry.rect = SkRect::MakeEmpty();
      return;
    }
  m_entries.push_back({std::move(name), std::move(sprite), {}});
}

const SpriteSheet::Entry* SpriteSheet::entryOf(std::string_view name) const {
  for (const Entry& entry : m_entries)
    if (entry.name == name) return &entry;
  return nullptr;
}

const Sprite* SpriteSheet::find(std::string_view name) const {
  const Entry* entry = entryOf(name);
  return entry ? &entry->sprite : nullptr;
}

std::vector<std::string_view> SpriteSheet::names() const {
  std::vector<std::string_view> out;
  out.reserve(m_entries.size());
  for (const Entry& entry : m_entries) out.push_back(entry.name);
  return out;
}

SkRect SpriteSheet::rect(std::string_view name) const {
  const Entry* entry = entryOf(name);
  return entry && m_sheet ? entry->rect : SkRect::MakeEmpty();
}

bool SpriteSheet::bake(const SpriteStyle& style) {
  m_sheet.reset();
  if (m_entries.empty()) return false;
  std::vector<SkSize> boxes;
  boxes.reserve(m_entries.size());
  for (const Entry& entry : m_entries)
    boxes.push_back({(float)entry.sprite.grid.width() * style.cell,
                     (float)entry.sprite.grid.height() * style.cell});
  // A gutter of one sheet pixel: a sheet exists to be sampled, and a
  // sprite flush against its neighbour bleeds into it the moment a stamp
  // lands off the pixel grid or is scaled.
  const Shelved packed = shelve(boxes, {.padding = 1.0f});
  if (packed.sheet.isEmpty()) return false;
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(packed.sheet.width(), packed.sheet.height()));
  if (!surface) return false;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  for (size_t i = 0; i < m_entries.size(); ++i) {
    m_entries[i].rect = packed.cells[i];
    drawSprite(canvas, m_entries[i].sprite,
               {packed.cells[i].left(), packed.cells[i].top()}, style);
  }
  m_sheet = surface->makeImageSnapshot();
  return m_sheet != nullptr;
}

Element SpriteSheet::cell(std::string_view name) const {
  const SkRect window = rect(name);
  if (!m_sheet || window.isEmpty()) return box().width(0).height(0);
  return box()
      .width(window.width())
      .height(window.height())
      .fill(material::skia::Paint::image(
          m_sheet, SkTileMode::kDecal, SkTileMode::kDecal,
          SkMatrix::Translate(-window.left(), -window.top()),
          SkSamplingOptions(SkFilterMode::kNearest)));
}

}  // namespace sigil::compose::kit
