#pragma once

/** @file
 * @ingroup io-source
 * AN ARCHIVE READ AS A BYTE SOURCE: a zip held in memory, whole, whose
 * files answer to their names the way any other source's resources do.
 * What is here is access: names in, bytes out. What the bytes inside
 * MEAN is the decoding library's, exactly as it is for a file on disk.
 *
 * Whole, not streamed: an archive small enough to hold in memory is the
 * only kind this reads, and holding it is what lets every entry be
 * answered without seeking the source again.
 */

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sigilio/source/Source.h"

namespace sigil::io {

/** One file out of an archive, already decompressed. */
struct ArchiveEntry {
  /** The path the archive lists the file under, as it lists it. */
  std::string name;
  std::shared_ptr<const Bytes> bytes;
};

/** THE FILES INSIDE ONE ARCHIVE. Construction reads them all; `fetch`
 *  then answers a name with the bytes already in hand. A name is matched
 *  as the archive spells it, and directories are left out. */
class ArchiveSource {
 public:
  /** THE MOST ANY ONE ENTRY MAY DECOMPRESS TO, in bytes. An archive's
   *  directory is a CLAIM, read before a byte of the entry is, so an
   *  entry claiming more than this is left out rather than allocated
   *  for. */
  static constexpr size_t kEntryCeiling = 64u * 1024u * 1024u;

  /** An empty source: every name answers null. */
  ArchiveSource() = default;

  /** Reads every regular file out of @p archive. Empty when the bytes
   *  are not a readable archive.
   *  @silent an entry claiming more than @p entryCeiling bytes, or more
   *  than a thousand times the archive's own size: it is left out. */
  explicit ArchiveSource(std::span<const std::byte> archive,
                         size_t entryCeiling = kEntryCeiling);

  /** Whether @p archive begins with a local file header. */
  [[nodiscard]] static bool isArchive(std::span<const std::byte> archive);

  /** The entry @p uri names, or null. */
  [[nodiscard]] std::shared_ptr<const Bytes> fetch(std::string_view uri) const;

  /** Every entry, in the order the archive lists them. */
  [[nodiscard]] std::span<const ArchiveEntry> entries() const {
    return m_entries;
  }
  /** Whether the archive lists no entry. */
  [[nodiscard]] bool empty() const { return m_entries.empty(); }
  /** How many entries the archive lists. */
  [[nodiscard]] size_t size() const { return m_entries.size(); }

 private:
  std::vector<ArchiveEntry> m_entries;
};

static_assert(ByteSource<ArchiveSource>);

}  // namespace sigil::io
