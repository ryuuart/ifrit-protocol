/** @file
 * The archive byte source: one file with files inside it, answered by
 * name, and what it does with a directory that claims more than could be
 * there.
 *
 * The fixtures are written by hand, byte for byte, because the claim
 * this file is about — an entry's stated uncompressed size — is a field
 * no writing library will let a caller lie in.
 */

#include <gtest/gtest.h>
#include <sigilio/source/Archive.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace sigil::io;

namespace {

std::vector<std::byte> textBytes(std::string_view text) {
  std::vector<std::byte> bytes(text.size());
  for (size_t i = 0; i < text.size(); ++i) bytes[i] = (std::byte)text[i];
  return bytes;
}

/** A zip written by hand, every entry stored rather than deflated, and
 *  every entry's stated uncompressed size settable independently of what
 *  it actually holds. */
class ZipWriter {
 public:
  /** @p claimed is what the archive SAYS the entry decompresses to; zero
   *  means "what it really is". */
  void add(std::string name, std::span<const std::byte> content,
           uint32_t claimed = 0) {
    const auto offset = (uint32_t)m_bytes.size();
    const uint32_t crc = crc32(content);
    const uint32_t stated = claimed ? claimed : (uint32_t)content.size();
    put32(0x04034b50);
    put16(20);
    put16(0);
    put16(0);
    put16(0);
    put16(0);
    put32(crc);
    put32((uint32_t)content.size());
    put32(stated);
    put16((uint16_t)name.size());
    put16(0);
    putName(name);
    m_bytes.insert(m_bytes.end(), content.begin(), content.end());
    m_directory.push_back(
        {std::move(name), offset, (uint32_t)content.size(), stated, crc});
  }

  std::vector<std::byte> finish() {
    const auto directoryStart = (uint32_t)m_bytes.size();
    for (const Record& record : m_directory) {
      put32(0x02014b50);
      put16(20);
      put16(20);
      put16(0);
      put16(0);
      put16(0);
      put16(0);
      put32(record.crc);
      put32(record.stored);
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
    uint32_t stored;
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
  void putName(std::string_view name) {
    for (char letter : name) m_bytes.push_back((std::byte)letter);
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

}  // namespace

TEST(IOArchive, AnswersEveryFileItHoldsByTheNameItListsThemUnder) {
  ZipWriter zip;
  zip.add("brush.json", textBytes("{\"width\":3}"));
  zip.add("art/shape.png", textBytes("not really a png"));
  const std::vector<std::byte> bytes = zip.finish();

  ASSERT_TRUE(ArchiveSource::isArchive(bytes));
  ArchiveSource archive(bytes);
  ASSERT_EQ(archive.size(), 2u);
  // The order is the archive's own.
  EXPECT_EQ(archive.entries()[0].name, "brush.json");
  EXPECT_EQ(archive.entries()[1].name, "art/shape.png");

  const std::shared_ptr<const Bytes> described = archive.fetch("brush.json");
  ASSERT_NE(described, nullptr);
  EXPECT_EQ(described->asText(), "{\"width\":3}");
  EXPECT_EQ(archive.fetch("art/shape.png")->asText(), "not really a png");
  // A name it does not hold is null, as any source answers a URI it
  // cannot serve — including the same name spelled without its path.
  EXPECT_EQ(archive.fetch("shape.png"), nullptr);
  EXPECT_EQ(archive.fetch(""), nullptr);
}

TEST(IOArchive, RefusesAnEntryThatClaimsMoreThanCouldBeThere) {
  // The directory is a CLAIM: this one says a gigabyte lives in a file
  // of a few hundred bytes. Sizing a buffer from it before reading a
  // byte is how a two-hundred-byte file takes a gigabyte of memory.
  ZipWriter zip;
  zip.add("greedy.bin", textBytes("tiny"), /*claimed=*/1u << 30);
  zip.add("honest.txt", textBytes("here"));
  const std::vector<std::byte> bytes = zip.finish();
  ASSERT_LT(bytes.size(), 1024u);

  ArchiveSource archive(bytes);
  EXPECT_EQ(archive.fetch("greedy.bin"), nullptr);
  // …and the file beside it still arrives: one hostile claim does not
  // cost the archive its other entries.
  ASSERT_NE(archive.fetch("honest.txt"), nullptr);
  EXPECT_EQ(archive.fetch("honest.txt")->asText(), "here");
}

TEST(IOArchive, BytesThatAreNotAnArchiveHoldNothing) {
  EXPECT_FALSE(ArchiveSource::isArchive({}));
  EXPECT_FALSE(ArchiveSource::isArchive(textBytes("PK")));
  EXPECT_FALSE(ArchiveSource::isArchive(textBytes("not an archive at all")));

  const ArchiveSource empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.fetch("anything"), nullptr);

  const ArchiveSource notAnArchive{textBytes("not an archive at all")};
  EXPECT_TRUE(notAnArchive.empty());

  // A local file header and nothing behind it is not a readable archive
  // either, and answering nothing is the whole of what it costs.
  const ArchiveSource truncated{textBytes("PK\3\4nothing follows")};
  EXPECT_TRUE(truncated.empty());
}
