/** @file
 * Brushes as resources: the native directory through a hub, the packed
 * archive, and the two importers against files built here byte by byte.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <sigildraw/brush/format/Load.h>
#include <sigildraw/brush/format/Photoshop.h>
#include <sigildraw/brush/format/Procreate.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/hub/Hub.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ScratchDir.h"

namespace {

namespace brush = sigil::draw::brush;
namespace format = sigil::draw::brush::format;

/** A tip drawn here rather than read from disk: a disc that fades out,
 *  dark on white, which is what a tip image usually is. */
std::vector<std::byte> tipPng(int side) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(side, side, true);
  const float centre = (float)side * 0.5f;
  for (int y = 0; y < side; ++y)
    for (int x = 0; x < side; ++x) {
      const float distance =
          std::hypot((float)x + 0.5f - centre, (float)y + 0.5f - centre) /
          centre;
      const uint32_t level =
          (uint32_t)(std::clamp(distance, 0.0f, 1.0f) * 255.0f);
      *bitmap.getAddr32(x, y) = SkPreMultiplyARGB(255, level, level, level);
    }
  bitmap.setImmutable();
  sk_sp<SkData> png = sigil::image::encodeImage(
      *SkImages::RasterFromBitmap(bitmap), sigil::image::Format::Png);
  EXPECT_TRUE(png);
  const std::byte* first = (const std::byte*)png->data();
  return {first, first + png->size()};
}

std::vector<std::byte> textBytes(std::string_view text) {
  const std::byte* first = (const std::byte*)text.data();
  return {first, first + text.size()};
}

/** A byte source over a table, which is the whole of what a loader
 *  needs: no hub, no filesystem, no mounts. */
struct Table {
  std::map<std::string, std::shared_ptr<const sigil::io::Bytes>> entries;

  void put(std::string uri, std::vector<std::byte> bytes) {
    entries[std::move(uri)] =
        std::make_shared<const sigil::io::Bytes>(sigil::io::Bytes{std::move(bytes)});
  }
  std::shared_ptr<const sigil::io::Bytes> fetch(std::string_view uri) {
    const auto found = entries.find(std::string(uri));
    return found == entries.end() ? nullptr : found->second;
  }
};
static_assert(sigil::io::ByteSource<Table>);

TEST(BrushFormat, ADirectoryOfThreeFilesLoadsThroughAnyByteSource) {
  brush::Tool written;
  written.tip = brush::Tip::Image;
  written.width = 26.0f;
  written.opacity = 0.8f;
  written.color = {0.2f, 0.1f, 0.05f, 1.0f};
  written.rotation = brush::Rotation::Fixed;
  written.shape = brush::Shape{
      .mask = brush::ImageMask::Alpha, .spacing = 0.07f, .scatter = 0.3f,
      .angleJitter = 0.25f};
  written.grain = brush::Grain{.space = brush::GrainSpace::Dab,
                               .scale = 1.75f,
                               .depth = 0.6f};
  written.dynamics.size =
      brush::Response{.drive = brush::Drive::Velocity,
                      .curve = {.minimum = 0.3f, .maximum = 1.0f,
                                .bend = 2.0f}};

  Table table;
  table.put("res://ink.sigilbrush/brush.json",
            textBytes(format::encodeBrush(written)));
  table.put("res://ink.sigilbrush/shape.png", tipPng(24));
  table.put("res://ink.sigilbrush/grain.png", tipPng(16));

  const std::optional<brush::Tool> read =
      format::loadBrush(table, "res://ink.sigilbrush");
  ASSERT_TRUE(read);
  EXPECT_EQ(read->tip, brush::Tip::Image);
  EXPECT_FLOAT_EQ(read->width, 26.0f);
  EXPECT_FLOAT_EQ(read->opacity, 0.8f);
  EXPECT_EQ(read->rotation, brush::Rotation::Fixed);
  EXPECT_FLOAT_EQ(read->color.fR, 0.2f);

  ASSERT_TRUE(read->shape);
  EXPECT_TRUE(read->shape->image);
  EXPECT_EQ(read->shape->image->width(), 24);
  EXPECT_EQ(read->shape->mask, brush::ImageMask::Alpha);
  EXPECT_FLOAT_EQ(read->shape->spacing, 0.07f);
  EXPECT_FLOAT_EQ(read->shape->scatter, 0.3f);
  EXPECT_FLOAT_EQ(read->shape->angleJitter, 0.25f);

  ASSERT_TRUE(read->grain);
  EXPECT_TRUE(read->grain->image);
  EXPECT_EQ(read->grain->image->width(), 16);
  EXPECT_EQ(read->grain->space, brush::GrainSpace::Dab);
  EXPECT_FLOAT_EQ(read->grain->scale, 1.75f);
  EXPECT_FLOAT_EQ(read->grain->depth, 0.6f);

  ASSERT_TRUE(read->dynamics.size);
  EXPECT_EQ(read->dynamics.size->drive, brush::Drive::Velocity);
  EXPECT_FLOAT_EQ(read->dynamics.size->curve.minimum, 0.3f);
  EXPECT_FLOAT_EQ(read->dynamics.size->curve.bend, 2.0f);
  EXPECT_FALSE(read->dynamics.opacity);
}

TEST(BrushFormat, ADirectoryLoadsThroughAHubAndItsRegisteredDecoder) {
  sigil::test::ScratchDir scratch{"draw_brush_format"};
  sigil::io::Hub hub;
  hub.mount("res://", scratch.path);
  hub.registerDecoder<brush::Tool>(format::BrushDecoder{});

  brush::Tool written;
  written.tip = brush::Tip::Image;
  written.width = 18.0f;
  written.shape = brush::Shape{.spacing = 0.2f};
  const std::string description = format::encodeBrush(written);
  const std::vector<std::byte> shape = tipPng(12);
  ASSERT_TRUE(hub.write("res://brushes/ink.sigilbrush/brush.json",
                        description.data(), description.size()));
  ASSERT_TRUE(hub.write("res://brushes/ink.sigilbrush/shape.png", shape.data(),
                        shape.size()));

  const std::optional<brush::Tool> read =
      format::loadBrush(hub, "res://brushes/ink.sigilbrush");
  ASSERT_TRUE(read);
  ASSERT_TRUE(read->shape);
  EXPECT_EQ(read->shape->image->width(), 12);
  EXPECT_FLOAT_EQ(read->shape->spacing, 0.2f);
  EXPECT_FLOAT_EQ(brush::spacingOf(*read), 18.0f * 0.2f);

  // The same brush, this time as one file the hub's own load<T> answers.
  ASSERT_TRUE(hub.write("res://brushes/bare.sigilbrush", description.data(),
                        description.size()));
  const std::shared_ptr<const brush::Tool> bare =
      hub.load<brush::Tool>("res://brushes/bare.sigilbrush");
  ASSERT_TRUE(bare);
  EXPECT_FLOAT_EQ(bare->width, 18.0f);
  // No artwork arrived with it, so there is nothing to stamp.
  EXPECT_FALSE(bare->shape);
  EXPECT_EQ(bare->tip, brush::Tip::Nib);
}

/** A zip written by hand, every entry stored rather than deflated: the
 *  smallest archive both importers accept. */
class ZipWriter {
 public:
  void add(std::string name, std::span<const std::byte> content) {
    const uint32_t offset = (uint32_t)m_bytes.size();
    const uint32_t crc = crc32(content);
    header(0x04034b50, name, content.size(), crc);
    append(content);
    m_directory.push_back({std::move(name), offset, (uint32_t)content.size(),
                           crc});
  }

  std::vector<std::byte> finish() {
    const uint32_t directoryStart = (uint32_t)m_bytes.size();
    for (const Record& record : m_directory) {
      put32(0x02014b50);
      put16(20);
      centralRest(record);
    }
    const uint32_t directorySize = (uint32_t)m_bytes.size() - directoryStart;
    put32(0x06054b50);
    put16(0);
    put16(0);
    put16((uint16_t)m_directory.size());
    put16((uint16_t)m_directory.size());
    put32(directorySize);
    put32(directoryStart);
    put16(0);
    return m_bytes;
  }

 private:
  struct Record {
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t crc;
  };

  void put16(uint16_t value) {
    m_bytes.push_back((std::byte)(value & 0xff));
    m_bytes.push_back((std::byte)(value >> 8));
  }
  void put32(uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
      m_bytes.push_back((std::byte)((value >> shift) & 0xff));
  }
  void append(std::span<const std::byte> content) {
    m_bytes.insert(m_bytes.end(), content.begin(), content.end());
  }
  void putName(std::string_view name) {
    for (char letter : name) m_bytes.push_back((std::byte)letter);
  }

  void header(uint32_t signature, std::string_view name, size_t size,
              uint32_t crc) {
    put32(signature);
    put16(20);  // version needed
    put16(0);   // flags
    put16(0);   // stored
    put16(0);   // time
    put16(0);   // date
    put32(crc);
    put32((uint32_t)size);
    put32((uint32_t)size);
    put16((uint16_t)name.size());
    put16(0);
    putName(name);
  }

  void centralRest(const Record& record) {
    put16(20);
    put16(0);
    put16(0);
    put16(0);
    put16(0);
    put32(record.crc);
    put32(record.size);
    put32(record.size);
    put16((uint16_t)record.name.size());
    put16(0);
    put16(0);
    put16(0);
    put16(0);
    put32(0);
    put32(record.offset);
    putName(record.name);
  }

  static uint32_t crc32(std::span<const std::byte> content) {
    uint32_t crc = 0xffffffffu;
    for (std::byte value : content) {
      crc ^= (uint8_t)value;
      for (int bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)(-(int32_t)(crc & 1)));
    }
    return ~crc;
  }

  std::vector<std::byte> m_bytes;
  std::vector<Record> m_directory;
};

TEST(BrushFormat, APackedArchiveCarriesTheSameThreeParts) {
  brush::Tool written;
  written.width = 33.0f;
  written.shape = brush::Shape{.scatter = 0.4f};

  ZipWriter zip;
  const std::string description = format::encodeBrush(written);
  zip.add("brush.json", textBytes(description));
  zip.add("shape.png", tipPng(20));
  const std::vector<std::byte> archive = zip.finish();

  const std::optional<brush::Tool> read =
      format::decodeBrush(sigil::io::Bytes{archive}, "ink.sigilbrush");
  ASSERT_TRUE(read);
  EXPECT_FLOAT_EQ(read->width, 33.0f);
  ASSERT_TRUE(read->shape);
  EXPECT_EQ(read->shape->image->width(), 20);
  EXPECT_FLOAT_EQ(read->shape->scatter, 0.4f);
}

TEST(BrushFormat, AProcreateArchiveGivesUpItsShapeAndItsGrain) {
  ZipWriter zip;
  zip.add("Brush.archive", textBytes("bplist00"));
  zip.add("Shape.png", tipPng(28));
  zip.add("Grain.png", tipPng(14));
  const std::vector<std::byte> archive = zip.finish();

  const std::optional<brush::Tool> read = format::decodeProcreateBrush(archive);
  ASSERT_TRUE(read);
  ASSERT_TRUE(read->shape);
  EXPECT_EQ(read->shape->image->width(), 28);
  ASSERT_TRUE(read->grain);
  EXPECT_EQ(read->grain->image->width(), 14);
  // The archive is not read, so the numbers are this library's.
  EXPECT_EQ(read->grain->space, brush::GrainSpace::Stroke);
  EXPECT_FLOAT_EQ(read->shape->spacing, brush::Shape{}.spacing);

  // The same bytes through the one decoder every form goes through.
  const std::optional<brush::Tool> sniffed =
      format::decodeBrush(sigil::io::Bytes{archive}, "swash.brush");
  ASSERT_TRUE(sniffed);
  EXPECT_TRUE(sniffed->shape);
}

/** A version 6 `.abr` written by hand: one 8BIM `samp` section holding
 *  one uncompressed sampled tip. */
class AbrWriter {
 public:
  explicit AbrWriter(uint16_t subversion) : m_subversion(subversion) {
    put16(6);
    put16(subversion);
  }

  void addBrush(int width, int height, uint8_t level) {
    std::vector<std::byte> block;
    const size_t preamble = m_subversion == 1 ? 47u : 301u;
    block.resize(preamble, std::byte{0});
    const auto put32be = [&block](int32_t value) {
      for (int shift = 24; shift >= 0; shift -= 8)
        block.push_back((std::byte)((value >> shift) & 0xff));
    };
    put32be(0);          // top
    put32be(0);          // left
    put32be(height);     // bottom
    put32be(width);      // right
    block.push_back(std::byte{0});
    block.push_back(std::byte{8});  // depth
    block.push_back(std::byte{0});  // uncompressed
    for (int index = 0; index < width * height; ++index)
      block.push_back((std::byte)level);
    m_brushes.push_back(std::move(block));
  }

  std::vector<std::byte> finish() {
    std::vector<std::byte> section;
    for (const std::vector<std::byte>& block : m_brushes) {
      const int32_t size = (int32_t)block.size();
      for (int shift = 24; shift >= 0; shift -= 8)
        section.push_back((std::byte)((size >> shift) & 0xff));
      section.insert(section.end(), block.begin(), block.end());
      while (section.size() % 4 != 0) section.push_back(std::byte{0});
    }
    putTag("8BIM");
    putTag("samp");
    put32((int32_t)section.size());
    m_bytes.insert(m_bytes.end(), section.begin(), section.end());
    return m_bytes;
  }

 private:
  void put16(uint16_t value) {
    m_bytes.push_back((std::byte)(value >> 8));
    m_bytes.push_back((std::byte)(value & 0xff));
  }
  void put32(int32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8)
      m_bytes.push_back((std::byte)((value >> shift) & 0xff));
  }
  void putTag(std::string_view tag) {
    for (char letter : tag) m_bytes.push_back((std::byte)letter);
  }

  uint16_t m_subversion;
  std::vector<std::byte> m_bytes;
  std::vector<std::vector<std::byte>> m_brushes;
};

TEST(BrushFormat, APhotoshopLibraryGivesUpEverySampledTip) {
  AbrWriter writer(2);
  writer.addBrush(12, 9, 255);
  writer.addBrush(6, 6, 128);
  const std::vector<std::byte> abr = writer.finish();

  EXPECT_TRUE(format::isPhotoshopBrushes(abr));
  const std::vector<brush::Tool> brushes = format::decodePhotoshopBrushes(abr);
  ASSERT_EQ(brushes.size(), 2u);

  ASSERT_TRUE(brushes[0].shape);
  EXPECT_EQ(brushes[0].shape->image->width(), 12);
  EXPECT_EQ(brushes[0].shape->image->height(), 9);
  // The bitmap is the coverage, so it is read through the alpha channel.
  EXPECT_EQ(brushes[0].shape->mask, brush::ImageMask::Alpha);
  EXPECT_EQ(brushes[0].tip, brush::Tip::Image);
  EXPECT_FLOAT_EQ(brushes[0].width, 12.0f);

  ASSERT_TRUE(brushes[1].shape);
  EXPECT_EQ(brushes[1].shape->image->width(), 6);

  // The descriptor is not read, so every other number is this library's.
  EXPECT_FLOAT_EQ(brushes[0].shape->spacing, brush::Shape{}.spacing);
  EXPECT_FALSE(brushes[0].grain);

  // And the one decoder answers with the first of them.
  const std::optional<brush::Tool> sniffed =
      format::decodeBrush(sigil::io::Bytes{abr}, "library.abr");
  ASSERT_TRUE(sniffed);
  ASSERT_TRUE(sniffed->shape);
  EXPECT_EQ(sniffed->shape->image->width(), 12);
}

TEST(BrushFormat, TheSubversionDecidesHowMuchPrecedesTheBitmap) {
  AbrWriter writer(1);
  writer.addBrush(7, 5, 200);
  const std::vector<brush::Tool> brushes =
      format::decodePhotoshopBrushes(writer.finish());
  ASSERT_EQ(brushes.size(), 1u);
  ASSERT_TRUE(brushes[0].shape);
  EXPECT_EQ(brushes[0].shape->image->width(), 7);
  EXPECT_EQ(brushes[0].shape->image->height(), 5);
}

TEST(BrushFormat, BytesThatAreNoBrushAnswerNothing) {
  EXPECT_FALSE(format::decodeBrush(sigil::io::Bytes{}, "empty"));
  EXPECT_FALSE(
      format::decodeBrush(sigil::io::Bytes{textBytes("not a brush")}, "x.abr"));
  EXPECT_FALSE(format::isPhotoshopBrushes(textBytes("PK\3\4")));
  EXPECT_TRUE(format::decodePhotoshopBrushes(textBytes("nope")).empty());

  Table empty;
  EXPECT_FALSE(format::loadBrush(empty, "res://missing.sigilbrush"));
}

}  // namespace
