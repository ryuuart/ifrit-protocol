#include "sigilsketch/core/Assets.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilio/hub/Network.h>

#include <system_error>

namespace sigil::sketch {

namespace {

/** The classic missing-texture checker: magenta/black, unmistakable. */
std::shared_ptr<const sigil::image::ImageAsset> makePlaceholder() {
  constexpr int kSize = 64, kCell = 16;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSize, kSize));
  SkCanvas& canvas = *surface->getCanvas();
  SkPaint paint;
  for (int y = 0; y < kSize / kCell; ++y)
    for (int x = 0; x < kSize / kCell; ++x) {
      paint.setColor(((unsigned)(x + y) & 1u) ? SK_ColorMAGENTA
                                              : SK_ColorBLACK);
      canvas.drawRect(SkRect::MakeXYWH((float)(x * kCell), (float)(y * kCell),
                                       kCell, kCell),
                      paint);
    }
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(surface->makeImageSnapshot()));
}

std::string uriFor(std::string_view name) {
  return "res://" + std::string(name);
}

}  // namespace

Assets::Assets(std::filesystem::path root) : m_root(std::move(root)) {
  m_hub.mount("res://", m_root);
  m_placeholder = makePlaceholder();
}

std::shared_ptr<const sigil::image::ImageAsset> Assets::image(
    std::string_view name) {
  if (auto asset = m_hub.image(uriFor(name))) {
    m_placeholders.erase(std::string(name));
    return asset;
  }
  m_placeholders.insert_or_assign(std::string(name), true);
  return m_placeholder;
}

std::shared_ptr<sigil::video::Video> Assets::video(
    std::string_view name, const sigil::video::DecodeOptions& options) {
  for (const CachedVideo& cached : m_videos)
    if (cached.name == name && cached.options == options) return cached.clip;

  const std::string uri = uriFor(name);
  const std::shared_ptr<const sigil::io::Bytes> encoded = m_hub.blob(uri);
  if (!encoded) return nullptr;
  std::shared_ptr<sigil::video::Video> clip =
      sigil::video::decodeVideo(encoded->bytes.data(), encoded->bytes.size(),
                                options, m_hub.resolve(uri));
  if (clip)
    m_videos.push_back(
        {.name = std::string(name), .options = options, .clip = clip});
  return clip;
}

bool Assets::poll() {
  bool changed = m_hub.poll();
  if (changed) m_videos.clear();
  // Placeholders heal the moment their file becomes loadable.
  for (auto it = m_placeholders.begin(); it != m_placeholders.end();) {
    if (m_hub.image(uriFor(it->first))) {
      it = m_placeholders.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }
  return changed;
}

bool requireCached(std::initializer_list<std::string_view> urls,
                   std::string* why, const std::filesystem::path& cacheDir) {
  const std::filesystem::path dir =
      cacheDir.empty() ? sigil::io::defaultNetworkCacheDir() : cacheDir;
  for (std::string_view url : urls) {
    // A fetch persists only when it succeeded — an HTTP error writes
    // nothing — so the file standing under the URL's own cache key is
    // the whole question. Its SIZE is asked for too, because a write
    // that was interrupted leaves a file that is there and holds
    // nothing, and a sketch given nothing draws its stand-in anyway.
    std::error_code ec;
    const std::filesystem::path cached = dir / sigil::io::networkCacheKey(url);
    if (std::filesystem::is_regular_file(cached, ec) && !ec &&
        std::filesystem::file_size(cached, ec) > 0 && !ec)
      continue;
    if (why)
      *why = "not in the IO hub's network cache on this machine \xe2\x80\x94 " +
             std::string(url);
    return false;
  }
  return true;
}

}  // namespace sigil::sketch
