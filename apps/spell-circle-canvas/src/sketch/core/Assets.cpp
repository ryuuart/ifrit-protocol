#include "sigilsketch/core/Assets.h"

#include <sigildata/read/Read.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilio/advanced/Network.h>
#include <sigilio/hub/Network.h>
#include <sigilio/transport/Transport.h>
#include <sigilio/advanced/Places.h>

#include <span>
#include <string>
#include <string_view>

namespace sigil::sketch {

namespace {

/** The hub a host's assets stand on. An empty root mounts nothing: a
 *  name under it is a placeholder, not a file found wherever the process
 *  happened to be started. A sketch's own files stand beside it:
 *  `sketch://<key>/name` is the file `name` in the directory the sketch's
 *  entry stands in, which for a directory sketch is its own and for a
 *  bare file is the folder the sketches share. A sketch that carries data
 *  of its own is a directory. */
io::HubOptions hubOptions(const std::filesystem::path& root,
                          const std::filesystem::path& sketches) {
  io::HubOptions options;
  if (!root.empty()) options.mounts.emplace("res://", root);
  if (!sketches.empty()) options.mounts.emplace("sketch://", sketches);
  return options;
}

}  // namespace

Assets::Assets(std::filesystem::path root, std::filesystem::path sketches)
    : m_root(std::move(root)),
      m_sketches(std::move(sketches)),
      m_hub(hubOptions(m_root, m_sketches)) {
  // An image is a resource SigilMedia says the meaning of: with its
  // decoders on, hub().load<media::Image>(uri) answers — stills,
  // animations and vector sources, with the library's own ImageOptions
  // when a sketch names a size or a layer — load<media::Channels>() the
  // float planes of the same file, and load<media::Video>() a clip. The
  // hub finds each by the name the meaning declares, so a sketch
  // compiled and loaded while the host runs asks the same door.
  sigil::media::registerDecoders(m_hub);
  // A data file is a resource like an image is: with the decoders on,
  // hub().load<Table>("sketch://<key>/data/x.csv") answers, cached and reloaded
  // by the same machinery, and a sketch carries no literal table. A
  // database file answers the same way, opened in place.
  sigil::data::registerDecoders(m_hub);
  // A resource that keeps ARRIVING is a feed, and a sketch opens one
  // through this same hub: with the transports registered,
  // hub().listen("udp://:27020") binds the port and every datagram that
  // reaches it arrives in that feed. A URI replay() names is played
  // back from its file instead, through no transport at all.
  sigil::io::registerTransports(m_hub);
}

void Assets::mountSketch(std::string_view key,
                         std::filesystem::path directory) {
  io::mount(m_hub, "sketch://" + std::string(key) + "/", std::move(directory));
}

bool Assets::poll() {
  // A file any typed ask found missing or unreadable is the hub's to
  // watch, as a loaded one is: its poll says when it appears or changes.
  return io::poll(m_hub);
}

bool requireCached(std::span<const std::string_view> urls, std::string* why,
                   const std::filesystem::path& cacheDirectory) {
  for (std::string_view url : urls) {
    // A present but empty resource cannot supply the sketch's art.
    const auto bytes = sigil::io::NetworkCache(cacheDirectory).byteSize(url);
    if (bytes && *bytes > 0) continue;
    if (why)
      *why = "not in the IO hub's network cache on this machine \xe2\x80\x94 " +
             std::string(url);
    return false;
  }
  return true;
}

bool requireCached(std::initializer_list<std::string_view> urls,
                   std::string* why,
                   const std::filesystem::path& cacheDirectory) {
  return requireCached(std::span(urls.begin(), urls.size()), why,
                       cacheDirectory);
}

}  // namespace sigil::sketch
