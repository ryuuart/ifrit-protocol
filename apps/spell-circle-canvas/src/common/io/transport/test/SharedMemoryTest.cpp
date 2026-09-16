/** @file
 * The shared memory transport: the messages a writer leaves in a region
 * and the arrivals a feed reading that region makes of them, the region
 * a feed names itself by and the way back it does not have, what a
 * writer does with a message too large for the region it made, the
 * name a feed waits at until somebody makes a region under it and the
 * one it follows to the region made under it next, and the promise the
 * whole layout stands on — that a message is never read half written.
 */

#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using sigil::io::Arrival;
using sigil::io::Feed;
using sigil::io::Hub;
using sigil::io::SharedMemoryWriter;
using namespace std::chrono_literals;

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on the transport's own thread, so there is nothing here to pump
 *  — only a moment to give it, and a deadline long enough that a loaded
 *  machine is not mistaken for a region nobody is reading. */
bool waitUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (ready()) return true;
    std::this_thread::sleep_for(1ms);
  }
  return ready();
}

std::vector<std::byte> bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  return {first, first + text.size()};
}

/** A region name no other run can be holding, and short: a shared
 *  memory name is a handful of characters on some systems, so it is
 *  spelled tight rather than after the case — the process it runs in,
 *  and one letter for the case itself. */
std::string regionName(std::string_view mark) {
  return "sigil-shm-" + std::string(mark) + std::to_string((long)::getpid());
}

/** The rate every case reads at. A case is over in the time a look
 *  takes, so it looks often; what the rate means is what the default
 *  means, one number apart. */
const char* kFast = "?rate=1000";

/** What a region is made to hold where the case does not care. */
constexpr size_t kRoom = 4096;

/** WHAT A REGION CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub that has
 *  been taught the scheme. Both ends stand in this binary — the writer
 *  is the library's own, so a case proves the reader against the writer
 *  the layout is stated by. */
class IOSharedMemory : public ::testing::Test {
 protected:
  IOSharedMemory() { sigil::io::registerTransports(hub); }

  Hub hub;
};

TEST_F(IOSharedMemory, AFeedTakesEveryMessageTheWriterLeavesInTheRegion) {
  const std::string name = regionName("a");
  SharedMemoryWriter writer(name, kRoom);
  ASSERT_TRUE(writer.open());

  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  ASSERT_TRUE(region->error().empty()) << region->error();

  // A region holds the message standing NOW, so each is waited for
  // before the next is written: what a reader that looked too slowly
  // missed is gone, not queued.
  const std::string_view said[] = {"the first sky", "the second sky",
                                   "the third sky"};
  uint64_t written = 0;
  for (const std::string_view line : said) {
    ASSERT_TRUE(writer.write(bytesOf(line)));
    ++written;
    ASSERT_TRUE(waitUntil([&] { return region->generation() == written; }))
        << "message " << written << " never arrived";
  }

  for (const std::string_view line : said) {
    const std::optional<Arrival> arrival = region->receive();
    ASSERT_TRUE(arrival.has_value());
    EXPECT_EQ(arrival->bytes->asText(), line);
    // The region is the sender: there is no other end to name.
    EXPECT_EQ(arrival->from, "shm://" + name);
  }
  EXPECT_FALSE(region->receive().has_value());
}

TEST_F(IOSharedMemory, TheSameMessageWrittenTwiceArrivesTwice) {
  const std::string name = regionName("b");
  SharedMemoryWriter writer(name, kRoom);
  ASSERT_TRUE(writer.open());

  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  ASSERT_TRUE(region->error().empty()) << region->error();

  // What makes a message new is the count of the messages written, not
  // what one says, so a sender repeating itself is heard both times.
  ASSERT_TRUE(writer.write(bytesOf("no change")));
  ASSERT_TRUE(waitUntil([&] { return region->generation() == 1; }));
  ASSERT_TRUE(writer.write(bytesOf("no change")));
  ASSERT_TRUE(waitUntil([&] { return region->generation() == 2; }));

  const std::optional<Arrival> first = region->receive();
  const std::optional<Arrival> second = region->receive();
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first->bytes->asText(), "no change");
  EXPECT_EQ(second->bytes->asText(), "no change");
  EXPECT_EQ(second->generation, first->generation + 1);
}

TEST_F(IOSharedMemory, AFeedNamesItsRegionAndHasNoWayBackToTheWriter) {
  const std::string name = regionName("c");
  SharedMemoryWriter writer(name, kRoom);
  ASSERT_TRUE(writer.open());

  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  ASSERT_TRUE(region->error().empty()) << region->error();
  // The rate is the reader's own arrangement and no part of what the
  // region is called.
  EXPECT_EQ(region->address(), "shm://" + name);
  // A reader maps what a writer left and has nothing to write back
  // through, either to everybody or to the one that wrote.
  EXPECT_FALSE(region->send({bytesOf("no way back")}));
  EXPECT_FALSE(region->sendTo(region->address(), {bytesOf("no way back")}));
}

TEST_F(IOSharedMemory, AMessageLargerThanTheRegionIsRefusedByTheWriter) {
  const std::string name = regionName("d");
  constexpr size_t kNarrow = 16;
  SharedMemoryWriter writer(name, kNarrow);
  ASSERT_TRUE(writer.open());

  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  ASSERT_TRUE(region->error().empty()) << region->error();

  ASSERT_TRUE(writer.write(bytesOf("sixteen bytes...")));
  ASSERT_TRUE(waitUntil([&] { return region->generation() == 1; }));

  // A message is written whole or not at all, so one that does not fit
  // is not written at all: the next message that does fit is the next
  // arrival, with nothing of the refused one between them.
  EXPECT_FALSE(writer.write(bytesOf("seventeen bytes..")));
  ASSERT_TRUE(writer.write(bytesOf("still sixteen!!!")));
  ASSERT_TRUE(waitUntil([&] { return region->generation() == 2; }));
  EXPECT_EQ(region->latest()->asText(), "still sixteen!!!");
}

TEST_F(IOSharedMemory,
       ARegionNobodyHasMadeYetIsADoorOntoNothingAndNotAFailure) {
  const std::string name = regionName("e");
  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  // A door holds the name and not the memory behind it, so a name
  // nothing stands under is a door waiting rather than one that failed:
  // it names the region it is waiting for and nothing is wrong with it.
  EXPECT_TRUE(region->error().empty()) << region->error();
  EXPECT_EQ(region->address(), "shm://" + name);
  EXPECT_EQ(region->latest(), nullptr);
  EXPECT_FALSE(region->closed());
}

TEST_F(IOSharedMemory, AWriterThatStartsAfterTheFeedIsOneTheFeedReads) {
  const std::string name = regionName("h");
  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  ASSERT_TRUE(region->error().empty()) << region->error();

  // THE TWO ENDS MAY START IN EITHER ORDER: the door looks for the name
  // again at every look it has nothing mapped for, so the region this
  // writer makes now is the region it reads.
  SharedMemoryWriter writer(name, kRoom);
  ASSERT_TRUE(writer.open());
  ASSERT_TRUE(writer.write(bytesOf("the sky that came late")));

  ASSERT_TRUE(waitUntil([&] { return region->generation() == 1; }))
      << "a region made after the feed opened was never read";
  EXPECT_EQ(region->latest()->asText(), "the sky that came late");
  EXPECT_TRUE(region->error().empty()) << region->error();
}

TEST_F(IOSharedMemory, ARegionMadeAgainUnderTheSameNameIsTheOneReadFromThenOn) {
  const std::string name = regionName("i");
  auto first = std::make_unique<SharedMemoryWriter>(name, kRoom);
  ASSERT_TRUE(first->open());

  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  ASSERT_TRUE(first->write(bytesOf("the writer that was here first")));
  ASSERT_TRUE(waitUntil([&] { return region->generation() == 1; }));

  // A writer started again takes the name back and makes its own region
  // under it, which is another object wearing that word. What the
  // reader holds is the name, so it maps the one standing now and reads
  // that region's messages rather than the unlinked one's.
  first.reset();
  SharedMemoryWriter second(name, kRoom);
  ASSERT_TRUE(second.open());
  ASSERT_TRUE(second.write(bytesOf("the writer that came after it")));

  ASSERT_TRUE(waitUntil([&] { return region->generation() == 2; }))
      << "the region made again was never read";
  EXPECT_EQ(region->latest()->asText(), "the writer that came after it");
}

TEST_F(IOSharedMemory, AUriThatNamesNoRegionOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> nothing = hub.feed("shm://");
  EXPECT_FALSE(nothing->error().empty());
  // A rate is a whole number of looks a second, of which none is no
  // rate at all, and a name is one segment.
  EXPECT_FALSE(
      hub.feed("shm://" + regionName("f") + "?rate=0")->error().empty());
  EXPECT_FALSE(hub.feed("shm://a/b")->error().empty());
}

TEST_F(IOSharedMemory, AMessageIsNeverSeenHalfWritten) {
  const std::string name = regionName("g");
  // Wide enough that a copy of one takes long enough to be overtaken by
  // a writer that never pauses, which is the case this is: a narrower
  // message would prove the protocol only by being too quick to tear.
  constexpr size_t kWide = 1 << 16;
  SharedMemoryWriter writer(name, kWide);
  ASSERT_TRUE(writer.open());

  const std::shared_ptr<Feed> region = hub.feed("shm://" + name + kFast);
  ASSERT_TRUE(region->error().empty()) << region->error();

  std::atomic<bool> writing{true};
  std::thread hand([&] {
    std::vector<std::byte> message(kWide);
    unsigned int mark = 0;
    while (writing.load(std::memory_order_relaxed)) {
      // ONE VALUE REPEATED, a different one each time: a copy that took
      // part of one message and part of the next holds two values, and
      // is caught by reading nothing but the message itself.
      std::fill(message.begin(), message.end(),
                std::byte{(unsigned char)(mark++ & 0xFF)});
      writer.write(message);
      // A breath between writes, far shorter than a look: a writer that
      // never pauses can overtake every copy the reader makes, and what
      // this case asserts is that a copy is never torn, which needs
      // copies to exist.
      std::this_thread::sleep_for(std::chrono::microseconds(20));
    }
  });

  size_t arrivals = 0;
  const auto until = std::chrono::steady_clock::now() + 500ms;
  while (std::chrono::steady_clock::now() < until) {
    while (const std::optional<Arrival> arrival = region->receive()) {
      const std::vector<std::byte>& message = arrival->bytes->bytes;
      ASSERT_EQ(message.size(), kWide);
      const std::byte one = message.front();
      ASSERT_TRUE(std::all_of(message.begin(), message.end(),
                              [one](std::byte each) { return each == one; }))
          << "arrival " << arrivals << " holds more than one message";
      ++arrivals;
    }
    std::this_thread::sleep_for(1ms);
  }

  writing.store(false, std::memory_order_relaxed);
  hand.join();
  // A case that read nothing has proved nothing.
  EXPECT_NE(arrivals, 0u);
}

}  // namespace
