#pragma once

/** @file
 * The BYTE SINK half of the resource vocabulary: where bytes go, told
 * the same way a `ByteSource` says where they come from. A sink answers
 * a URI and a run of bytes with a verdict; nothing here knows what a
 * byte means, which is why an encoder can hand its result to a hub, a
 * fixture directory or a pack writer without naming any of them.
 *
 * `writeBytes` is the local-filesystem end of it, the one place in this
 * tree where a path and a run of bytes become a file.
 *
 * Standard library only, header only.
 */

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

#include "sigilloader/source/Source.h"

namespace sigil::loader {

/** Writes @p size bytes at @p bytes to @p path, creating the
 *  directories above it. True only when every byte reached the file and
 *  the stream closed clean, so a half-written file reads as a failure
 *  rather than as a shorter resource. A zero-length write still makes
 *  the file: emptiness is a value a resource may have. */
inline bool writeBytes(const std::filesystem::path& path, const void* bytes,
                       size_t size) {
  if (size > 0 && bytes == nullptr) return false;
  if (path.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  if (size > 0)
    stream.write(static_cast<const char*>(bytes), (std::streamsize)size);
  stream.close();
  return stream.good();
}

/** The `Bytes` spelling of the same write. */
inline bool writeBytes(const std::filesystem::path& path, const Bytes& bytes) {
  return writeBytes(path, bytes.bytes.data(), bytes.bytes.size());
}

/** Anything that stores bytes under a URI: false when the URI cannot be
 *  written to. The mirror of `ByteSource`, so code that produces a
 *  resource can be written against a hub, a scratch directory or a test
 *  double without knowing which it has. */
template <typename S>
concept ByteSink =
    requires(S& sink, std::string_view uri, const void* bytes, size_t size) {
      { sink.write(uri, bytes, size) } -> std::convertible_to<bool>;
    };

}  // namespace sigil::loader
