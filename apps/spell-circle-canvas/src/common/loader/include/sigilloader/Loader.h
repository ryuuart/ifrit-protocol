#pragma once
// SigilLoader — the runtime resource system: game-engine-style mounted
// URIs over pluggable decode backends.
//
// A Hub maps URI prefixes to providers (today: filesystem directories;
// the scheme leaves room for pack files and in-memory providers), so
// application code asks for "res://ui/logo.png" and never touches the
// filesystem again. Resources are cached, hot-reloadable (poll()
// re-checks everything previously loaded), and typed:
//
//   hub.mount("res://", assetsDir);
//   auto bytes = hub.blob("res://data/table.bin");
//   auto text  = hub.text("res://shaders/glow.sksl");
//   auto img   = hub.image("res://ui/logo.png");            // stills+anim
//   auto hdr   = hub.image("res://light/probe.exr",         // OIIO: EXR,
//                          {.layer = "diffuse"});           //  PSD, TIFF…
//   auto info  = hub.probe("res://light/probe.exr");        // metadata
//
// The loader owns ACCESS: where bytes come from, caching, reload.
// What pixels mean is SigilImage's concern (sigilimage/Decode.h) — the
// Skia codecs plus, when built in, the OpenImageIO backend (EXR with
// layer selection, PSD, TIFF, HDR; float sources land as RGBA_F32).

#include <sigilimage/Decode.h>

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::loader {

/** Raw bytes of a resource. */
struct Blob {
  std::vector<std::byte> bytes;

  std::string_view asText() const {
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
  }
};

/** What a resource is, before (or without) fully decoding it. */
struct ResourceInfo {
  enum class Kind { Data, Image };
  Kind kind = Kind::Data;
  std::uintmax_t byteSize = 0;
  std::string format; // "png", "openexr", "psd", … (decoder's name)

  /** Image metadata (kind == Image); see sigil::image::ImageProbe. */
  sigil::image::ImageProbe image;
};

/** Image decode options come from SigilImage — the loader adds no
 *  format knowledge of its own. */
using ImageOptions = sigil::image::DecodeOptions;

/**
 * The resource hub: mount prefixes, ask for resources by URI.
 *
 * Loaded resources are cached per (uri, options); poll() re-stats every
 * previously requested resource and reloads the changed ones, returning
 * true so hosts can re-render (holders of old shared_ptrs keep the old
 * data — swap by re-asking). Failed lookups are NOT cached: a missing
 * file loads as soon as it appears.
 */
class Hub {
public:
  Hub() = default;

  /** Maps every URI starting with `prefix` to files under `dir`
   *  ("res://" + "ui/logo.png" → dir/ui/logo.png). Longest matching
   *  prefix wins; re-mounting a prefix replaces it. */
  void mount(std::string prefix, std::filesystem::path dir);

  /** The mounted filesystem path a URI resolves to (empty when no
   *  mount matches — the URI is then tried as a plain path). */
  std::filesystem::path resolve(std::string_view uri) const;

  /** Raw bytes; null when unresolvable/unreadable. */
  std::shared_ptr<const Blob> blob(std::string_view uri);

  /** UTF-8 text convenience over blob(). */
  std::optional<std::string> text(std::string_view uri);

  /** Decoded image (stills and animations); null on failure. */
  std::shared_ptr<const sigil::image::ImageAsset>
  image(std::string_view uri, const ImageOptions &options = {});

  /** The raw decoded color data — every channel the source carries
   *  (EXR layers included) as named float planes; null on failure.
   *  See sigil::image::ChannelData for Skia composition helpers. */
  std::shared_ptr<const sigil::image::ChannelData>
  channels(std::string_view uri);

  /** Metadata without a full decode (dimensions, channels, layers,
   *  float-ness, animation frames); nullopt when unreadable. */
  std::optional<ResourceInfo> probe(std::string_view uri) const;

  /** Re-checks every previously loaded resource; reloads changes and
   *  drops entries whose files vanished. Returns true if anything
   *  changed. */
  bool poll();

private:
  struct Entry {
    std::shared_ptr<const Blob> blob;
    std::shared_ptr<const sigil::image::ImageAsset> image;
    std::shared_ptr<const sigil::image::ChannelData> channels;
    ImageOptions imageOptions;
    std::filesystem::file_time_type mtime{};
  };

  bool reload(const std::string &key, Entry &entry);
  static std::string cacheKey(std::string_view uri,
                              const ImageOptions *options);

  std::vector<std::pair<std::string, std::filesystem::path>> m_mounts;
  std::map<std::string, Entry, std::less<>> m_entries;
};

} // namespace sigil::loader
