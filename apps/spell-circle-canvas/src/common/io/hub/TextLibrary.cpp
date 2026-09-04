/** @file The synchronized text-library facade over a private resource hub. */

#include "sigilio/hub/TextLibrary.h"

#include <map>
#include <mutex>
#include <utility>

#include "sigilio/hub/Hub.h"

namespace sigil::io {

namespace {

struct TextStore {
  TextStore(std::string uriPrefix, std::filesystem::path root)
      : prefix(std::move(uriPrefix)) {
    hub.mount(prefix, std::move(root));
  }

  Hub hub;
  std::mutex mutex;
  std::string prefix;
};

std::shared_ptr<TextStore> storeFor(const std::string& prefix,
                                    const std::filesystem::path& root) {
  static std::mutex mutex;
  static std::map<std::string, std::weak_ptr<TextStore>, std::less<>> stores;
  std::string key = prefix;
  key += '\0';
  key += root.lexically_normal().generic_string();
  const std::lock_guard lock(mutex);
  if (const auto found = stores.find(key); found != stores.end())
    if (std::shared_ptr<TextStore> existing = found->second.lock())
      return existing;
  auto created = std::make_shared<TextStore>(prefix, root);
  stores[key] = created;
  return created;
}

}  // namespace

struct TextLibrary::Impl {
  Impl(std::string uriPrefix, const std::filesystem::path& root)
      : store(storeFor(uriPrefix, root)) {}

  std::shared_ptr<TextStore> store;
};

TextLibrary::TextLibrary(std::string uriPrefix, std::filesystem::path root)
    : m_impl(std::make_unique<Impl>(std::move(uriPrefix), std::move(root))) {}

TextLibrary::~TextLibrary() = default;
TextLibrary::TextLibrary(TextLibrary&&) noexcept = default;
TextLibrary& TextLibrary::operator=(TextLibrary&&) noexcept = default;

std::optional<std::string> TextLibrary::text(std::string_view uri) {
  TextStore& store = *m_impl->store;
  const std::lock_guard lock(store.mutex);
  if (!uri.starts_with(store.prefix)) return std::nullopt;
  return store.hub.text(uri);
}

size_t TextLibrary::preload(std::span<const std::string_view> uris) {
  TextStore& store = *m_impl->store;
  const std::lock_guard lock(store.mutex);
  for (std::string_view uri : uris)
    if (!uri.starts_with(store.prefix)) return 0;
  return store.hub.preload(uris);
}

size_t TextLibrary::preload() {
  TextStore& store = *m_impl->store;
  const std::lock_guard lock(store.mutex);
  return store.hub.preloadDirectory(store.prefix);
}

bool TextLibrary::poll() {
  TextStore& store = *m_impl->store;
  const std::lock_guard lock(store.mutex);
  return store.hub.poll();
}

}  // namespace sigil::io
