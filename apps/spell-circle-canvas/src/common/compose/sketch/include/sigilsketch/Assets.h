#pragma once
// Hot-reloading asset loader for sketches: images resolve against the
// sketch's assets directory through SigilImage (PNG/JPEG/WebP/GIF/AVIF,
// stills and animations). Missing files return a magenta placeholder
// (the game-dev tradition) and heal automatically when the file
// appears; editing an asset on disk swaps it into the running sketch.

#include <sigilimage/ImageAsset.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace sigil::compose::sketch {

class Assets {
public:
  explicit Assets(std::filesystem::path root);

  /** The image at `<root>/<name>`, cached. Never null: a missing or
   *  undecodable file yields the placeholder until it becomes
   *  loadable. */
  std::shared_ptr<const sigil::image::ImageAsset>
  image(std::string_view name);

  /** Re-checks every requested asset against the filesystem; returns
   *  true when something changed (host re-runs the sketch's setup). */
  bool poll();

  const std::filesystem::path &root() const { return m_root; }

private:
  struct Entry {
    std::shared_ptr<const sigil::image::ImageAsset> asset;
    std::filesystem::file_time_type mtime{};
    bool placeholder = false;
  };

  std::shared_ptr<const sigil::image::ImageAsset> load(
      const std::filesystem::path &path, Entry &entry);

  std::filesystem::path m_root;
  std::map<std::string, Entry, std::less<>> m_entries;
  std::shared_ptr<const sigil::image::ImageAsset> m_placeholder;
};

} // namespace sigil::compose::sketch
