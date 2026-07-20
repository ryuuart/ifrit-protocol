#include "sigilloader/Loader.h"

#include <fstream>

namespace sigil::loader {

namespace {

std::shared_ptr<const Blob> readFile(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream)
    return nullptr;
  const std::streamsize size = stream.tellg();
  stream.seekg(0);
  auto blob = std::make_shared<Blob>();
  blob->bytes.resize((size_t)size);
  if (!stream.read(reinterpret_cast<char *>(blob->bytes.data()), size))
    return nullptr;
  return blob;
}

} // namespace

void Hub::mount(std::string prefix, std::filesystem::path dir) {
  for (auto &[existing, path] : m_mounts)
    if (existing == prefix) {
      path = std::move(dir);
      return;
    }
  m_mounts.emplace_back(std::move(prefix), std::move(dir));
}

std::filesystem::path Hub::resolve(std::string_view uri) const {
  const std::pair<std::string, std::filesystem::path> *best = nullptr;
  for (const auto &mountPair : m_mounts)
    if (uri.starts_with(mountPair.first) &&
        (!best || mountPair.first.size() > best->first.size()))
      best = &mountPair;
  if (best)
    return best->second / std::string(uri.substr(best->first.size()));
  return {};
}

std::string Hub::cacheKey(std::string_view uri,
                          const ImageOptions *options) {
  std::string key(uri);
  if (options && !options->layer.empty()) {
    key += "#layer=";
    key += options->layer;
  }
  return key;
}

std::shared_ptr<const Blob> Hub::blob(std::string_view uri) {
  const std::string key = cacheKey(uri, nullptr);
  if (auto it = m_entries.find(key); it != m_entries.end())
    return it->second.blob;
  std::filesystem::path path = resolve(uri);
  if (path.empty())
    path = std::string(uri); // no mount matched: try as plain path
  std::error_code ec;
  const auto mtime = std::filesystem::last_write_time(path, ec);
  auto bytes = ec ? nullptr : readFile(path);
  if (!bytes)
    return nullptr; // not cached: heals as soon as the file appears
  Entry entry;
  entry.blob = std::move(bytes);
  entry.mtime = mtime;
  return m_entries.emplace(key, std::move(entry)).first->second.blob;
}

std::optional<std::string> Hub::text(std::string_view uri) {
  auto bytes = blob(uri);
  if (!bytes)
    return std::nullopt;
  return std::string(bytes->asText());
}

std::shared_ptr<const sigil::image::ImageAsset>
Hub::image(std::string_view uri, const ImageOptions &options) {
  const std::string key = cacheKey(uri, &options);
  if (auto it = m_entries.find(key); it != m_entries.end())
    return it->second.image;
  std::filesystem::path path = resolve(uri);
  if (path.empty())
    path = std::string(uri);
  std::error_code ec;
  const auto mtime = std::filesystem::last_write_time(path, ec);
  auto bytes = ec ? nullptr : readFile(path);
  if (!bytes)
    return nullptr;
  auto decoded =
      decodeImage(bytes->bytes.data(), bytes->bytes.size(), options, path);
  if (!decoded)
    return nullptr;
  Entry entry;
  entry.image = std::make_shared<sigil::image::ImageAsset>(
      std::move(*decoded));
  entry.imageOptions = options;
  entry.mtime = mtime;
  return m_entries.emplace(key, std::move(entry)).first->second.image;
}

std::optional<ResourceInfo> Hub::probe(std::string_view uri) const {
  std::filesystem::path path = resolve(uri);
  if (path.empty())
    path = std::string(uri);
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec)
    return std::nullopt;
  auto bytes = readFile(path);
  if (!bytes)
    return std::nullopt;
  if (auto info = probeImage(bytes->bytes.data(), bytes->bytes.size(),
                             path)) {
    info->byteSize = size;
    return info;
  }
  ResourceInfo info;
  info.kind = ResourceInfo::Kind::Data;
  info.byteSize = size;
  return info;
}

bool Hub::reload(const std::string &key, Entry &entry) {
  // The key embeds the uri (and options); recover the uri part.
  const size_t hash = key.find('#');
  const std::string uri = key.substr(0, hash);
  std::filesystem::path path = resolve(uri);
  if (path.empty())
    path = uri;
  auto bytes = readFile(path);
  if (!bytes)
    return false;
  if (entry.image) {
    auto decoded = decodeImage(bytes->bytes.data(), bytes->bytes.size(),
                               entry.imageOptions, path);
    if (!decoded)
      return false;
    entry.image = std::make_shared<sigil::image::ImageAsset>(
        std::move(*decoded));
  } else {
    entry.blob = std::move(bytes);
  }
  return true;
}

bool Hub::poll() {
  bool changed = false;
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    const std::string uri =
        it->first.substr(0, it->first.find('#'));
    std::filesystem::path path = resolve(uri);
    if (path.empty())
      path = uri;
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path, ec);
    if (ec) {
      it = m_entries.erase(it); // vanished: next ask sees the truth
      changed = true;
      continue;
    }
    if (mtime != it->second.mtime && reload(it->first, it->second)) {
      it->second.mtime = mtime;
      changed = true;
    }
    ++it;
  }
  return changed;
}

} // namespace sigil::loader
