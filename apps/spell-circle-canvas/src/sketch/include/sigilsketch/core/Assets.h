#pragma once

/** @file
 * Sketch-facing assets: a thin veneer over the resource hub, with the
 * forgiving contract a live-edited file wants — and the probe a sketch
 * over fetched art answers its availability with.
 */

#include <sigilimage/asset/ImageAsset.h>
#include <sigilio/IO.h>
#include <sigilvideo/decode/Decode.h>

#include <boost/container/flat_map.hpp>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::sketch {

/** THE FILES A SKETCH REACHES FOR that it did not generate.
 *
 *  The sketch's assets directory mounts at `res://`. `image()` keeps the
 *  forgiving contract a live-edited file wants — a magenta placeholder
 *  stands in for a missing or undecodable file and heals the moment one
 *  appears — and `hub()` opens the full resource surface (text, blobs,
 *  metadata probes, EXR layers, PSD) without the sketch ever touching
 *  the filesystem. */
class Assets {
 public:
  explicit Assets(std::filesystem::path root);

  /** The image at "res://<name>", cached by the hub. Never null: a
   *  missing or undecodable file yields the placeholder until it
   *  becomes loadable. */
  std::shared_ptr<const sigil::image::ImageAsset> image(std::string_view name);

  /** The video at "res://<name>", decoded on demand and cached with the
   *  requested device policy. Null when the resource is missing or cannot be
   *  decoded. The clip keeps only a small presentation-frame cache. */
  std::shared_ptr<sigil::video::Video> video(
      std::string_view name, const sigil::video::DecodeOptions& options = {});

  /** The full resource hub (text/blob/probe/EXR layers…) with the
   *  sketch's assets directory mounted at "res://". */
  sigil::io::Hub& hub() { return m_hub; }

  /** Re-checks everything: returns true when a loaded resource changed
   *  on disk OR a placeholder's file appeared (host re-runs setup). */
  bool poll();

  const std::filesystem::path& root() const { return m_root; }

 private:
  struct CachedVideo {
    std::string name;
    sigil::video::DecodeOptions options;
    std::shared_ptr<sigil::video::Video> clip;
  };

  std::filesystem::path m_root;
  sigil::io::Hub m_hub;
  boost::container::flat_map<std::string, bool, std::less<>>
      m_placeholders;  // name → waiting
  std::vector<CachedVideo> m_videos;
  std::shared_ptr<const sigil::image::ImageAsset> m_placeholder;
};

/** WHETHER EVERY ONE OF @p urls IS ALREADY ON THIS MACHINE, in the
 *  IO hub's network cache — the probe a sketch over fetched art writes
 *  into its `available()`.
 *
 *  Such a sketch keeps a procedural stand-in at every use site, so a
 *  cold cache still renders; but it renders the STAND-IN, and the plate
 *  the sketch is judged on is then not the picture its header describes.
 *  Two plates under one name is the one thing a byte-identity sweep
 *  cannot survive, so the sketch is unavailable BY NAME until the art is
 *  here: a machine that has fetched once stays available offline forever
 *  after, and one that never has stands down with the first missing URL
 *  as the reason, in @p why when it is given.
 *
 *  It asks the cache the IO hub's own way — the file a URL lands under,
 *  in the directory fetches persist to — and never the network, because
 *  a probe that fetched would make availability a function of the
 *  connection. @p cacheDir names another cache than the IO hub's default,
 *  which is what a test hands it. */
[[nodiscard]] bool requireCached(std::initializer_list<std::string_view> urls,
                                 std::string* why,
                                 const std::filesystem::path& cacheDir = {});

}  // namespace sigil::sketch
