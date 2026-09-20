/** @file
 * The still writer: that the PNG lands, that it lands somewhere other
 * than the thread that asked, that the spent stills go with it, and that
 * the caller's own pixels may go the moment it has asked.
 */

#include <gtest/gtest.h>
#include <include/codec/SkCodec.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <sigilsketch/plate/ThumbnailWriter.h>
#include <sigilsketch/plate/Thumbnails.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "ScratchDir.h"

namespace {

using sigil::sketch::ThumbnailWriter;
using sigil::test::ScratchDir;

constexpr int kWidth = 24;
constexpr int kHeight = 16;

/** A still of one flat colour, which is what every case here hands
 *  over: the subject is the write, not what was drawn. */
SkBitmap flatStill(SkColor colour) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  bitmap.eraseColor(colour);
  bitmap.setImmutable();
  return bitmap;
}

/** What one finished write reported, and on which thread. */
struct Reports {
  std::mutex mutex;
  std::vector<int> indices;
  std::vector<std::thread::id> threads;

  ThumbnailWriter::Wrote sink() {
    return [this](int index) {
      const std::lock_guard lock(mutex);
      indices.push_back(index);
      threads.push_back(std::this_thread::get_id());
    };
  }
};

SkISize decodedSize(const std::filesystem::path& png) {
  std::ifstream file(png, std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
  std::unique_ptr<SkCodec> codec =
      SkCodec::MakeFromData(SkData::MakeWithCopy(bytes.data(), bytes.size()));
  return codec ? codec->dimensions() : SkISize::MakeEmpty();
}

TEST(SketchThumbnailWriter, WritesTheStillAndReportsIt) {
  const ScratchDir store("sigil_thumbnail_writer_wrote");
  const std::filesystem::path out =
      sigil::sketch::thumbnailFile(store.path, "probe", "key");
  Reports reports;
  {
    ThumbnailWriter writer(reports.sink());
    writer.write(7, flatStill(SK_ColorGREEN), out, store.path, "probe");
    writer.drain();
  }
  EXPECT_TRUE(std::filesystem::exists(out));
  EXPECT_EQ(decodedSize(out), SkISize::Make(kWidth, kHeight));
  ASSERT_EQ(reports.indices.size(), 1u);
  EXPECT_EQ(reports.indices.front(), 7);
}

TEST(SketchThumbnailWriter, RunsOffTheCallersThread) {
  const ScratchDir store("sigil_thumbnail_writer_thread");
  const std::filesystem::path out =
      sigil::sketch::thumbnailFile(store.path, "probe", "key");
  Reports reports;
  {
    ThumbnailWriter writer(reports.sink());
    writer.write(0, flatStill(SK_ColorRED), out, store.path, "probe");
    writer.drain();
  }
  // The whole point of the writer: a frame that hands a still over does
  // not pay for the encode.
  ASSERT_EQ(reports.threads.size(), 1u);
  EXPECT_NE(reports.threads.front(), std::this_thread::get_id());
}

TEST(SketchThumbnailWriter, PrunesTheSpentStillsUnderTheStem) {
  const ScratchDir store("sigil_thumbnail_writer_prune");
  const std::filesystem::path spent =
      sigil::sketch::thumbnailFile(store.path, "probe", "oldkey");
  std::filesystem::create_directories(spent.parent_path());
  std::ofstream(spent, std::ios::binary) << "a still of an older source";
  ASSERT_TRUE(std::filesystem::exists(spent));
  const std::filesystem::path out =
      sigil::sketch::thumbnailFile(store.path, "probe", "newkey");
  {
    ThumbnailWriter writer;
    writer.write(3, flatStill(SK_ColorBLUE), out, store.path, "probe");
    writer.drain();
  }
  EXPECT_TRUE(std::filesystem::exists(out));
  EXPECT_FALSE(std::filesystem::exists(spent));
}

TEST(SketchThumbnailWriter, TheCallersBitmapMayGoAtOnce) {
  const ScratchDir store("sigil_thumbnail_writer_moved");
  const std::filesystem::path out =
      sigil::sketch::thumbnailFile(store.path, "probe", "key");
  ThumbnailWriter writer;
  {
    SkBitmap still = flatStill(SK_ColorYELLOW);
    writer.write(1, std::move(still), out, store.path, "probe");
  }
  writer.drain();
  EXPECT_TRUE(std::filesystem::exists(out));
  EXPECT_EQ(decodedSize(out), SkISize::Make(kWidth, kHeight));
}

TEST(SketchThumbnailWriter, AnAskWithNoPixelsIsDropped) {
  const ScratchDir store("sigil_thumbnail_writer_empty");
  const std::filesystem::path out =
      sigil::sketch::thumbnailFile(store.path, "probe", "key");
  Reports reports;
  {
    ThumbnailWriter writer(reports.sink());
    writer.write(2, SkBitmap{}, out, store.path, "probe");
    writer.drain();
  }
  EXPECT_FALSE(std::filesystem::exists(out));
  EXPECT_TRUE(reports.indices.empty());
}

}  // namespace
