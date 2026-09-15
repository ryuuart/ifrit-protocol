/** @file
 * The serial transport: the lines a board's port hands over and the
 * newline that is no part of them, the carriage return a line does not
 * carry, the line that arrived in two pieces, the blank line that is no
 * reading, the line a scene writes back, what a URI nobody can open
 * leaves on its feed, and the port a feed gives back when the last
 * holder lets go.
 *
 * BOTH ENDS OF A CABLE IN ONE BINARY. A case makes a pseudo-terminal
 * pair rather than waiting for a board to be plugged in: the feed opens
 * the slave by the path the system named it, exactly as it would open a
 * device file, and the case writes the readings into the master. The
 * master is put in raw mode so that what the case writes is what the
 * port reads — a line discipline left as it comes would echo the bytes
 * back and turn the newlines into something else.
 */

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilio/transport/Transport.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {

using sigil::io::Arrival;
using sigil::io::Bytes;
using sigil::io::Feed;
using sigil::io::Hub;
using namespace std::chrono_literals;

/** Polls @p ready until it holds, or gives up. The work it waits for
 *  runs on the transport's own thread, so there is nothing here to pump
 *  — only a moment to give it, and a deadline long enough that a loaded
 *  machine is not mistaken for a cable that says nothing. */
bool waitUntil(const std::function<bool()>& ready) {
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (std::chrono::steady_clock::now() < deadline) {
    if (ready()) return true;
    std::this_thread::sleep_for(1ms);
  }
  return ready();
}

Bytes bytesOf(std::string_view text) {
  const auto* const first = reinterpret_cast<const std::byte*>(text.data());
  Bytes out;
  out.bytes.assign(first, first + text.size());
  return out;
}

/** WHAT A SERIAL CASE NEEDS BEFORE IT CAN OPEN ANYTHING: a hub that has
 *  been taught the scheme, and a pseudo-terminal pair standing in for
 *  the board — the slave being a port with a path, which is the whole
 *  of what this transport asks of a device. */
class IOSerial : public ::testing::Test {
 protected:
  IOSerial() { sigil::io::registerTransports(hub); }

  void SetUp() override {
    m_master = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_master < 0)
      GTEST_SKIP() << "this machine offers no pseudo-terminal pair: "
                   << std::strerror(errno);
    if (::grantpt(m_master) != 0 || ::unlockpt(m_master) != 0)
      GTEST_SKIP() << "this machine would not hand over a pseudo-terminal: "
                   << std::strerror(errno);
    const char* const named = ::ptsname(m_master);
    if (named == nullptr)
      GTEST_SKIP() << "this machine gave the pseudo-terminal no name: "
                   << std::strerror(errno);
    m_device = named;
  }

  void TearDown() override {
    if (m_master >= 0) ::close(m_master);
  }

  /** The device the pair named, opened at a rate a board speaks. */
  std::string uri(std::string_view settings = "?baud=115200") const {
    return "serial://" + m_device + std::string(settings);
  }

  /** The feed on the pair's slave, with the line discipline between the
   *  two ends put in raw mode once it stands.
   *
   *  A PSEUDO-TERMINAL ANSWERS NO TERMIOS CALL UNTIL ITS SLAVE IS OPEN,
   *  and opening the feed is what opens it, so the two steps are one and
   *  in this order. Raw is what makes the bytes this case writes the
   *  bytes the port reads: a discipline left as it comes echoes them
   *  back to the writer and turns a newline into a pair. */
  std::shared_ptr<Feed> openSensor(std::string_view settings = "?baud=115200") {
    const std::shared_ptr<Feed> sensor = hub.feed(uri(settings));
    if (!sensor->error().empty()) return sensor;
    termios line{};
    EXPECT_EQ(::tcgetattr(m_master, &line), 0) << std::strerror(errno);
    ::cfmakeraw(&line);
    EXPECT_EQ(::tcsetattr(m_master, TCSANOW, &line), 0) << std::strerror(errno);
    return sensor;
  }

  /** @p text onto the wire, whole: a master that has no room now takes
   *  the rest of it in a moment. */
  void toMaster(std::string_view text) {
    size_t written = 0;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (written != text.size() &&
           std::chrono::steady_clock::now() < deadline) {
      const ssize_t count =
          ::write(m_master, text.data() + written, text.size() - written);
      if (count > 0)
        written += (size_t)count;
      else
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_EQ(written, text.size()) << std::strerror(errno);
  }

  /** Everything the master has read by the time @p wanted stands whole
   *  in it, or by the deadline. */
  std::string fromMaster(std::string_view wanted) {
    std::string got;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
      std::array<char, 256> chunk{};
      const ssize_t count = ::read(m_master, chunk.data(), chunk.size());
      if (count > 0)
        got.append(chunk.data(), (size_t)count);
      else
        std::this_thread::sleep_for(1ms);
      if (got.find(wanted) != std::string::npos) break;
    }
    return got;
  }

  /** Whether the master has seen the other end go: a read that answers
   *  no bytes at all, or one that fails with anything but the emptiness
   *  a port nobody has written to answers. */
  bool masterSawHangup() {
    return waitUntil([this] {
      std::array<char, 64> chunk{};
      const ssize_t count = ::read(m_master, chunk.data(), chunk.size());
      if (count == 0) return true;
      return count < 0 && errno != EAGAIN && errno != EWOULDBLOCK;
    });
  }

  Hub hub;
  int m_master = -1;
  std::string m_device;
};

TEST_F(IOSerial, TwoLinesAreTwoArrivalsInOrderWithoutTheirNewlines) {
  const std::shared_ptr<Feed> sensor = openSensor();
  ASSERT_TRUE(sensor->error().empty()) << sensor->error();
  EXPECT_EQ(sensor->address(), "serial://" + m_device);

  toMaster("{\"lux\":412}\n{\"tilt\":-3.2}\n");
  ASSERT_TRUE(waitUntil([&] { return sensor->generation() == 2; }));

  const std::optional<Arrival> first = sensor->receive();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->bytes->asText(), "{\"lux\":412}");
  // The sender is the port the line came off, spelled the way a URI
  // that opens it is — the settings this end was told to read it at
  // being no part of what the board IS.
  EXPECT_EQ(first->from, "serial://" + m_device);

  const std::optional<Arrival> second = sensor->receive();
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->bytes->asText(), "{\"tilt\":-3.2}");
}

TEST_F(IOSerial, ACarriageReturnBeforeTheNewlineIsNoPartOfTheLine) {
  const std::shared_ptr<Feed> sensor = openSensor();
  ASSERT_TRUE(sensor->error().empty()) << sensor->error();

  // A board that ends its lines the way a terminal does writes both,
  // and what a reader wants is the reading without either.
  toMaster("{\"tilt\":-3.2}\r\n");
  ASSERT_TRUE(waitUntil([&] { return sensor->latest() != nullptr; }));
  EXPECT_EQ(sensor->latest()->asText(), "{\"tilt\":-3.2}");
}

TEST_F(IOSerial, ALineThatArrivedInTwoWritesIsOneArrival) {
  const std::shared_ptr<Feed> sensor = openSensor();
  ASSERT_TRUE(sensor->error().empty()) << sensor->error();

  // Nothing on a serial port says where a message ends but the newline,
  // so half a line is not half an arrival: it is no arrival at all, and
  // the pause here is what makes the port read the two pieces apart.
  toMaster("{\"lux\":6");
  std::this_thread::sleep_for(50ms);
  EXPECT_EQ(sensor->latest(), nullptr);

  toMaster("40,\"tilt\":1.5}\n");
  ASSERT_TRUE(waitUntil([&] { return sensor->latest() != nullptr; }));
  EXPECT_EQ(sensor->latest()->asText(), "{\"lux\":640,\"tilt\":1.5}");
  EXPECT_EQ(sensor->generation(), 1u);
}

TEST_F(IOSerial, ABlankLineIsNoReadingAndDoesNotArrive) {
  const std::shared_ptr<Feed> sensor = openSensor();
  ASSERT_TRUE(sensor->error().empty()) << sensor->error();

  // A board that writes a blank line between its readings, or one that
  // ends the last one twice, has said nothing: a reader handed the
  // blank would read every field of it as missing.
  toMaster("\n\n{\"lux\":10}\n\n");
  ASSERT_TRUE(waitUntil([&] { return sensor->latest() != nullptr; }));
  EXPECT_EQ(sensor->latest()->asText(), "{\"lux\":10}");
  EXPECT_EQ(sensor->generation(), 1u);
}

TEST_F(IOSerial, SendWritesALineTheOtherEndReadsBack) {
  const std::shared_ptr<Feed> sensor = openSensor();
  ASSERT_TRUE(sensor->error().empty()) << sensor->error();

  // A cable holds the one peer at the other end of it, so a send goes
  // there and there is nobody else to name.
  EXPECT_TRUE(sensor->send(bytesOf("{\"led\":true}")));
  EXPECT_FALSE(sensor->sendTo(sensor->address(), bytesOf("by name")));

  // A MESSAGE IS A LINE BOTH WAYS: the reader on the board stops at the
  // newline this end wrote after the bytes.
  EXPECT_EQ(fromMaster("\n"), "{\"led\":true}\n");
}

TEST_F(IOSerial, ADeviceThatIsNotThereOpensNothingAndSaysWhy) {
  const std::shared_ptr<Feed> feed =
      hub.feed("serial:///dev/tty.no-board-of-this-name?baud=115200");
  EXPECT_FALSE(feed->error().empty());
  EXPECT_TRUE(feed->address().empty());
  EXPECT_EQ(feed->latest(), nullptr);
}

TEST_F(IOSerial, AUriWithNoBaudRateIsRefusedWithTheReason) {
  // Two ends that disagree about the rate read each other as noise, so
  // there is no rate to fall back on and the URI has to carry one.
  const std::shared_ptr<Feed> feed = hub.feed(uri(""));
  EXPECT_FALSE(feed->error().empty());
  EXPECT_NE(feed->error().find("baud"), std::string::npos) << feed->error();
  EXPECT_TRUE(feed->address().empty());
}

TEST_F(IOSerial, DroppingTheLastHolderOfAFeedClosesThePort) {
  {
    const std::shared_ptr<Feed> sensor = openSensor();
    ASSERT_TRUE(sensor->error().empty()) << sensor->error();
    toMaster("{\"lux\":1}\n");
    ASSERT_TRUE(waitUntil([&] { return sensor->latest() != nullptr; }));
  }

  // The close travels to the transport's thread, so the port goes a
  // moment after the last holder lets go rather than within it — and
  // the other end of the pair sees that nothing holds it any more.
  EXPECT_TRUE(masterSawHangup());
}

TEST_F(IOSerial, TwoAsksForOneUriAnswerOneFeed) {
  const std::shared_ptr<Feed> first = openSensor();
  ASSERT_TRUE(first->error().empty()) << first->error();
  const std::shared_ptr<Feed> second = hub.feed(uri());
  EXPECT_EQ(first, second);
  EXPECT_EQ(first->address(), second->address());
}

}  // namespace
