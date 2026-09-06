/** @file
 * The archive reader, over minizip-ng's memory stream.
 */

#include "sigilio/source/Archive.h"

#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <cstdint>
#include <cstring>

namespace sigil::io {

namespace {

/** minizip's handles are opaque void pointers with paired create/delete
 *  calls; these two carriers are what makes an early return safe. */
struct MemoryStream {
  void* handle = mz_stream_mem_create();
  ~MemoryStream() {
    if (handle) mz_stream_mem_delete(&handle);
  }
};

struct Reader {
  void* handle = mz_zip_reader_create();
  ~Reader() {
    if (handle) mz_zip_reader_delete(&handle);
  }
};

/** How much larger than the archive itself one entry may claim to be.
 *  Compression does not reach this ratio on anything an asset holds, so
 *  a claim past it is a claim about a file that is not there. */
constexpr size_t kMaxExpansion = 1024u;

}  // namespace

bool ArchiveSource::isArchive(std::span<const std::byte> archive) {
  static constexpr unsigned char kLocalFileHeader[4] = {'P', 'K', 0x03, 0x04};
  if (archive.size() < sizeof(kLocalFileHeader)) return false;
  return std::memcmp(archive.data(), kLocalFileHeader,
                     sizeof(kLocalFileHeader)) == 0;
}

ArchiveSource::ArchiveSource(std::span<const std::byte> archive,
                             size_t entryCeiling) {
  if (!isArchive(archive)) return;
  // The length handed to minizip is a signed 32-bit count, so an archive
  // of two gigabytes or more would arrive as a negative length. Refusing
  // it is the answer; truncating it would read a different file.
  if (archive.size() > (size_t)INT32_MAX) return;

  MemoryStream stream;
  Reader reader;
  if (!stream.handle || !reader.handle) return;
  // The buffer is only read from, and the stream never outlives this
  // call, so handing the caller's bytes over without a copy is safe.
  mz_stream_mem_set_buffer(stream.handle,
                           const_cast<std::byte*>(archive.data()),
                           (int32_t)archive.size());
  if (mz_stream_open(stream.handle, nullptr, MZ_OPEN_MODE_READ) != MZ_OK)
    return;
  if (mz_zip_reader_open(reader.handle, stream.handle) != MZ_OK) return;

  const size_t expansionCeiling = archive.size() > entryCeiling / kMaxExpansion
                                      ? entryCeiling
                                      : archive.size() * kMaxExpansion;
  const size_t ceiling =
      entryCeiling < expansionCeiling ? entryCeiling : expansionCeiling;

  int32_t status = mz_zip_reader_goto_first_entry(reader.handle);
  while (status == MZ_OK) {
    mz_zip_file* info = nullptr;
    if (mz_zip_reader_entry_get_info(reader.handle, &info) == MZ_OK && info &&
        info->filename && mz_zip_reader_entry_is_dir(reader.handle) != MZ_OK) {
      // The length is what the archive's directory CLAIMS, read before
      // any byte of the entry is: an entry claiming more than could fit
      // is left out rather than allocated for.
      const int32_t claimed =
          mz_zip_reader_entry_save_buffer_length(reader.handle);
      if (claimed >= 0 && (size_t)claimed <= ceiling) {
        auto bytes = std::make_shared<Bytes>();
        if (claimed > 0) {
          bytes->bytes.resize((size_t)claimed);
          if (mz_zip_reader_entry_save_buffer(
                  reader.handle, bytes->bytes.data(), claimed) != MZ_OK)
            bytes->bytes.clear();
        }
        m_entries.push_back({info->filename, std::move(bytes)});
      }
    }
    status = mz_zip_reader_goto_next_entry(reader.handle);
  }
  mz_zip_reader_close(reader.handle);
}

std::shared_ptr<const Bytes> ArchiveSource::fetch(std::string_view uri) const {
  for (const ArchiveEntry& entry : m_entries)
    if (entry.name == uri) return entry.bytes;
  return nullptr;
}

}  // namespace sigil::io
