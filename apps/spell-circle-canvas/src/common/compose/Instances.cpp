#include "sigilcompose/Instances.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkSurface.h>

#include <cmath>

namespace sigil::compose::instancing {

size_t Pool::add(SkPoint position, int frame, float rotateRadians, float scale,
                 SkColor4f tint) {
  m_positions.push_back(position);
  m_rotations.push_back(rotateRadians);
  m_scales.push_back(scale);
  m_tints.push_back(tint);
  m_frames.push_back(frame);
  // The opt-in lanes ride along when present, each at its neutral value
  // (unit scale, whole-cell window, fully opaque). Every has*() test is a
  // length comparison against the position lane, so a lane allowed to lag
  // here would silently switch itself off — or, after a clear() and
  // re-fill, line up again and apply a previous generation's values.
  if (!m_sizes.empty()) m_sizes.push_back({1.0f, 1.0f});
  if (!m_alphas.empty()) m_alphas.push_back(1.0f);
  if (!m_texWindows.empty()) m_texWindows.push_back(SkRect::MakeWH(1.0f, 1.0f));
  ++m_revision;
  return m_positions.size() - 1;
}

void Pool::clear() {
  m_positions.clear();
  m_rotations.clear();
  m_scales.clear();
  m_tints.clear();
  m_frames.clear();
  m_sizes.clear();
  m_alphas.clear();
  m_texWindows.clear();
  ++m_revision;
}

void Pool::resize(size_t n) {
  m_positions.resize(n, {0, 0});
  m_rotations.resize(n, 0.0f);
  m_scales.resize(n, 1.0f);
  m_tints.resize(n, {1, 1, 1, 1});
  m_frames.resize(n, 0);
  if (!m_sizes.empty()) m_sizes.resize(n, {1.0f, 1.0f});
  if (!m_alphas.empty()) m_alphas.resize(n, 1.0f);
  if (!m_texWindows.empty()) m_texWindows.resize(n, SkRect::MakeWH(1.0f, 1.0f));
  ++m_revision;
}

std::span<SkRect> Pool::texWindows() {
  if (m_texWindows.size() != m_positions.size())
    m_texWindows.resize(m_positions.size(), SkRect::MakeWH(1.0f, 1.0f));
  return m_texWindows;
}

std::span<SkSize> Pool::sizes() {
  if (m_sizes.size() != m_positions.size())
    m_sizes.resize(m_positions.size(), {1.0f, 1.0f});
  return m_sizes;
}

std::span<float> Pool::alphas() {
  if (m_alphas.size() != m_positions.size())
    m_alphas.resize(m_positions.size(), 1.0f);
  return m_alphas;
}

int Atlas::cell(Element tree, SkSize logicalSize) {
  tree.width(logicalSize.width()).height(logicalSize.height());
  m_cells.push_back({std::move(tree), logicalSize});
  m_sheet.reset();
  return (int)m_cells.size() - 1;
}

int Atlas::variants(int count, SkSize logicalSize,
                    const std::function<Element(int)>& make) {
  int first = -1;
  for (int v = 0; v < count; ++v) {
    const int idx = cell(make(v), logicalSize);
    if (v == 0) first = idx;
  }
  return first;
}

int Atlas::variants(
    int count, const std::function<std::pair<Element, SkSize>(int)>& make) {
  int first = -1;
  for (int v = 0; v < count; ++v) {
    auto [tree, size] = make(v);
    const int idx = cell(std::move(tree), size);
    if (v == 0) first = idx;
  }
  return first;
}

bool Atlas::ensureBaked(sigil::weave::FontContext& fonts) {
  if (m_sheet) return true;
  if (m_cells.empty()) return false;
  // Shelf pack in baked pixels.
  m_tex.assign(m_cells.size(), SkRect::MakeEmpty());
  float penX = 0, penY = 0, shelfH = 0, sheetW = 0;
  for (size_t i = 0; i < m_cells.size(); ++i) {
    const float w = m_cells[i].size.width() * m_oversample;
    const float h = m_cells[i].size.height() * m_oversample;
    if (penX > 0 && penX + w > kMaxSheetWidth) {
      penY += shelfH;
      penX = 0;
      shelfH = 0;
    }
    m_tex[i] = SkRect::MakeXYWH(penX, penY, w, h);
    penX += w;
    shelfH = std::max(shelfH, h);
    sheetW = std::max(sheetW, penX);
  }
  const int sheetWi = std::max(1, (int)std::ceil(sheetW));
  const int sheetHi = std::max(1, (int)std::ceil(penY + shelfH));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(sheetWi, sheetHi));
  if (!surface) return false;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  for (size_t i = 0; i < m_cells.size(); ++i) {
    // snapshot() sizes by the ROOT'S CHILDREN and ignores the root's own
    // dimensions — the cell tree already carries forced dims, so wrapping
    // it in a plain shell gives the picture exactly the cell's size.
    sk_sp<SkPicture> picture = snapshot(box().child(m_cells[i].tree), fonts);
    if (!picture) continue;
    canvas.save();
    canvas.translate(m_tex[i].left(), m_tex[i].top());
    canvas.scale(m_oversample, m_oversample);
    canvas.drawPicture(picture);
    canvas.restore();
  }
  m_sheet = surface->makeImageSnapshot();
  return m_sheet != nullptr;
}

namespace detail {

void stamp(SkCanvas& canvas, const PaintContext& ctx, Atlas& atlas,
           const Pool& pool, SkBlendMode blend) {
  if (!ctx.fonts || pool.size() == 0 || !atlas.ensureBaked(*ctx.fonts)) return;
  const float inv = 1.0f / atlas.oversample();
  const bool cull = pool.size() >= kCullThreshold;
  const SkRect clip = cull ? canvas.getLocalClipBounds() : SkRect::MakeEmpty();

  // Scratch reuse: a Live pool stamps every frame, so these buffers are
  // kept and refilled rather than allocated per stamp. thread_local
  // because a canvas may be painted from any one thread at a time, and
  // sharing one static across threads would interleave the fills.
  static thread_local std::vector<SkRSXform> xforms;
  static thread_local std::vector<SkRect> tex;
  static thread_local std::vector<SkColor> colors;
  static thread_local std::vector<SkSize> sizes;
  xforms.clear();
  tex.clear();
  colors.clear();
  sizes.clear();
  xforms.reserve(pool.size());
  tex.reserve(pool.size());
  colors.reserve(pool.size());
  bool tinted = false;

  const auto positions = pool.positions();
  const auto rotations = pool.rotations();
  const auto scales = pool.scales();
  const auto tints = pool.tints();
  const auto frames = pool.frames();
  const auto poolSizes = pool.sizes();  // empty unless the lane exists
  const auto poolWindows = pool.texWindows();
  const bool windowed = poolWindows.size() == positions.size();
  const bool nonUniform = poolSizes.size() == positions.size();
  if (nonUniform) sizes.reserve(pool.size());
  const int frameCount = atlas.frameCount();
  for (size_t i = 0; i < positions.size(); ++i) {
    const int frame = std::clamp(frames[i], 0, frameCount - 1);
    SkRect cellTex = atlas.frameTex(frame);
    if (cellTex.isEmpty()) continue;
    if (windowed) {
      const SkRect& w = poolWindows[i];
      cellTex = SkRect::MakeXYWH(cellTex.left() + w.left() * cellTex.width(),
                                 cellTex.top() + w.top() * cellTex.height(),
                                 w.width() * cellTex.width(),
                                 w.height() * cellTex.height());
      if (cellTex.isEmpty()) continue;
    }
    const float scale = scales[i] * inv;
    const SkSize sizeMul = nonUniform ? poolSizes[i] : SkSize{1.0f, 1.0f};
    if (cull) {
      const float widest =
          std::max(std::abs(sizeMul.width()), std::abs(sizeMul.height()));
      const float reach = 0.5f *
                          SkPoint{cellTex.width(), cellTex.height()}.length() *
                          scale * std::max(widest, 1.0f);
      const SkPoint p = positions[i];
      if (p.fX + reach < clip.left() || p.fX - reach > clip.right() ||
          p.fY + reach < clip.top() || p.fY - reach > clip.bottom())
        continue;
    }
    xforms.push_back(SkRSXform::MakeFromRadians(
        scale, rotations[i], positions[i].fX, positions[i].fY,
        cellTex.width() * 0.5f, cellTex.height() * 0.5f));
    tex.push_back(cellTex);
    if (nonUniform) sizes.push_back(sizeMul);
    SkColor4f t = tints[i];
    if (pool.hasAlphas())
      t.fA *= pool.alphas()[i];  // the fade lane composes with the tint
    const SkColor tint = t.toSkColor();
    tinted |= tint != SK_ColorWHITE;
    colors.push_back(tint);
  }
  if (xforms.empty()) return;
  // All-white tints modulate to identity, so the colors lane is dropped
  // entirely when nothing is tinted — the common case for UI sprites.
  // drawSpriteAtlas is used rather than a native atlas draw because it
  // decomposes to one drawVertices on every backend: the native call draws
  // nothing on some of them, including when a picture recorded elsewhere
  // replays there.
  gpuimg::drawSpriteAtlas(canvas, atlas.gpuCache, atlas.image(), xforms.data(),
                          tex.data(), tinted ? colors.data() : nullptr,
                          xforms.size(), SkSamplingOptions(atlas.filter()),
                          blend, nonUniform ? sizes.data() : nullptr);
}

}  // namespace detail

std::optional<size_t> pick(const Pool& pool, const Atlas& atlas,
                           SkPoint point) {
  const auto positions = pool.positions();
  const auto rotations = pool.rotations();
  const auto scales = pool.scales();
  const auto frames = pool.frames();
  const auto sizes = pool.sizes();
  const auto windows = pool.texWindows();
  const bool nonUniform = sizes.size() == positions.size();
  const bool windowed = windows.size() == positions.size();
  const int frameCount = atlas.frameCount();
  if (frameCount == 0) return std::nullopt;
  for (size_t n = positions.size(); n-- > 0;) {
    const int frame = std::clamp(frames[n], 0, frameCount - 1);
    SkSize half = atlas.frameSize(frame);
    if (half.isEmpty()) continue;
    if (windowed)
      half = {half.width() * windows[n].width(),
              half.height() * windows[n].height()};
    const SkSize mul = nonUniform ? sizes[n] : SkSize{1.0f, 1.0f};
    const float sx = scales[n] * mul.width(), sy = scales[n] * mul.height();
    if (sx == 0.0f || sy == 0.0f) continue;
    // Inverse of the stamp's RSXform: translate to the anchor (the quad
    // centre), un-rotate, un-scale, test against the half-extents.
    const float dx = point.fX - positions[n].fX;
    const float dy = point.fY - positions[n].fY;
    const float c = std::cos(-rotations[n]), s = std::sin(-rotations[n]);
    const float lx = (dx * c - dy * s) / sx;
    const float ly = (dx * s + dy * c) / sy;
    if (std::abs(lx) <= half.width() * 0.5f &&
        std::abs(ly) <= half.height() * 0.5f)
      return n;
  }
  return std::nullopt;
}

Element instances(std::shared_ptr<Atlas> atlas,
                  std::shared_ptr<const Pool> pool, Mode mode,
                  SkBlendMode blend) {
  if (mode == Mode::Live) {
    return custom([atlas = std::move(atlas), pool = std::move(pool), blend](
                      SkCanvas& canvas, const PaintContext& ctx) {
             detail::stamp(canvas, ctx, *atlas, *pool, blend);
           })
        .absolute()
        .inset(0)
        .cache(Cache::None);
  }
  detail::DataProps props{atlas, pool, pool->revision(), blend};
  return memo(std::move(props), [](const detail::DataProps& p) {
    return custom([atlas = p.atlas, pool = p.pool, blend = p.blend](
                      SkCanvas& canvas, const PaintContext& ctx) {
             detail::stamp(canvas, ctx, *atlas, *pool, blend);
           })
        .absolute()
        .inset(0);
  });
}

namespace place {

void grid(Pool& pool, size_t count, int columns, SkSize cell, SkPoint origin,
          SkSize gap) {
  pool.resize(count);
  auto positions = pool.positions();
  const int cols = std::max(1, columns);
  for (size_t i = 0; i < count; ++i) {
    const int col = (int)(i % (size_t)cols), row = (int)(i / (size_t)cols);
    positions[i] = {origin.fX + cell.width() * 0.5f +
                        (float)col * (cell.width() + gap.width()),
                    origin.fY + cell.height() * 0.5f +
                        (float)row * (cell.height() + gap.height())};
  }
  pool.commit();
}

void ring(Pool& pool, size_t count, SkPoint center, float radius,
          float startRadians, bool faceOut) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  for (size_t i = 0; i < count; ++i) {
    const float a = startRadians + (float)i * 2.0f * (float)M_PI / (float)count;
    positions[i] = {center.fX + std::cos(a) * radius,
                    center.fY + std::sin(a) * radius};
    if (faceOut) rotations[i] = a + (float)M_PI / 2.0f;
  }
  pool.commit();
}

void repeat(Pool& pool, size_t count, SkPoint start, SkPoint translate,
            float rotateStepRadians, float scaleStep, float opacityFrom,
            float opacityTo, int frame) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  auto scales = pool.scales();
  for (size_t i = 0; i < count; ++i) {
    positions[i] = {start.fX + translate.fX * (float)i,
                    start.fY + translate.fY * (float)i};
    rotations[i] = rotateStepRadians * (float)i;
    scales[i] = std::pow(scaleStep, (float)i);
  }
  if (opacityFrom != 1.0f || opacityTo != 1.0f) {
    auto alphas = pool.alphas();
    for (size_t i = 0; i < count; ++i) {
      const float t = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
      alphas[i] = opacityFrom + (opacityTo - opacityFrom) * t;
    }
  }
  if (frame >= 0) {
    auto frames = pool.frames();
    for (size_t i = 0; i < count; ++i) frames[i] = frame;
  }
  pool.commit();
}

}  // namespace place

}  // namespace sigil::compose::instancing
