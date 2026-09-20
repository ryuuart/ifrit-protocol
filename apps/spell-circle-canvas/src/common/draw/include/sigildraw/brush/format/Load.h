#pragma once

/** @file
 * @ingroup draw-brush
 *
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
#include <span>
#include <string>
#include <string_view>

/** THE BRUSH FILES A PAINTING APPLICATION EXPORTS, read into this
 *  library's tool: the native directory or archive of a description plus
 *  its artwork, and the two vendor formats. What a vendor format carries
 *  and this library has no place for is dropped rather than
 *  approximated, so a loaded brush is the part of the original this
 *  library can actually paint with. */
namespace sigil::draw::brush::format {

/** The name a native brush's description carries inside a directory or
 *  an archive, beside `shape.png` and the optional `grain.png`. */
inline constexpr std::string_view kDescriptionName = "brush.json";
/** The shape artwork's name beside the description. */
inline constexpr std::string_view kShapeName = "shape.png";
/** The optional grain texture's name beside the description. */
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
 *  brush) or a Procreate `.brush`. The bytes decide; `hint` is the
 *  resource's name, and the one thing it settles is which reader an
 *  archive is offered to first, since a native pack and a Procreate
 *  brush are both zips. Null when the bytes are none of those. */
[[nodiscard]] std::optional<Tool> decodeBrush(std::span<const std::byte> bytes,
                                              std::string_view hint = {});

/** The description a native brush directory holds, written from
 *  @p tool: every value of it that is a number, a flag or a word, so a
 *  tool written and read back is the tool that was written.
 *  @trap What a description cannot hold is not in it — the two images,
 *  which sit beside this text, and the callables, which are code. */
[[nodiscard]] std::string encodeBrush(const Tool& tool);

/** The decoder to register with a hub, so a typed load answers. One
 *  decoder answers for every form, because a hub registers one decoder
 *  per type and a brush is one type however it was authored. */
struct BrushDecoder {
  [[nodiscard]] std::optional<Tool> decode(const io::Bytes& bytes,
                                           std::string_view hint) const {
    return decodeBrush(bytes.bytes, hint);
  }
};

/** The brush at @p uri, read through any byte source. The native
 *  format is a DIRECTORY holding a description and its two images, so
 *  the artwork stays an image a painting program can edit in place; a
 *  directory has no bytes of its own, which is why loading one goes
 *  through a source rather than through a decoder. Anything that is one
 *  file is fetched whole and handed to `decodeBrush`. */
template <io::ByteSource S>
[[nodiscard]] std::optional<Tool> loadBrush(S& source, std::string_view uri) {
  // One file first: a brush that is one resource costs one fetch, and a
  // directory has no bytes of its own, so it falls through to its parts.
  if (const std::shared_ptr<const io::Bytes> packed = source.fetch(uri))
    if (std::optional<Tool> tool = decodeBrush(packed->bytes, uri)) return tool;

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
  return std::nullopt;
}

}  // namespace sigil::draw::brush::format
