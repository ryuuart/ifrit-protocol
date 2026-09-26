#pragma once

/** @file
 * @ingroup io-hub
 * HOW A HUB LEARNS WHAT BYTES MEAN: the decoder a library registers so
 * `hub.load<T>()` can answer, and the probe that reads what a resource
 * is without decoding it. The `Decoder`, `Probable` and `Configurable`
 * concepts and `LoadOptions` are the source vocabulary these stand on.
 */

#include <sigilcore/callable/Callable.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>

#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"

namespace sigil::io {

/** WHERE A RESOURCE'S BYTES ARE AND HOW MANY OF THEM THERE ARE — the
 *  whole of what a hub can say about a resource without deciding what
 *  its bytes mean, asked as `probe<ResourceInfo>()`. What they mean is
 *  `probe<T>()` for another T, answered by the library that owns T. */
struct ResourceInfo {
  std::uintmax_t byteSize = 0;
  /** The local file the bytes were read from — the cache file for a
   *  network URI — or empty when they came from no file. It is also
   *  the name a prober takes as its format hint. */
  std::filesystem::path path;
};

/** How a ResourceInfo is probed: the count of the bytes and the file
 *  they came from, which is the hint every prober is handed. This is
 *  what makes it `Probable`, so the hub asks for it as it asks for any
 *  other meaning. */
inline std::optional<ResourceInfo> probeResource(
    std::type_identity<ResourceInfo>, std::span<const std::byte> bytes,
    const std::filesystem::path& file) {
  return ResourceInfo{bytes.size(), file};
}

namespace detail {
/** What a decoder for T is called with: the bytes, the resource's name
 *  as a hint, and — for a Configurable T — the options the load asked
 *  for. Every parameter after the bytes is offered, never demanded. */
template <typename T>
struct DecoderCall {
  using type = std::optional<T>(const Bytes&, std::string_view hint);
};
/** The same for a T loaded with options, which are offered third. */
template <Configurable T>
struct DecoderCall<T> {
  using type = std::optional<T>(const Bytes&, std::string_view hint,
                                const LoadOptions<T>& options);
};

/** Re-decodes bytes into a type-erased value; null on failure. */
using Redecode = std::function<std::shared_ptr<const void>(
    const Bytes&, const std::filesystem::path&)>;
/** A registered decoder with a load's options bound into it. */
using Configure = std::function<Redecode(std::shared_ptr<const void>)>;

/** Registers @p decode for @p type on @p hub at its defaults, and — for
 *  a type loaded with options — @p configure, which binds other options
 *  in. */
void setDecoder(Hub& hub, std::type_index type, Redecode decode,
                Configure configure);

/** The one read a probe makes: the bytes, uncached, with @p info filled
 *  in from them. Null when the URI cannot be served. */
std::shared_ptr<const Bytes> probeRead(const Hub& hub, std::string_view uri,
                                       ResourceInfo& info);
}  // namespace detail

/** Registers on @p hub how a T is decoded from bytes, so load<T>() can
 *  answer. `hint` is the resource's local path when it has one, and is
 *  OFFERED: a decoder reading the bytes alone takes
 *  `[](const Bytes& bytes) {…}`. For a Configurable T the options a load
 *  asked for are offered third, T's defaults when it named none. A hub
 *  registers nothing itself: the library that owns T calls this, as
 *  SigilMedia's and SigilData's `registerDecoders(hub)` do.
 *  @trap Replacing a decoder leaves a view already decoded holding its
 *  value and the decoder that made it, which poll() re-runs. */
template <typename T>
void registerDecoder(
    Hub& hub,
    core::Callable<typename detail::DecoderCall<T>::type> decode) {
  if constexpr (Configurable<T>) {
    using Options = LoadOptions<T>;
    const auto configure =
        [decode](std::shared_ptr<const void> options) -> detail::Redecode {
      return [decode, options = std::move(options)](
                 const Bytes& bytes, const std::filesystem::path& path)
                 -> std::shared_ptr<const void> {
        auto value = decode(bytes, path.native(),
                            *static_cast<const Options*>(options.get()));
        if (!value) return nullptr;
        return std::make_shared<const T>(std::move(*value));
      };
    };
    detail::setDecoder(hub, std::type_index(typeid(T)),
                       configure(std::make_shared<const Options>()),
                       configure);
  } else {
    detail::setDecoder(
        hub, std::type_index(typeid(T)),
        [decode = std::move(decode)](const Bytes& bytes,
                                     const std::filesystem::path& path)
            -> std::shared_ptr<const void> {
          auto value = decode(bytes, path.native());
          if (!value) return nullptr;
          return std::make_shared<const T>(std::move(*value));
        },
        {});
  }
}

/** The same, from any object satisfying the Decoder concept — which
 *  reads the hint or the bytes alone, as the callable form does. */
template <typename T, Decoder<T> D>
void registerDecoder(Hub& hub, D decoder) {
  registerDecoder<T>(hub, [decoder = std::move(decoder)](
                              const Bytes& bytes, std::string_view hint) {
    return core::callPrefix(
        [&decoder](const Bytes& resourceBytes, std::string_view resourceHint) {
          if constexpr (requires {
                          decoder.decode(resourceBytes, resourceHint);
                        })
            return decoder.decode(resourceBytes, resourceHint);
          else
            return decoder.decode(resourceBytes);
        },
        bytes, hint);
  });
}

/** WHAT THE BYTES AT @p uri ARE, WITHOUT DECODING THEM: `ResourceInfo`
 *  for how many bytes and from which file, dimensions and layers for an
 *  image, and whatever the next kind of meaning turns out to need. The
 *  answer comes from T's own library through the `Probable` seam, so the
 *  hub carries no opinion about any format; nothing when the URI cannot
 *  be served or T's library cannot read it.
 *  @trap Neither cheap nor side-effect-free — every call performs a full
 *  read and caches nothing, which for a network URI is a round trip and
 *  a write into the disk cache directory. */
template <Probable T>
std::optional<T> probe(const Hub& hub, std::string_view uri) {
  ResourceInfo info;
  const std::shared_ptr<const Bytes> bytes = detail::probeRead(hub, uri, info);
  if (!bytes) return std::nullopt;
  return probeResource(std::type_identity<T>{}, bytes->span(), info.path);
}

}  // namespace sigil::io
