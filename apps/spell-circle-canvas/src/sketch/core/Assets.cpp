#include "sigilsketch/core/Assets.h"

#include <sigildata/read/Read.h>
#include <sigilio/advanced/Network.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Network.h>
#include <sigilio/transport/Transport.h>
#include <sigilmedia/advanced/Resource.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/Resource.h>

#include <algorithm>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace sigil::sketch {

/** The faces a hub has read, by the URI each was read from, and the font
 *  contexts adopting them with the thread each belongs to. Shared by the
 *  assets, the hub's face decoder and every adoption standing, so none of
 *  them outlives what it reads. */
struct Assets::FaceAdoption::Faces {
  struct Adopter {
    weave::FontContext* fonts = nullptr;
    std::thread::id thread;
    int holds = 0;
  };
  std::mutex guard;
  std::vector<weave::AdoptedFace> read;
  std::vector<Adopter> adopters;
  /** Each mount's URI prefix and directory, so a path the decoder is
   *  handed is named by the URI a sketch would spell for it. */
  std::vector<std::pair<std::string, std::filesystem::path>> mounts;

  void mount(std::string prefix, const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::path resolved =
        std::filesystem::weakly_canonical(directory, error);
    if (error) resolved = directory.lexically_normal();
    const std::lock_guard lock(guard);
    mounts.emplace_back(std::move(prefix), std::move(resolved));
  }

  /** The URI @p path is read through: the mount holding it most closely,
   *  else the path itself. Asked under the lock. */
  std::string uriOf(std::string_view path) const {
    std::error_code error;
    std::filesystem::path file =
        std::filesystem::weakly_canonical(std::filesystem::path(path), error);
    if (error) file = std::filesystem::path(path).lexically_normal();
    std::string best(path);
    size_t bestDepth = 0;
    for (const auto& [prefix, directory] : mounts) {
      const std::filesystem::path relative = file.lexically_relative(directory);
      if (relative.empty() || *relative.begin() == "..") continue;
      const size_t depth = directory.native().size();
      if (depth < bestDepth) continue;
      bestDepth = depth;
      best = prefix + relative.generic_string();
    }
    return best;
  }

  /** A face the decoder read from @p path: kept under its URI, replacing
   *  what that URI held, and adopted by every context on this thread. */
  void arrived(const weave::Face& face, std::string_view path) {
    std::vector<weave::FontContext*> here;
    std::string uri;
    {
      const std::lock_guard lock(guard);
      uri = uriOf(path);
      const auto held =
          std::ranges::find(read, uri, &weave::AdoptedFace::origin);
      if (held != read.end())
        held->face = face;
      else
        read.push_back({face, uri});
      for (const Adopter& adopter : adopters)
        if (adopter.thread == std::this_thread::get_id())
          here.push_back(adopter.fonts);
    }
    for (weave::FontContext* fonts : here) fonts->adopt(face, uri);
  }
};

Assets::FaceAdoption::FaceAdoption(std::shared_ptr<Faces> faces,
                                   weave::FontContext* fonts)
    : m_faces(std::move(faces)), m_fonts(fonts) {}

Assets::FaceAdoption::FaceAdoption(FaceAdoption&& other) noexcept
    : m_faces(std::move(other.m_faces)),
      m_fonts(std::exchange(other.m_fonts, nullptr)) {}

Assets::FaceAdoption& Assets::FaceAdoption::operator=(
    FaceAdoption&& other) noexcept {
  if (this == &other) return *this;
  FaceAdoption ended(std::move(*this));
  m_faces = std::move(other.m_faces);
  m_fonts = std::exchange(other.m_fonts, nullptr);
  return *this;
}

Assets::FaceAdoption::~FaceAdoption() {
  if (!m_faces || !m_fonts) return;
  const std::lock_guard lock(m_faces->guard);
  auto& adopters = m_faces->adopters;
  const auto standing =
      std::ranges::find(adopters, m_fonts, &Faces::Adopter::fonts);
  if (standing != adopters.end() && --standing->holds <= 0)
    adopters.erase(standing);
}

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
      m_faces(std::make_shared<FaceAdoption::Faces>()),
      m_hub(hubOptions(m_root, m_sketches)) {
  if (!m_root.empty()) m_faces->mount("res://", m_root);
  if (!m_sketches.empty()) m_faces->mount("sketch://", m_sketches);
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
  // A font file is a resource too: with Weave's face decoder on,
  // hub().load<weave::Face>("sketch://<key>/data/x.ttf") answers the face
  // it holds, cached, held and read again on an edit like any asset, and
  // a sketch in either language asks the same door for the same face.
  // Every face it reads is also handed to the font contexts the open
  // sessions adopt faces into, so the face's family answers by name.
  sigil::weave::ports::registerDecoders(
      m_hub,
      [faces = m_faces](const sigil::weave::Face& face, std::string_view path) {
        faces->arrived(face, path);
      });
  // A resource that keeps ARRIVING is a feed, and a sketch opens one
  // through this same hub: with the transports registered,
  // hub().listen("udp://:27020") binds the port and every datagram that
  // reaches it arrives in that feed. A URI replay() names is played
  // back from its file instead, through no transport at all.
  sigil::io::registerTransports(m_hub);
}

void Assets::mountSketch(std::string_view key,
                         std::filesystem::path directory) {
  const std::string prefix = "sketch://" + std::string(key) + "/";
  m_faces->mount(prefix, directory);
  io::mount(m_hub, prefix, std::move(directory));
}

Assets::FaceAdoption Assets::adoptFacesInto(weave::FontContext& fonts) {
  std::vector<weave::AdoptedFace> read;
  {
    const std::lock_guard lock(m_faces->guard);
    auto& adopters = m_faces->adopters;
    const auto standing = std::ranges::find(
        adopters, &fonts, &FaceAdoption::Faces::Adopter::fonts);
    if (standing != adopters.end()) {
      ++standing->holds;
      standing->thread = std::this_thread::get_id();
    } else {
      adopters.push_back({&fonts, std::this_thread::get_id(), 1});
    }
    read = m_faces->read;
  }
  // What the hub read before this context was adopting, at once.
  for (weave::AdoptedFace& face : read)
    fonts.adopt(std::move(face.face), std::move(face.origin));
  return FaceAdoption(m_faces, &fonts);
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
