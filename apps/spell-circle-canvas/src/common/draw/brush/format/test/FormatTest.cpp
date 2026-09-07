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
    entries[std::move(uri)] = std::make_shared<const sigil::io::Bytes>(
        sigil::io::Bytes{std::move(bytes)});
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
  written.shape = brush::Shape{.mask = brush::ImageMask::Alpha,
                               .spacing = 0.07f,
                               .scatter = 0.3f,
                               .angleJitter = 0.25f};
  written.grain = brush::Grain{
      .space = brush::GrainSpace::Dab, .scale = 1.75f, .depth = 0.6f};
  written.dynamics.size = brush::Response{
      .drive = brush::Drive::Velocity,
      .curve = {.minimum = 0.3f, .maximum = 1.0f, .bend = 2.0f}};

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
    addClaiming(std::move(name), content, 0);
  }

  /** The same entry, with @p claimed written as the uncompressed size
   *  wherever the archive states one: what a hostile archive does. */
  void addClaiming(std::string name, std::span<const std::byte> content,
                   uint32_t claimed) {
    const uint32_t offset = (uint32_t)m_bytes.size();
    const uint32_t crc = crc32(content);
    const uint32_t stated = claimed != 0 ? claimed : (uint32_t)content.size();
    header(0x04034b50, name, content.size(), stated, crc);
    append(content);
    m_directory.push_back(
        {std::move(name), offset, (uint32_t)content.size(), stated, crc});
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
    uint32_t stated;
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
              uint32_t stated, uint32_t crc) {
    put32(signature);
    put16(20);  // version needed
    put16(0);   // flags
    put16(0);   // stored
    put16(0);   // time
    put16(0);   // date
    put32(crc);
    put32((uint32_t)size);
    put32(stated);
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
    put32(record.stated);
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
      format::decodeBrush(archive, "ink.sigilbrush");
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
      format::decodeBrush(archive, "swash.brush");
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
    std::vector<std::byte> block = blockHeader(width, height, 8, 0);
    for (int index = 0; index < width * height; ++index)
      block.push_back((std::byte)level);
    m_brushes.push_back(std::move(block));
  }

  /** A block that states bounds and stops: the header a hostile file
   *  carries, with no pixel byte behind the size it claims. */
  void addHeaderOnly(int width, int height, uint16_t depth,
                     uint8_t compression) {
    m_brushes.push_back(blockHeader(width, height, depth, compression));
  }

  /** The block's own size field, written as @p size whatever the block
   *  holds. */
  void claimBlockSize(int32_t size) { m_claimedBlockSize = size; }
  /** The section's length field, written as @p length whatever it
   *  holds. */
  void claimSectionLength(int32_t length) { m_claimedSection = length; }

  std::vector<std::byte> finish() {
    std::vector<std::byte> section;
    for (const std::vector<std::byte>& block : m_brushes) {
      const int32_t size =
          m_claimedBlockSize ? *m_claimedBlockSize : (int32_t)block.size();
      for (int shift = 24; shift >= 0; shift -= 8)
        section.push_back((std::byte)((size >> shift) & 0xff));
      section.insert(section.end(), block.begin(), block.end());
      while (section.size() % 4 != 0) section.push_back(std::byte{0});
    }
    putTag("8BIM");
    putTag("samp");
    put32(m_claimedSection ? *m_claimedSection : (int32_t)section.size());
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

  std::vector<std::byte> blockHeader(int width, int height, uint16_t depth,
                                     uint8_t compression) const {
    std::vector<std::byte> block;
    const size_t preamble = m_subversion == 1 ? 47u : 301u;
    block.resize(preamble, std::byte{0});
    const auto put32be = [&block](int32_t value) {
      for (int shift = 24; shift >= 0; shift -= 8)
        block.push_back((std::byte)((value >> shift) & 0xff));
    };
    put32be(0);       // top
    put32be(0);       // left
    put32be(height);  // bottom
    put32be(width);   // right
    block.push_back((std::byte)(depth >> 8));
    block.push_back((std::byte)(depth & 0xff));
    block.push_back((std::byte)compression);
    return block;
  }

  uint16_t m_subversion;
  std::optional<int32_t> m_claimedBlockSize;
  std::optional<int32_t> m_claimedSection;
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
      format::decodeBrush(abr, "library.abr");
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
  EXPECT_FALSE(format::decodeBrush({}, "empty"));
  EXPECT_FALSE(format::decodeBrush(textBytes("not a brush"), "x.abr"));
  EXPECT_FALSE(format::isPhotoshopBrushes(textBytes("PK\3\4")));
  EXPECT_TRUE(format::decodePhotoshopBrushes(textBytes("nope")).empty());

  Table empty;
  EXPECT_FALSE(format::loadBrush(empty, "res://missing.sigilbrush"));
}

/** Every value of a tool that the description can hold, written and read
 *  back: the format's promise is that a tool survives it. */
TEST(BrushFormat, EveryFieldOfATheDescriptionCanHoldSurvivesTheRoundTrip) {
  brush::Tool authored;
  authored.tip = brush::Tip::Fibres;
  authored.color = {1.0f / 3.0f, 0.2f, 0.7f, 0.9f};
  authored.width = 1.0f / 3.0f;
  authored.spacing = 2.0f / 7.0f;
  authored.opacity = 0.123456789f;
  authored.scatter = 0.37f;
  authored.density = 1.2f;
  authored.bristles = 37;
  authored.pressure = {0.3f, 0.9f, 0.15f};
  authored.pressure.gaussian = brush::Pressure::Gaussian{.center = 0.4f,
                                                         .width = 0.6f,
                                                         .sharpness = 2.5f,
                                                         .minimum = 0.1f,
                                                         .maximum = 0.95f,
                                                         .centerJitter = 0.05f,
                                                         .widthJitter = 0.07f};
  authored.pressure.variation = brush::Pressure::Variation{
      .offset = 0.11f, .scale = 0.12f, .warp = 0.13f, .tilt = 0.14f};
  authored.blend = sigil::draw::MULTIPLY;
  authored.rotation = brush::Rotation::Tilt;
  authored.angle = 0.7853982f;
  authored.aspect = 0.45f;
  authored.sizeJitter = 0.21f;
  authored.opacityJitter = 0.22f;
  authored.spacingJitter = 0.23f;
  authored.speedSize = 0.31f;
  authored.speedOpacity = 0.32f;
  authored.speedReference = 933.5f;
  authored.pressureSize = 0.81f;
  authored.pressureOpacity = 0.82f;
  authored.tiltSize = 0.41f;
  authored.tiltOpacity = 0.42f;
  authored.tiltAspect = 0.43f;
  authored.tiltOffset = 0.44f;
  authored.sharpness = 0.61f;
  authored.noise = 0.62f;
  authored.markerTip = true;
  authored.shape = brush::Shape{.mask = brush::ImageMask::Alpha,
                                .spacing = 1.0f / 3.0f,
                                .scatter = 0.29f,
                                .angleJitter = 0.19f};
  authored.grain = brush::Grain{
      .space = brush::GrainSpace::Dab, .scale = 1.0f / 7.0f, .depth = 0.66f};
  authored.dynamics.opacity = brush::Response{
      .drive = brush::Drive::Tilt,
      .curve = {.minimum = 1.0f / 9.0f, .maximum = 0.87f, .bend = 1.75f}};

  const std::string description = format::encodeBrush(authored);
  const std::vector<std::byte> artwork = tipPng(8);
  const std::optional<brush::Tool> read =
      format::assembleBrush(textBytes(description), artwork, artwork);
  ASSERT_TRUE(read);

  EXPECT_EQ(read->tip, authored.tip);
  EXPECT_EQ(read->color.fR, authored.color.fR);
  EXPECT_EQ(read->color.fG, authored.color.fG);
  EXPECT_EQ(read->color.fB, authored.color.fB);
  EXPECT_EQ(read->color.fA, authored.color.fA);
  EXPECT_EQ(read->width, authored.width);
  EXPECT_EQ(read->spacing, authored.spacing);
  EXPECT_EQ(read->opacity, authored.opacity);
  EXPECT_EQ(read->scatter, authored.scatter);
  EXPECT_EQ(read->density, authored.density);
  EXPECT_EQ(read->bristles, authored.bristles);
  EXPECT_EQ(read->pressure.start, authored.pressure.start);
  EXPECT_EQ(read->pressure.middle, authored.pressure.middle);
  EXPECT_EQ(read->pressure.end, authored.pressure.end);
  ASSERT_TRUE(read->pressure.gaussian);
  EXPECT_EQ(read->pressure.gaussian->center, 0.4f);
  EXPECT_EQ(read->pressure.gaussian->width, 0.6f);
  EXPECT_EQ(read->pressure.gaussian->sharpness, 2.5f);
  EXPECT_EQ(read->pressure.gaussian->minimum, 0.1f);
  EXPECT_EQ(read->pressure.gaussian->maximum, 0.95f);
  EXPECT_EQ(read->pressure.gaussian->centerJitter, 0.05f);
  EXPECT_EQ(read->pressure.gaussian->widthJitter, 0.07f);
  ASSERT_TRUE(read->pressure.variation);
  EXPECT_EQ(*read->pressure.variation, *authored.pressure.variation);
  EXPECT_EQ(read->blend, authored.blend);
  EXPECT_EQ(read->rotation, authored.rotation);
  EXPECT_EQ(read->angle, authored.angle);
  EXPECT_EQ(read->aspect, authored.aspect);
  EXPECT_EQ(read->sizeJitter, authored.sizeJitter);
  EXPECT_EQ(read->opacityJitter, authored.opacityJitter);
  EXPECT_EQ(read->spacingJitter, authored.spacingJitter);
  EXPECT_EQ(read->speedSize, authored.speedSize);
  EXPECT_EQ(read->speedOpacity, authored.speedOpacity);
  EXPECT_EQ(read->speedReference, authored.speedReference);
  EXPECT_EQ(read->pressureSize, authored.pressureSize);
  EXPECT_EQ(read->pressureOpacity, authored.pressureOpacity);
  EXPECT_EQ(read->tiltSize, authored.tiltSize);
  EXPECT_EQ(read->tiltOpacity, authored.tiltOpacity);
  EXPECT_EQ(read->tiltAspect, authored.tiltAspect);
  EXPECT_EQ(read->tiltOffset, authored.tiltOffset);
  EXPECT_EQ(read->sharpness, authored.sharpness);
  EXPECT_EQ(read->noise, authored.noise);
  EXPECT_EQ(read->markerTip, authored.markerTip);

  ASSERT_TRUE(read->shape);
  EXPECT_EQ(read->shape->mask, authored.shape->mask);
  EXPECT_EQ(read->shape->spacing, authored.shape->spacing);
  EXPECT_EQ(read->shape->scatter, authored.shape->scatter);
  EXPECT_EQ(read->shape->angleJitter, authored.shape->angleJitter);
  ASSERT_TRUE(read->grain);
  EXPECT_EQ(read->grain->space, authored.grain->space);
  EXPECT_EQ(read->grain->scale, authored.grain->scale);
  EXPECT_EQ(read->grain->depth, authored.grain->depth);
  ASSERT_TRUE(read->dynamics.opacity);
  EXPECT_EQ(read->dynamics.opacity->drive, brush::Drive::Tilt);
  EXPECT_EQ(read->dynamics.opacity->curve.minimum, 1.0f / 9.0f);
  EXPECT_EQ(read->dynamics.opacity->curve.maximum, 0.87f);
  EXPECT_EQ(read->dynamics.opacity->curve.bend, 1.75f);
  EXPECT_FALSE(read->dynamics.size);
  EXPECT_FALSE(read->dynamics.flow);
}

/** A tool that states no bell and no per-stroke variation reads back
 *  with neither, rather than with the library's defaults. */
TEST(BrushFormat, AnEnvelopeWithoutItsOptionalPartsReadsBackWithoutThem) {
  brush::Tool authored;
  authored.pressure = {0.5f, 0.5f, 0.5f};
  authored.pressure.gaussian.reset();
  authored.pressure.variation.reset();
  authored.markerTip = false;

  const std::string description = format::encodeBrush(authored);
  const std::optional<brush::Tool> read =
      format::assembleBrush(textBytes(description), {}, {});
  ASSERT_TRUE(read);
  EXPECT_FALSE(read->pressure.gaussian);
  EXPECT_FALSE(read->pressure.variation);
  EXPECT_FALSE(read->markerTip);
  EXPECT_EQ(read->pressure.middle, 0.5f);
}

/** Whatever a decoded tool is, it is a tool: what every hostile case
 *  below asserts about the answer it gets. */
void expectWellFormed(const std::optional<brush::Tool>& tool) {
  if (!tool) return;
  EXPECT_GT(tool->width, 0.0f);
  if (tool->shape) EXPECT_TRUE(tool->shape->image);
  if (tool->grain) EXPECT_TRUE(tool->grain->image);
}

TEST(BrushFormat, APhotoshopLibraryTruncatedAnywhereAnswersShort) {
  AbrWriter writer(1);
  writer.addBrush(9, 7, 190);
  const std::vector<std::byte> whole = writer.finish();

  for (size_t length = 0; length <= whole.size(); ++length) {
    const std::span<const std::byte> cut(whole.data(), length);
    const std::vector<brush::Tool> brushes =
        format::decodePhotoshopBrushes(cut);
    EXPECT_LE(brushes.size(), 1u) << "truncated at " << length;
    for (const brush::Tool& brush : brushes) expectWellFormed(brush);
    expectWellFormed(format::decodeBrush(cut, "library.abr"));
  }
}

TEST(BrushFormat, APhotoshopHeaderNeverBuysMoreThanItsBlockHolds) {
  // A block that claims the reader's largest bitmap and carries not one
  // pixel byte: the header alone must never be worth an allocation.
  {
    AbrWriter writer(1);
    writer.addHeaderOnly(8192, 8192, 8, 0);
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
  {
    AbrWriter writer(1);
    writer.addHeaderOnly(8192, 8192, 8, 1);  // and compressed
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
  // Bounds that describe no bitmap at all, and a depth the format does
  // not carry.
  {
    AbrWriter writer(1);
    writer.addHeaderOnly(0, 0, 8, 0);
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
  {
    AbrWriter writer(1);
    writer.addHeaderOnly(9000, 4, 8, 0);
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
  {
    AbrWriter writer(1);
    writer.addHeaderOnly(4, 4, 4, 0);
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
  // A block whose size field is smaller than the block: its bitmap is
  // the next block's bytes, and it must not be read as one.
  {
    AbrWriter writer(1);
    writer.addBrush(6, 6, 120);
    writer.claimBlockSize(60);
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
  // A section that claims to run past the end of the file.
  {
    AbrWriter writer(1);
    writer.addBrush(6, 6, 120);
    writer.claimSectionLength(1 << 20);
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
  // And a block size the reader would step by forever.
  {
    AbrWriter writer(1);
    writer.addBrush(6, 6, 120);
    writer.claimBlockSize(0);
    EXPECT_TRUE(format::decodePhotoshopBrushes(writer.finish()).empty());
  }
}

TEST(BrushFormat, AnArchiveTruncatedAnywhereAnswersShort) {
  ZipWriter native;
  brush::Tool written;
  written.width = 21.0f;
  native.add("brush.json", textBytes(format::encodeBrush(written)));
  native.add("shape.png", tipPng(4));
  const std::vector<std::byte> pack = native.finish();

  for (size_t length = 0; length <= pack.size(); ++length) {
    const std::span<const std::byte> cut(pack.data(), length);
    expectWellFormed(format::decodeBrush(cut, "ink.sigilbrush"));
    expectWellFormed(format::decodeProcreateBrush(cut));
  }
}

TEST(BrushFormat, AnArchiveEntryClaimingMoreThanCouldBeThereIsLeftOut) {
  ZipWriter zip;
  zip.addClaiming("Shape.png", tipPng(4), 1u << 31);
  const std::vector<std::byte> archive = zip.finish();

  EXPECT_FALSE(format::decodeProcreateBrush(archive));
  EXPECT_FALSE(format::decodeBrush(archive, "swash.brush"));

  // An archive holding nothing a brush is made of answers nothing too.
  ZipWriter bare;
  bare.add("notes/", {});
  EXPECT_FALSE(format::decodeBrush(bare.finish(), "empty.sigilbrush"));
}

TEST(BrushFormat, AMalformedDescriptionLeavesTheToolAsItWas) {
  const std::vector<std::byte> artwork = tipPng(4);
  for (std::string_view text :
       {"", "{", "{\"width\":", "[]", "{\"width\": \"wide\"}",
        "{\"tip\": 4, \"shape\": 7}", "null", "{\"bristles\": \"many\"}"}) {
    const std::optional<brush::Tool> read =
        format::assembleBrush(textBytes(text), artwork, {});
    ASSERT_TRUE(read) << text;
    expectWellFormed(read);
    EXPECT_EQ(read->width, brush::Tool{}.width) << text;
  }

  // Truncating a well-formed description anywhere costs at most its
  // numbers, never a fault.
  brush::Tool written;
  written.width = 19.0f;
  const std::string description = format::encodeBrush(written);
  for (size_t length = 0; length <= description.size(); ++length)
    expectWellFormed(format::assembleBrush(
        textBytes(std::string_view(description).substr(0, length)), artwork,
        {}));
}

}  // namespace
