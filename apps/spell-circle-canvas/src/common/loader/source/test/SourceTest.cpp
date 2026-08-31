/** @file
 * The byte-source vocabulary on its own: a fixture source satisfies the
 * concepts, a decoder written against them is one, and AnyByteSource
 * erases a borrowed or a shared source without a hub in sight.
 */

#include <gtest/gtest.h>
#include <sigilloader/source/Source.h>

#include <map>
#include <memory>
#include <string>

using namespace sigil::loader;

namespace {

/** An in-memory source: URIs mapped to text, nothing on disk. */
struct TableSource {
  std::map<std::string, std::string, std::less<>> table;

  std::shared_ptr<const Bytes> fetch(std::string_view uri) {
    const auto it = table.find(uri);
    if (it == table.end()) return nullptr;
    auto blob = std::make_shared<Bytes>();
    for (const char c : it->second) blob->bytes.push_back((std::byte)c);
    return blob;
  }
};
static_assert(ByteSource<TableSource>);
static_assert(!ResolvingByteSource<TableSource>);

struct Length {
  size_t bytes = 0;
};
struct LengthDecoder {
  std::optional<Length> decode(const Bytes& bytes, std::string_view) const {
    return Length{bytes.bytes.size()};
  }
};
static_assert(Decoder<LengthDecoder, Length>);

}  // namespace

TEST(SourceVocabulary, BytesReadBackAsText) {
  Bytes bytes;
  for (const char c : std::string("carry the coal"))
    bytes.bytes.push_back((std::byte)c);
  EXPECT_EQ(bytes.asText(), "carry the coal");
  EXPECT_EQ(Bytes{}.asText(), "");
}

TEST(SourceVocabulary, AnyByteSourceBorrowsAndAnswersEmptyResolve) {
  TableSource table;
  table.table["mem://a"] = "alpha";
  AnyByteSource any(table);
  ASSERT_TRUE(any);
  auto fetched = any.fetch("mem://a");
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->asText(), "alpha");
  EXPECT_EQ(any.fetch("mem://missing"), nullptr);
  // A source with no resolve of its own answers an empty path.
  EXPECT_TRUE(any.resolve("mem://a").empty());
  EXPECT_FALSE(AnyByteSource{});
  EXPECT_EQ(AnyByteSource{}.fetch("mem://a"), nullptr);
}

TEST(SourceVocabulary, AnyByteSourceSharesOwnership) {
  auto table = std::make_shared<TableSource>();
  table->table["mem://b"] = "beta";
  AnyByteSource any(table);
  EXPECT_GT(table.use_count(), 1);
  table.reset();
  auto fetched = any.fetch("mem://b");
  ASSERT_NE(fetched, nullptr);
  EXPECT_EQ(fetched->asText(), "beta");
}

TEST(SourceVocabulary, DecoderRunsOverFetchedBytes) {
  TableSource table;
  table.table["mem://c"] = "gamma";
  const LengthDecoder decoder;
  auto bytes = table.fetch("mem://c");
  ASSERT_NE(bytes, nullptr);
  const auto length = decoder.decode(*bytes, "mem://c");
  ASSERT_TRUE(length.has_value());
  EXPECT_EQ(length->bytes, 5u);
}
