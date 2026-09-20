#pragma once

/** @file
 * @ingroup io-source
 * The BYTE SOURCE foundation: the one vocabulary every resource path in
 * this library and its consumers speaks. A source answers a URI with
 * bytes; a decoder turns bytes into a value. Nothing here knows what a
 * URI resolves to or what a byte means. `readBytes` is the
 * local-filesystem end of it, the one place in this tree where a file
 * becomes a run of bytes, as `writeBytes` beside the sink concept is
 * the one place a run of bytes becomes a file.
 *
 * Standard library only, header only.
 */

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/** RESOURCE ACCESS: the bytes of a thing, and the doors they come
 *  through. A URI resolved against a mount table, fetched, cached,
 *  decoded into the type asked for and reloaded when it changes; the
 *  byte source seam every one of those doors is written to; sinks for
 *  bytes going out; feeds for bytes that keep arriving; and native
 *  frame sharing with another application. Reach for it whenever code
 *  needs something that is not already in memory. What those bytes MEAN
 *  is never decided here: a decoder registered by the library that owns
 *  the format is what turns them into a value. */
namespace sigil::io {

/** Raw bytes of a resource. */
struct Bytes {
  std::vector<std::byte> bytes;

  /** The same bytes read as text, with no copy and no validation. */
  std::string_view asText() const {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
  }
};


/** Every byte of @p path, or nothing when it cannot be read whole. A
 *  file that shrank between the size and the read, or that could not be
 *  opened at all, answers nothing rather than the part that arrived: a
 *  short read is not a shorter resource. An empty file answers empty
 *  bytes, emptiness being a value a resource may have. */
inline std::optional<Bytes> readBytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (!stream) return std::nullopt;
  const std::streamoff size = stream.tellg();
  if (size < 0) return std::nullopt;
  Bytes out;
  out.bytes.resize((size_t)size);
  stream.seekg(0);
  if (size > 0)
    stream.read(reinterpret_cast<char*>(out.bytes.data()),
                (std::streamsize)size);
  if (!stream) return std::nullopt;
  return out;
}

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
 *  is the resource's name, a path or a URI, and is OFFERED rather than
 *  demanded: a decoder that reads the bytes alone spells
 *  `decode(bytes)` and is as good a decoder as one taking both.
 *  @trap A decoder must never REQUIRE the hint — bytes arriving from
 *  memory carry no name. */
template <typename D, typename T>
concept Decoder =
    requires(const D& decoder, const Bytes& bytes, std::string_view hint) {
      { decoder.decode(bytes, hint) } -> std::same_as<std::optional<T>>;
    } || requires(const D& decoder, const Bytes& bytes) {
      { decoder.decode(bytes) } -> std::same_as<std::optional<T>>;
    };

/** WHAT A KIND OF MEANING IS PROBED WITH: a free function found by
 *  argument-dependent lookup in T's own namespace, answering what @p
 *  bytes are without decoding them. The library that owns the meaning
 *  declares it against nothing from here but the standard library, so a
 *  byte source answers `probe<T>()` for a T it has never heard of.
 *  @trap @p hint is the resource's name, which a prober may use to
 *  sharpen format detection and must never require. */
template <typename T>
concept Probable = requires(std::span<const std::byte> bytes,
                            const std::filesystem::path& hint) {
  {
    probeResource(std::type_identity<T>{}, bytes, hint)
  } -> std::same_as<std::optional<T>>;
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

  /** Borrows @p source, which must outlive this value. */
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

  /** Shares ownership of @p source, so this value keeps it alive. */
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

  /** Whether a source is held at all. */
  explicit operator bool() const { return static_cast<bool>(m_fetch); }

  /** The bytes of @p uri from the held source; null when it holds none
   *  or the source answers none. */
  std::shared_ptr<const Bytes> fetch(std::string_view uri) {
    return m_fetch ? m_fetch(uri) : nullptr;
  }

  /** The file @p uri stands for, or an empty path when the held source
   *  resolves nothing. */
  std::filesystem::path resolve(std::string_view uri) const {
    return m_resolve ? m_resolve(uri) : std::filesystem::path{};
  }

 private:
  std::shared_ptr<void> m_owner;
  std::function<std::shared_ptr<const Bytes>(std::string_view)> m_fetch;
  std::function<std::filesystem::path(std::string_view)> m_resolve;
};

static_assert(ResolvingByteSource<AnyByteSource>);

}  // namespace sigil::io
