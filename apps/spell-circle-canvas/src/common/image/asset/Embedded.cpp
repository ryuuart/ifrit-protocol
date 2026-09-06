/** @file
 * The signature scan: finding whole encoded images inside a blob whose
 * container this library has no parser for.
 */

#include <sigilimage/asset/Embedded.h>

#include <cstdint>
#include <cstring>

namespace sigil::image {

namespace {

/** The eight bytes every PNG begins with. */
constexpr unsigned char kPngSignature[8] = {0x89, 'P',  'N',  'G',
                                            0x0D, 0x0A, 0x1A, 0x0A};

/** A PNG chunk header is a big-endian length and a four-character type. */
uint32_t beU32(const unsigned char* at) {
  return (uint32_t)at[0] << 24u | (uint32_t)at[1] << 16u |
         (uint32_t)at[2] << 8u | (uint32_t)at[3];
}

/** Walks the chunk table from a signature to the end of IEND, and answers
 *  where the image ends. Zero means the table ran past the blob, which is
 *  a truncated image rather than a short one. */
size_t endOfPng(const unsigned char* data, size_t size, size_t start) {
  size_t at = start + 8;
  while (at + 8 <= size) {
    const uint32_t length = beU32(data + at);
    const bool last = std::memcmp(data + at + 4, "IEND", 4) == 0;
    // Length, type, payload, CRC. A payload that would run past the blob
    // is a table this scan cannot follow.
    if (at + 8 + (size_t)length + 4 > size) return 0;
    at += 8 + length + 4;
    if (last) return at;
  }
  return 0;
}

/** The last printable-ASCII run of at least @p minimum bytes in
 *  [from, upTo) — the name an author gave the image that follows, in
 *  every container that writes one before its payload. */
std::string lastPrintableRun(const unsigned char* data, size_t from,
                             size_t upTo, size_t minimum) {
  std::string last;
  std::string run;
  for (size_t at = from; at < upTo; ++at) {
    const unsigned char byte = data[at];
    if (byte >= 0x20 && byte < 0x7F) {
      run.push_back((char)byte);
      continue;
    }
    if (run.size() >= minimum) last = run;
    run.clear();
  }
  if (run.size() >= minimum) last = run;
  return last;
}

}  // namespace

std::vector<EmbeddedImage> embeddedPngs(std::span<const std::byte> bytes,
                                        const EmbeddedScan& how) {
  std::vector<EmbeddedImage> found;
  const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
  const size_t size = bytes.size();
  size_t previousEnd = 0;
  for (size_t at = 0; at + sizeof kPngSignature <= size;) {
    if (std::memcmp(data + at, kPngSignature, sizeof kPngSignature) != 0) {
      ++at;
      continue;
    }
    const size_t end = endOfPng(data, size, at);
    if (end == 0) break;
    found.push_back({.offset = at,
                     .length = end - at,
                     .name = lastPrintableRun(data, previousEnd, at,
                                              how.minimumNameLength)});
    if (how.limit != 0 && found.size() >= how.limit) break;
    previousEnd = end;
    at = end;
  }
  return found;
}

}  // namespace sigil::image
