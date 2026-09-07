/** @file
 * The Photoshop `.abr` reader: the sampled tips out of a brush library.
 */

#include <sigildraw/brush/format/Photoshop.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

#include "Import.h"

namespace sigil::draw::brush::format {

namespace {

/** A big-endian cursor over the file that answers zero and stops rather
 *  than reading past its end, so a truncated or hostile file costs a
 *  short answer and never a fault. */
class Cursor {
 public:
  explicit Cursor(std::span<const std::byte> bytes) : m_bytes(bytes) {}

  [[nodiscard]] bool ok() const { return m_ok; }
  [[nodiscard]] size_t at() const { return m_at; }
  [[nodiscard]] size_t size() const { return m_bytes.size(); }
  /** How many bytes are left ahead of the cursor. */
  [[nodiscard]] size_t left() const { return m_bytes.size() - m_at; }
  /** A cursor over the bytes from @p from to @p to, clamped to what is
   *  here: how a section or a block is read inside its own bounds. */
  [[nodiscard]] Cursor window(size_t from, size_t to) const {
    const size_t first = std::min(from, m_bytes.size());
    const size_t last = std::clamp(to, first, m_bytes.size());
    return Cursor(m_bytes.subspan(first, last - first));
  }

  void seek(size_t offset) {
    if (offset > m_bytes.size()) {
      m_ok = false;
      return;
    }
    m_at = offset;
  }
  void skip(size_t count) { seek(m_at + count); }

  uint8_t byte() {
    if (m_at + 1 > m_bytes.size()) {
      m_ok = false;
      return 0;
    }
    return (uint8_t)m_bytes[m_at++];
  }
  uint16_t word() {
    const uint16_t high = byte();
    return (uint16_t)((high << 8) | byte());
  }
  int32_t integer() {
    const uint32_t high = word();
    return (int32_t)((high << 16) | word());
  }
  /** The next four bytes as a tag, empty at the end of the file. */
  std::string_view tag() {
    if (m_at + 4 > m_bytes.size()) {
      m_ok = false;
      return {};
    }
    const std::string_view text((const char*)m_bytes.data() + m_at, 4);
    m_at += 4;
    return text;
  }
  /** @p count bytes, or an empty span at the end of the file. */
  std::span<const std::byte> run(size_t count) {
    if (m_at + count > m_bytes.size()) {
      m_ok = false;
      return {};
    }
    const std::span<const std::byte> taken = m_bytes.subspan(m_at, count);
    m_at += count;
    return taken;
  }

 private:
  std::span<const std::byte> m_bytes;
  size_t m_at = 0;
  bool m_ok = true;
};

/** How much of a brush block precedes its bounds, which is where the
 *  descriptor this reader does not parse sits. The two subversions put a
 *  different amount there. */
constexpr size_t kPreambleSubversion1 = 47;
constexpr size_t kPreambleSubversion2 = 301;

/** THE MOST PACKBITS EXPANDS A RUN OF BYTES BY. A two-byte pair states
 *  at most 128 repeats and a literal run expands not at all, so no block
 *  decompresses past this multiple of its own length; it is the bound a
 *  bitmap's claimed size is held to before the bitmap is allocated. */
constexpr size_t kPackBitsExpansion = 129;

/** One PackBits row expanded into @p row; false when the source runs out
 *  before the row is full.
 *
 *  Written here because Skia's PackBits decoder sits inside its codecs
 *  behind `SkCodec`, which decodes a whole image out of a stream and is
 *  not callable on a bare row. */
bool expandPackBits(Cursor& cursor, std::span<uint8_t> row) {
  size_t written = 0;
  while (written < row.size() && cursor.ok()) {
    const int8_t control = (int8_t)cursor.byte();
    if (control >= 0) {
      const size_t count = (size_t)control + 1;
      const std::span<const std::byte> literal = cursor.run(count);
      if (literal.size() != count || written + count > row.size()) return false;
      for (size_t index = 0; index < count; ++index)
        row[written + index] = (uint8_t)literal[index];
      written += count;
    } else if (control != -128) {
      const size_t count = (size_t)(1 - control);
      const uint8_t value = cursor.byte();
      if (written + count > row.size()) return false;
      for (size_t index = 0; index < count; ++index)
        row[written + index] = value;
      written += count;
    }
  }
  return written == row.size() && cursor.ok();
}

/** The one brush block at the cursor as coverage bytes; the cursor is
 *  left where the block ends whatever happened inside it. */
std::optional<Tool> readSampledBrush(Cursor& cursor, uint16_t subversion) {
  const size_t blockStart = cursor.at();
  const int32_t blockSize = cursor.integer();
  if (!cursor.ok() || blockSize <= 0) return std::nullopt;
  size_t padded = (size_t)blockSize;
  while (padded % 4 != 0) ++padded;
  const size_t blockEnd = blockStart + 4 + padded;

  const auto finish = [&](std::optional<Tool> result) {
    cursor.seek(blockEnd <= cursor.size() ? blockEnd : cursor.size());
    return result;
  };

  // Everything after the size is read through a cursor over the block's
  // own bytes, so a block whose size is smaller than what it claims to
  // hold answers short rather than reading the next block's bytes as its
  // bounds and its bitmap.
  Cursor body = cursor.window(blockStart + 4, blockEnd);

  body.skip(subversion == 1 ? kPreambleSubversion1 : kPreambleSubversion2);
  const int32_t top = body.integer();
  const int32_t left = body.integer();
  const int32_t bottom = body.integer();
  const int32_t right = body.integer();
  const uint16_t depth = body.word();
  const uint8_t compression = body.byte();
  if (!body.ok()) return finish(std::nullopt);

  const int64_t width = (int64_t)right - left;
  const int64_t height = (int64_t)bottom - top;
  if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
    return finish(std::nullopt);
  const size_t bytesPerSample = depth / 8;
  if (bytesPerSample != 1 && bytesPerSample != 2) return finish(std::nullopt);

  const size_t rowBytes = (size_t)width * bytesPerSample;
  const size_t claimed = (size_t)height * rowBytes;
  // The bytes still inside the block bound the bitmap BEFORE it is
  // allocated: uncompressed the pixels have to be there in full, and
  // compressed they cannot expand further than PackBits can expand. Four
  // header integers otherwise buy a hundred megabytes out of a
  // sixty-byte file.
  const size_t ceiling =
      compression == 0 ? body.left() : body.left() * kPackBitsExpansion;
  if (claimed > ceiling) return finish(std::nullopt);

  std::vector<uint8_t> raw(claimed);
  if (compression == 0) {
    const std::span<const std::byte> source = body.run(raw.size());
    if (source.size() != raw.size()) return finish(std::nullopt);
    std::memcpy(raw.data(), source.data(), raw.size());
  } else {
    // The compressed form lists every row's length first, then the rows
    // themselves; the lengths are not needed to expand a row, only to
    // know the rows are all there.
    for (int64_t row = 0; row < height; ++row) body.word();
    if (!body.ok()) return finish(std::nullopt);
    for (int64_t row = 0; row < height; ++row)
      if (!expandPackBits(
              body, std::span(raw).subspan((size_t)row * rowBytes, rowBytes)))
        return finish(std::nullopt);
  }

  std::vector<uint8_t> coverage((size_t)width * (size_t)height);
  for (size_t index = 0; index < coverage.size(); ++index)
    coverage[index] =
        bytesPerSample == 1 ? raw[index] : raw[index * 2];  // the high byte

  sk_sp<SkImage> artwork = coverageImage(coverage, (int)width, (int)height);
  if (!artwork) return finish(std::nullopt);

  Tool tool = importedTool();
  tool.width = (float)std::max(width, height);
  tool.shape = Shape{.image = std::move(artwork), .mask = ImageMask::Alpha};
  return finish(std::move(tool));
}

}  // namespace

bool isPhotoshopBrushes(std::span<const std::byte> bytes) {
  if (bytes.size() < 4) return false;
  const uint16_t version =
      (uint16_t)(((uint8_t)bytes[0] << 8) | (uint8_t)bytes[1]);
  return version == 6 || version == 7 || version == 10;
}

std::vector<Tool> decodePhotoshopBrushes(std::span<const std::byte> bytes) {
  std::vector<Tool> brushes;
  if (!isPhotoshopBrushes(bytes)) return brushes;

  Cursor cursor(bytes);
  cursor.word();  // version, already checked
  const uint16_t subversion = cursor.word();

  // The file is a run of length-prefixed 8BIM sections; the sampled tips
  // are in `samp` and every other section is stepped over by its length.
  while (cursor.ok() && cursor.at() + 12 <= cursor.size()) {
    if (cursor.tag() != "8BIM") break;
    const std::string_view key = cursor.tag();
    const int32_t length = cursor.integer();
    if (!cursor.ok() || length < 0) break;
    const size_t sectionStart = cursor.at();
    const size_t sectionEnd = sectionStart + (size_t)length;
    if (sectionEnd > cursor.size()) break;

    if (key == "samp") {
      while (cursor.ok() && cursor.at() + 4 <= sectionEnd) {
        const size_t blockStart = cursor.at();
        std::optional<Tool> brush = readSampledBrush(cursor, subversion);
        if (brush) brushes.push_back(std::move(*brush));
        // A block that answered nothing without moving past its own size
        // stated no length to step over, so the section is malformed from
        // here on; reading on would walk it one integer at a time.
        if (!cursor.ok() || cursor.at() <= blockStart + 4) break;
      }
    }
    cursor.seek(sectionEnd);
  }
  return brushes;
}

}  // namespace sigil::draw::brush::format
