/** @file
 * The zip reader, over minizip-ng's memory stream.
 */

#include "Zip.h"

#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <cstring>

namespace sigil::draw::brush::format {

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

}  // namespace

bool isZip(std::span<const std::byte> archive) {
  static constexpr unsigned char kLocalFileHeader[4] = {'P', 'K', 0x03, 0x04};
  if (archive.size() < sizeof(kLocalFileHeader)) return false;
  return std::memcmp(archive.data(), kLocalFileHeader,
                     sizeof(kLocalFileHeader)) == 0;
}

std::vector<ZipEntry> readZip(std::span<const std::byte> archive) {
  std::vector<ZipEntry> entries;
  if (!isZip(archive)) return entries;

  MemoryStream stream;
  Reader reader;
  if (!stream.handle || !reader.handle) return entries;
  // The buffer is only read from, and the stream never outlives this
  // call, so handing the caller's bytes over without a copy is safe.
  mz_stream_mem_set_buffer(stream.handle,
                           const_cast<std::byte*>(archive.data()),
                           (int32_t)archive.size());
  if (mz_stream_open(stream.handle, nullptr, MZ_OPEN_MODE_READ) != MZ_OK)
    return entries;
  if (mz_zip_reader_open(reader.handle, stream.handle) != MZ_OK) return entries;

  int32_t status = mz_zip_reader_goto_first_entry(reader.handle);
  while (status == MZ_OK) {
    mz_zip_file* info = nullptr;
    if (mz_zip_reader_entry_get_info(reader.handle, &info) == MZ_OK && info &&
        info->filename && mz_zip_reader_entry_is_dir(reader.handle) != MZ_OK) {
      const int32_t size = mz_zip_reader_entry_save_buffer_length(reader.handle);
      ZipEntry entry{.name = info->filename};
      if (size > 0) {
        entry.bytes.resize((size_t)size);
        if (mz_zip_reader_entry_save_buffer(reader.handle, entry.bytes.data(),
                                            size) != MZ_OK)
          entry.bytes.clear();
      }
      entries.push_back(std::move(entry));
    }
    status = mz_zip_reader_goto_next_entry(reader.handle);
  }
  mz_zip_reader_close(reader.handle);
  return entries;
}

}  // namespace sigil::draw::brush::format
