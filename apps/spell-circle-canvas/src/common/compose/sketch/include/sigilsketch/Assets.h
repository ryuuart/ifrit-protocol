#pragma once
// Sketch-facing assets: a thin veneer over sigil::loader::Hub. The
// sketch's assets directory mounts at "res://"; image(name) keeps the
// forgiving sketch contract (magenta placeholder for missing files,
// heals + hot-swaps on change), and hub() opens the full resource
// surface — text, blobs, metadata probes, EXR layers, PSD — without
// the sketch ever touching the filesystem.

#include <sigilloader/Loader.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace sigil::compose::sketch {

class Assets {
public:
  explicit Assets(std::filesystem::path root);

  /** The image at "res://<name>", cached by the hub. Never null: a
   *  missing or undecodable file yields the placeholder until it
   *  becomes loadable. */
  std::shared_ptr<const sigil::image::ImageAsset>
  image(std::string_view name);

  /** The full resource hub (text/blob/probe/EXR layers…) with the
   *  sketch's assets directory mounted at "res://". */
  sigil::loader::Hub &hub() { return m_hub; }

  /** Re-checks everything: returns true when a loaded resource changed
   *  on disk OR a placeholder's file appeared (host re-runs setup). */
  bool poll();

  const std::filesystem::path &root() const { return m_root; }

private:
  std::filesystem::path m_root;
  sigil::loader::Hub m_hub;
  std::map<std::string, bool, std::less<>> m_placeholders; // name → waiting
  std::shared_ptr<const sigil::image::ImageAsset> m_placeholder;
};

} // namespace sigil::compose::sketch
