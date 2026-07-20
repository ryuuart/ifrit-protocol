#include "ifritsketch/Assets.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>

namespace ifrit::compose::sketch {

namespace {

/** The classic missing-texture checker: magenta/black, unmistakable. */
std::shared_ptr<const ifrit::image::ImageAsset> makePlaceholder() {
  constexpr int kSize = 64, kCell = 16;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSize, kSize));
  SkCanvas &canvas = *surface->getCanvas();
  SkPaint paint;
  for (int y = 0; y < kSize / kCell; ++y)
    for (int x = 0; x < kSize / kCell; ++x) {
      paint.setColor(((x + y) & 1) ? SK_ColorMAGENTA : SK_ColorBLACK);
      canvas.drawRect(SkRect::MakeXYWH((float)(x * kCell),
                                       (float)(y * kCell), kCell, kCell),
                      paint);
    }
  return std::make_shared<ifrit::image::ImageAsset>(
      ifrit::image::ImageAsset::wrap(surface->makeImageSnapshot()));
}

} // namespace

Assets::Assets(std::filesystem::path root) : m_root(std::move(root)) {
  m_placeholder = makePlaceholder();
}

std::shared_ptr<const ifrit::image::ImageAsset>
Assets::image(std::string_view name) {
  auto it = m_entries.find(name);
  if (it != m_entries.end())
    return it->second.asset;
  Entry entry;
  entry.asset = load(m_root / name, entry);
  return m_entries.emplace(std::string(name), std::move(entry))
      .first->second.asset;
}

std::shared_ptr<const ifrit::image::ImageAsset>
Assets::load(const std::filesystem::path &path, Entry &entry) {
  std::error_code ec;
  const auto mtime = std::filesystem::last_write_time(path, ec);
  if (!ec) {
    if (auto decoded = ifrit::image::ImageAsset::load(path.string())) {
      entry.mtime = mtime;
      entry.placeholder = false;
      return std::make_shared<ifrit::image::ImageAsset>(
          std::move(*decoded));
    }
  }
  entry.placeholder = true;
  entry.mtime = {};
  return m_placeholder;
}

bool Assets::poll() {
  bool changed = false;
  for (auto &[name, entry] : m_entries) {
    const std::filesystem::path path = m_root / name;
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    const bool existsNow = !ec;
    if (entry.placeholder ? existsNow : (ec || mtime != entry.mtime)) {
      entry.asset = load(path, entry);
      changed = true;
    }
  }
  return changed;
}

} // namespace ifrit::compose::sketch
