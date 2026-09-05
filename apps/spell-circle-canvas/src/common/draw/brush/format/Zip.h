#pragma once

/** @file
 * A zip archive read out of memory, whole. Private to the brush formats:
 * two of the three they read are zips, and neither is large enough to be
 * worth streaming.
 */

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace sigil::draw::brush::format {

/** One file out of an archive, already decompressed. */
struct ZipEntry {
  std::string name;
  std::vector<std::byte> bytes;
};

/** Whether @p archive begins with a local file header. */
[[nodiscard]] bool isZip(std::span<const std::byte> archive);

/** Every regular file in @p archive, in the order the archive lists
 *  them; empty when the bytes are not a readable archive. Directories
 *  are left out — a name is a path, and what a reader wants is the
 *  files under it. */
[[nodiscard]] std::vector<ZipEntry> readZip(std::span<const std::byte> archive);

}  // namespace sigil::draw::brush::format
