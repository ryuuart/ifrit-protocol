#pragma once

/** @file
 * AN ARCHIVE READ AS A BYTE SOURCE: a zip held in memory, whole, whose
 * files answer to their names the way any other source's resources do.
 *
 * A brush, a font pack and a scene bundle are all one file with files
 * inside it, and the reader is the same reader every time — so it lives
 * beside the byte vocabulary rather than inside whichever decoder needed
 * it first. What is here is access: names in, bytes out. What the bytes
 * inside MEAN is the decoding library's, exactly as it is for a file on
 * disk.
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
 *  then answers a name with the bytes already in hand, so a decoder that
 *  asks for three files out of a brush pays for one read.
 *
 *  A name is matched as the archive spells it. Directories are left out
 *  — a name is a path, and what a reader wants is the files under it. */
class ArchiveSource {
 public:
  /** THE MOST ANY ONE ENTRY MAY DECOMPRESS TO, and the reason there is a
   *  number here at all: an archive's directory is a CLAIM, and a
   *  two-hundred-byte file may claim a two-gigabyte entry. The claim is
   *  read before a byte of the entry is, so an entry claiming more than
   *  this is left out rather than allocated for. Tens of megabytes is
   *  past anything an authored asset holds and far under what a hostile
   *  claim asks for. */
  static constexpr size_t kEntryCeiling = 64u * 1024u * 1024u;

  /** An empty source: every name answers null. */
  ArchiveSource() = default;

  /** Reads every regular file out of @p archive. Empty when the bytes
   *  are not a readable archive. Entries claiming more than
   *  @p entryCeiling bytes, or more than a thousand times the archive's
   *  own size, are left out — both bounds are the same question, which
   *  is whether the claim could be true. */
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
  [[nodiscard]] bool empty() const { return m_entries.empty(); }
  [[nodiscard]] size_t size() const { return m_entries.size(); }

 private:
  std::vector<ArchiveEntry> m_entries;
};

static_assert(ByteSource<ArchiveSource>);

}  // namespace sigil::io
