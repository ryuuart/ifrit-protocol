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

namespace sigil::data {
class Table;
class Database;
class Json;
}  // namespace sigil::data

namespace sigil::sketch {

/** THE FILES A SKETCH REACHES FOR that it did not generate.
 *
 *  The demo assets root mounts at `res://` and the sketches folder at
 *  `sketch://`, under which a sketch's own files stand. `image()` keeps the
 *  forgiving contract a live-edited file wants — a magenta placeholder
 *  stands in for a missing or undecodable file and heals the moment one
 *  appears — and `hub()` opens the full resource surface (text, blobs,
 *  metadata probes, EXR layers, PSD) without the sketch ever touching
 *  the filesystem. */
class Assets {
 public:
  /** @p root mounts at `res://`; @p sketches, the directory the sketch
   *  sources stand in, mounts at `sketch://`, under which a sketch's own
   *  files are `sketch://<key>/name`. */
  explicit Assets(std::filesystem::path root,
                  std::filesystem::path sketches = {});
  /** Mounts @p directory as the files of the sketch keyed @p key — what a
   *  workspace sketch opened by path needs, since its files stand beside
   *  that path and not under the sketches the build compiled. */
  void mountSketch(std::string_view key, std::filesystem::path directory);

  /** The image at "res://<name>", cached by the hub. Never null: a
   *  missing or undecodable file yields the placeholder until it
   *  becomes loadable. */
  std::shared_ptr<const sigil::image::ImageAsset> image(std::string_view name);

  /** The video at "res://<name>", decoded on demand and cached with the
   *  requested device policy. Null when the resource is missing or cannot be
   *  decoded. The clip keeps only a small presentation-frame cache. */
  std::shared_ptr<sigil::video::Video> video(
      std::string_view name, const sigil::video::DecodeOptions& options = {});

  /** The table at "res://<name>", decoded from whichever rectangular
   *  format the file is in, cached and reloaded by the hub. Null where
   *  there is no such resource or no decoder answers for it.
   *
   *  A TYPED LOAD IS SPELLED HERE and not at the call site, because a
   *  hot-reloaded sketch is its own image: the hub keys a decoder by
   *  the asking image's type, and a sketch dylib's `data::Table` is not
   *  the host's. Asked through this one function, a sketch reads its
   *  data whether it was compiled in or swapped in while running. */
  std::shared_ptr<const sigil::data::Table> table(std::string_view name);
  /** A SQL store — a `.sqlite`, `.sqlite3`, `.db` or `.duckdb` file —
   *  opened in place, cached and reopened when the file changes. Null
   *  until it loads or when it is not one. */
  std::shared_ptr<const sigil::data::Database> database(std::string_view name);
  /** THE DOCUMENT AT @p name — a `.json` file read whole as one nested
   *  value, cached and reloaded by the hub, so a sketch's words and
   *  settings stand in a file beside it and an edit to that file re-runs
   *  setup without a rebuild. Null when there is no such file or it is
   *  not JSON; a key that is not in it reads as a null value, so a sketch
   *  states the default it wants where it reads. */
  std::shared_ptr<const sigil::data::Json> json(std::string_view name);

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
  std::filesystem::path m_sketches;
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
 *  connection. @p cacheDirectory names another cache than the IO hub's default,
 *  which is what a test hands it. */
[[nodiscard]] bool requireCached(
    std::initializer_list<std::string_view> urls, std::string* why,
    const std::filesystem::path& cacheDirectory = {});

}  // namespace sigil::sketch
