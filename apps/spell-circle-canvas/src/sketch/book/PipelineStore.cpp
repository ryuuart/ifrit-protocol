/** @file
 * The recorded keys on disk: where they stand, the name a set answers
 * to, and the two halves of reading and writing one.
 */

#include "PipelineStore.h"

#include <include/core/SkData.h>
#include <include/effects/SkRuntimeEffect.h>

#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>

namespace fs = std::filesystem;

namespace pipelines {

namespace {

/** WHAT MARKS A FILE AS ONE OF THESE, and what says the shape inside it
 *  is the shape this build writes. The keys themselves carry Skia's own
 *  version and a stale one is refused when it is replayed, so the
 *  number here moves only when the framing around them moves. */
constexpr char kMark[8] = {'S', 'I', 'G', 'P', 'I', 'P', 'E', '2'};

/** A digest with no seed and no table, so the same bodies answer the
 *  same name on every machine and every run — which is the whole
 *  requirement, since the name is written into a file one run and read
 *  by the next. */
std::uint64_t foldIn(std::uint64_t digest, std::string_view text) {
  for (const char letter : text) {
    digest ^= (std::uint64_t)(unsigned char)letter;
    digest *= 0x100000001b3ull;
  }
  return digest;
}

void writeNumber(std::FILE* file, std::uint32_t value) {
  unsigned char bytes[4] = {(unsigned char)(value & 0xffu),
                            (unsigned char)((value >> 8) & 0xffu),
                            (unsigned char)((value >> 16) & 0xffu),
                            (unsigned char)((value >> 24) & 0xffu)};
  std::fwrite(bytes, 1, sizeof bytes, file);
}

bool readNumber(std::FILE* file, std::uint32_t& value) {
  unsigned char bytes[4];
  if (std::fread(bytes, 1, sizeof bytes, file) != sizeof bytes) return false;
  value = (std::uint32_t)bytes[0] | ((std::uint32_t)bytes[1] << 8) |
          ((std::uint32_t)bytes[2] << 16) | ((std::uint32_t)bytes[3] << 24);
  return true;
}

/** The longest one key may claim to be. A key is a description of one
 *  device program, so anything near this is a file that is not one of
 *  these however well its mark reads, and reading it would be an
 *  allocation a corrupt length asked for. */
constexpr std::uint32_t kLongestKey = 1u << 20;

}  // namespace

fs::path storeDirectory() {
  if (const char* named = std::getenv("SIGIL_SKETCHBOOK_PIPELINES");
      named && *named)
    return named;
  const QString cache =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  const fs::path base = cache.isEmpty() ? fs::temp_directory_path()
                                        : fs::path(cache.toStdString());
  return base / "Sketchbook" / "pipelines";
}

fs::path keySetFile(const fs::path& directory, std::string_view name) {
  return directory / (std::string(name) + ".keys");
}

std::string keySetName(std::span<const sk_sp<SkRuntimeEffect>> effects,
                       std::string_view backend) {
  std::uint64_t digest = 0xcbf29ce484222325ull;
  digest = foldIn(digest, backend);
  for (const sk_sp<SkRuntimeEffect>& effect : effects) {
    // The position separator matters: two lists holding the same bodies
    // in a different order name different programs, and running the
    // sources together would give them one name.
    digest = foldIn(digest, "\n");
    if (effect) digest = foldIn(digest, effect->source());
  }
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx", (unsigned long long)digest);
  return hex;
}

std::vector<RecordedPipeline> readKeySet(const fs::path& file) {
  std::vector<RecordedPipeline> recorded;
  std::FILE* open = std::fopen(file.c_str(), "rb");
  if (!open) return recorded;
  char mark[sizeof kMark];
  std::uint32_t count = 0;
  if (std::fread(mark, 1, sizeof mark, open) != sizeof mark ||
      std::memcmp(mark, kMark, sizeof kMark) != 0 || !readNumber(open, count)) {
    std::fclose(open);
    return recorded;
  }
  for (std::uint32_t at = 0; at < count; ++at) {
    std::uint32_t length = 0;
    if (!readNumber(open, length) || length == 0 || length > kLongestKey) break;
    sk_sp<SkData> key = SkData::MakeUninitialized(length);
    if (std::fread(key->writable_data(), 1, length, open) != length) break;
    std::uint32_t described = 0;
    if (!readNumber(open, described) || described > kLongestKey) break;
    std::string description(described, '\0');
    if (described &&
        std::fread(description.data(), 1, described, open) != described)
      break;
    recorded.push_back({std::move(key), std::move(description)});
  }
  std::fclose(open);
  // A truncated file answers with the programs that were whole. Each
  // one stands on its own, so the set is simply smaller than it was
  // meant to be.
  return recorded;
}

bool writeKeySet(const fs::path& file,
                 std::span<const RecordedPipeline> recorded) {
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  const fs::path beside = fs::path(file).concat(".writing");
  std::FILE* open = std::fopen(beside.c_str(), "wb");
  if (!open) return false;
  const auto writable = [](const RecordedPipeline& one) {
    return one.key && !one.key->isEmpty() && one.key->size() <= kLongestKey &&
           one.description.size() <= kLongestKey;
  };
  std::uint32_t count = 0;
  for (const RecordedPipeline& one : recorded)
    if (writable(one)) ++count;
  std::fwrite(kMark, 1, sizeof kMark, open);
  writeNumber(open, count);
  for (const RecordedPipeline& one : recorded) {
    if (!writable(one)) continue;
    writeNumber(open, (std::uint32_t)one.key->size());
    std::fwrite(one.key->data(), 1, one.key->size(), open);
    writeNumber(open, (std::uint32_t)one.description.size());
    std::fwrite(one.description.data(), 1, one.description.size(), open);
  }
  // BOTH HALVES OF THE WRITE. The close flushes the stream's last
  // buffer, so a write can still fail inside it — a file called whole
  // before the close and renamed over the last good one would lose its
  // tail with nothing saying so.
  const bool written = std::ferror(open) == 0;
  const bool closed = std::fclose(open) == 0;
  if (!written || !closed) {
    fs::remove(beside, ec);
    return false;
  }
  fs::rename(beside, file, ec);
  if (ec) {
    fs::remove(beside, ec);
    return false;
  }
  return true;
}

}  // namespace pipelines
