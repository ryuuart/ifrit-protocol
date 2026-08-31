#pragma once

/** @file
 * The BYTE SOURCE foundation: the one vocabulary every resource path in
 * this library and its consumers speaks. A source answers a URI with
 * bytes; a decoder turns bytes into a value. Nothing here knows what a
 * URI resolves to or what a byte means — that is what lets a decoder be
 * written against a test fixture and run unchanged behind a hub, and a
 * hub be replaced by a pack file or an in-memory table without any
 * decoder noticing.
 *
 * Standard library only, header only.
 */

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::loader {

/** Raw bytes of a resource. */
struct Bytes {
  std::vector<std::byte> bytes;

  std::string_view asText() const {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
  }
};

/** A second name for Bytes; the two are one type. */
using Blob = Bytes;

/** Anything that answers a URI with bytes: null when the URI cannot be
 *  served. The result is shared and immutable, so a source may hand out
 *  a cached copy and a caller may keep it for as long as it likes. */
template <typename S>
concept ByteSource = requires(S& source, std::string_view uri) {
  { source.fetch(uri) } -> std::convertible_to<std::shared_ptr<const Bytes>>;
};

/** A ByteSource that can also say WHERE a URI's bytes live on the local
 *  filesystem — empty when the URI does not map to a file. Optional: a
 *  network or in-memory source has no path to give. */
template <typename S>
concept ResolvingByteSource =
    ByteSource<S> && requires(const S& source, std::string_view uri) {
      { source.resolve(uri) } -> std::convertible_to<std::filesystem::path>;
    };

/** Turns bytes into a T, or nothing when the bytes are not one. `hint`
 *  is the resource's name (a path or URI); a decoder may use its
 *  extension to sharpen format detection but must never REQUIRE it —
 *  bytes arriving from memory carry no name. */
template <typename D, typename T>
concept Decoder =
    requires(const D& decoder, const Bytes& bytes, std::string_view hint) {
      { decoder.decode(bytes, hint) } -> std::same_as<std::optional<T>>;
    };

/** A ByteSource VALUE holding any ByteSource: the type-erased form for
 *  code that stores a source rather than being templated on one.
 *
 *  Built from a reference, it borrows — the source must outlive it.
 *  Built from a shared_ptr, it shares ownership. `resolve()` answers an
 *  empty path for a source that has no resolve of its own. */
class AnyByteSource {
 public:
  AnyByteSource() = default;

  template <ByteSource S>
  explicit AnyByteSource(S& source)
      : m_fetch([&source](std::string_view uri) {
          return std::shared_ptr<const Bytes>(source.fetch(uri));
        }) {
    if constexpr (ResolvingByteSource<S>)
      m_resolve = [&source](std::string_view uri) {
        return std::filesystem::path(source.resolve(uri));
      };
  }

  template <ByteSource S>
  explicit AnyByteSource(const std::shared_ptr<S>& source)
      : m_owner(source), m_fetch([source](std::string_view uri) {
          return std::shared_ptr<const Bytes>(source->fetch(uri));
        }) {
    if constexpr (ResolvingByteSource<S>)
      m_resolve = [source](std::string_view uri) {
        return std::filesystem::path(source->resolve(uri));
      };
  }

  explicit operator bool() const { return static_cast<bool>(m_fetch); }

  std::shared_ptr<const Bytes> fetch(std::string_view uri) {
    return m_fetch ? m_fetch(uri) : nullptr;
  }

  std::filesystem::path resolve(std::string_view uri) const {
    return m_resolve ? m_resolve(uri) : std::filesystem::path{};
  }

 private:
  std::shared_ptr<void> m_owner;
  std::function<std::shared_ptr<const Bytes>(std::string_view)> m_fetch;
  std::function<std::filesystem::path(std::string_view)> m_resolve;
};

static_assert(ResolvingByteSource<AnyByteSource>);

}  // namespace sigil::loader
