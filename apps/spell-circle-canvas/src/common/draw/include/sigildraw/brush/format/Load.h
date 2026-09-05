#pragma once

/** @file
 * Brushes as resources: the native format, and the one decoder that
 * answers every form of it.
 *
 * The brush library never opens a file. Everything here takes bytes,
 * from a hub, a fixture or a caller's own array, and the images inside
 * them are decoded by SigilImage.
 */

#include <sigildraw/brush/Tool.h>
#include <sigilio/source/Source.h>

#include <optional>
#include <string>
#include <string_view>

namespace sigil::draw::brush::format {

/** The name a native brush's description carries inside a directory or
 *  an archive, beside `shape.png` and the optional `grain.png`. */
inline constexpr std::string_view kDescriptionName = "brush.json";
inline constexpr std::string_view kShapeName = "shape.png";
inline constexpr std::string_view kGrainName = "grain.png";

/** A brush built from its three parts, any of which may be absent: the
 *  JSON description, the shape artwork and the grain texture. A missing
 *  description leaves every number at the library's default, so a bare
 *  pair of images is already a brush. Null when nothing readable
 *  arrived. */
[[nodiscard]] std::optional<Tool> assembleBrush(
    std::span<const std::byte> description, std::span<const std::byte> shape,
    std::span<const std::byte> grain);

/** The brush one run of bytes is, whatever form it takes: a native
 *  archive or bare description, a Photoshop `.abr` (its first sampled
 *  brush) or a Procreate `.brush`. `hint` is the resource's name, used
 *  only to sharpen the sniff — the bytes decide. Null when they are none
 *  of those. */
[[nodiscard]] std::optional<Tool> decodeBrush(const io::Bytes& bytes,
                                              std::string_view hint = {});

/** The description a native brush directory holds, written from @p tool.
 *  The images are not in it: a directory keeps them beside this text,
 *  under `shape.png` and `grain.png`, and whoever writes the directory
 *  encodes them. */
[[nodiscard]] std::string encodeBrush(const Tool& tool);

/** The decoder to register with a hub, so `load<Tool>()` answers:
 *
 *      hub.registerDecoder<brush::Tool>(brush::format::BrushDecoder{});
 *      auto ink = hub.load<brush::Tool>("res://brushes/ink.sigilbrush");
 *
 *  One decoder answers for every form, because a hub registers one
 *  decoder per type and a brush is one type however it was authored. */
struct BrushDecoder {
  [[nodiscard]] std::optional<Tool> decode(const io::Bytes& bytes,
                                           std::string_view hint) const {
    return decodeBrush(bytes, hint);
  }
};

/** The brush at @p uri, read through any byte source.
 *
 *  The native format is a DIRECTORY, `<name>.sigilbrush/`, holding
 *  `brush.json`, `shape.png` and an optional `grain.png` — so the
 *  artwork stays an image a painting program can open and edit in
 *  place. A directory has no bytes of its own, which is why loading one
 *  goes through a source rather than through a decoder: the three parts
 *  are three ordinary resources under it. Anything that is one file —
 *  a packed archive, an `.abr`, a `.brush` — is fetched whole and
 *  handed to `decodeBrush`, which is also what a hub's registered
 *  decoder runs. */
template <io::ByteSource S>
[[nodiscard]] std::optional<Tool> loadBrush(S& source, std::string_view uri) {
  std::string base(uri);
  while (!base.empty() && base.back() == '/') base.pop_back();

  const std::shared_ptr<const io::Bytes> description =
      source.fetch(base + "/" + std::string(kDescriptionName));
  const std::shared_ptr<const io::Bytes> shape =
      source.fetch(base + "/" + std::string(kShapeName));
  if (description || shape) {
    const std::shared_ptr<const io::Bytes> grain =
        source.fetch(base + "/" + std::string(kGrainName));
    static constexpr std::span<const std::byte> kNothing;
    return assembleBrush(description ? std::span(description->bytes) : kNothing,
                         shape ? std::span(shape->bytes) : kNothing,
                         grain ? std::span(grain->bytes) : kNothing);
  }

  if (const std::shared_ptr<const io::Bytes> packed = source.fetch(uri))
    return decodeBrush(*packed, uri);
  return std::nullopt;
}

}  // namespace sigil::draw::brush::format
