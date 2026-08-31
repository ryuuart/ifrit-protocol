#pragma once

/** @file
 * What the codec's translation units share and nothing outside the
 * directory sees: the chores every reader finishes a Part with, the
 * text and extension helpers the dispatcher and the readers both use,
 * and one entry point per format so the dispatcher in Model.cpp can
 * route bytes without knowing how any format is parsed.
 *
 * Each format's reader is a translation unit of its own; a reader that
 * does not need a library the others use keeps that library out of its
 * compile.
 */

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "sigilgeometry/mesh/codec/Model.h"

namespace sigil::geometry::mesh::codec::decode::detail {

constexpr glm::vec4 kWhite = {1, 1, 1, 1};

/** The bytes viewed as text — no copy, no validation. */
std::string_view asText(const void* bytes, size_t size);

/** The lowercase extension of a path or URL path, "" when it has none
 *  (a dot inside a directory name does not count). */
std::string lowerExtension(std::string_view pathHint);

/** Every importer's closing chores: lanes sized to positions (the Mesh
 *  contract), normals derived from the triangles when the file carried
 *  none. */
void finishPart(Part& part, bool hasNormals);

/** Wavefront OBJ text; .mtl libraries reach through @p resolve. */
std::optional<Model> importObj(std::string_view text, const Resolver& resolve);

/** glTF 2.0 as JSON or a GLB container; external buffers and images
 *  reach through @p resolve, @p pathHint only names the source. */
std::optional<Model> importGltf(const void* bytes, size_t size,
                                std::string_view pathHint,
                                const Resolver& resolve);

/** STL, binary or ascii — the bytes decide. */
std::optional<Model> importStl(const std::byte* bytes, size_t size);
bool looksLikeBinaryStl(const std::byte* bytes, size_t size);
bool looksLikeAsciiStl(std::string_view text);

/** PLY, ascii or binary little-endian — the header decides. */
std::optional<Model> importPly(const std::byte* bytes, size_t size);
bool looksLikePly(std::string_view text);

/** Houdini's JSON .geo. */
std::optional<Model> importHoudiniGeo(std::string_view text);
bool looksLikeHoudiniGeo(std::string_view text);

}  // namespace sigil::geometry::mesh::codec::decode::detail
