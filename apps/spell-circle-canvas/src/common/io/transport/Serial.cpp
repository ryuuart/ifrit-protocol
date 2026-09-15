/** @file
 * The SERIAL transport: the device a feed's URI names, the settings the
 * wire is read at, the port opened on them, and the lines that come off
 * it.
 *
 * A BOARD ON A CABLE PRINTS LINES. What reaches a serial port is a run
 * of bytes with no message boundary anywhere in it, so the boundary
 * this transport takes is the one the sender writes: a newline. Every
 * arrival is one line with the newline left off, and a line that has
 * only half arrived is nothing yet — the bytes wait for the rest of
 * their line rather than reaching a reader as a reading with its
 * numbers cut in two.
 *
 * A MESSAGE IS A LINE BOTH WAYS: send() writes the bytes and a newline
 * after them, which is where the reader on the board stops reading.
 *
 * What a line MEANS is not decided here: a feed answers bytes, and the
 * library that owns the format reads them.
 */

#include <array>
#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/asio/serial_port_base.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <charconv>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "IoThread.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

using boost::system::error_code;
using BaudRate = boost::asio::serial_port_base::baud_rate;
using CharacterSize = boost::asio::serial_port_base::character_size;
using FlowControl = boost::asio::serial_port_base::flow_control;
using Parity = boost::asio::serial_port_base::parity;
using StopBits = boost::asio::serial_port_base::stop_bits;

/** How much of the wire one read takes. A reading is tens of bytes, so
 *  this is a page that is never full rather than a size anything
 *  depends on: what a read does not take stays on the port and is taken
 *  by the next one. */
constexpr size_t kReadChunk = 4096;

/** WHERE A LINE STOPS BEING A LINE. A sender that writes no newline at
 *  all would otherwise grow one arrival without end, so this many bytes
 *  with no newline among them are handed over as the line they stand as
 *  and the buffer begins again: a wire nobody framed still reaches a
 *  reader, and a reader can see how long what it got was. */
constexpr size_t kLineCeiling = 64 * 1024;

/** The scheme, with the separator that always follows it. */
constexpr std::string_view kScheme = "serial://";

/** WHAT A SERIAL URI NAMES: the device file the cable stands behind,
 *  and the settings the wire is read at. */
struct Wire {
  std::string device;
  unsigned int baud = 0;
  unsigned int bits = 8;
  Parity::type parity = Parity::none;
  StopBits::type stop = StopBits::one;
  FlowControl::type flow = FlowControl::none;
};

/** A wire, or the sentence saying why a URI named none: exactly one of
 *  the two stands. */
struct ReadWire {
  Wire wire;
  std::string trouble;
};

/** @p digits as a whole number, or nothing when they are not decimal
 *  digits and nothing else. */
std::optional<unsigned int> wholeNumber(std::string_view digits) {
  unsigned int value = 0;
  const char* const end = digits.data() + digits.size();
  const std::from_chars_result read =
      std::from_chars(digits.data(), end, value);
  if (read.ec != std::errc() || read.ptr != end) return std::nullopt;
  return value;
}

/** One setting of the query, written into @p wire; the sentence saying
 *  why it could not be, or empty when it was. */
std::string readSetting(Wire& wire, std::string_view name,
                        std::string_view value) {
  const std::string wrong = std::string(value) + " is no " + std::string(name);
  if (name == "baud") {
    const std::optional<unsigned int> rate = wholeNumber(value);
    // Zero is the rate that hangs a line up rather than one a board
    // speaks at, so a feed asking for it is asking for no readings.
    if (!rate || *rate == 0) return wrong + ": a rate is whole and above zero";
    wire.baud = *rate;
    return {};
  }
  if (name == "bits") {
    const std::optional<unsigned int> width = wholeNumber(value);
    if (!width || *width < 5 || *width > 8)
      return wrong + ": a character is 5, 6, 7 or 8 bits wide";
    wire.bits = *width;
    return {};
  }
  if (name == "parity") {
    if (value == "none")
      wire.parity = Parity::none;
    else if (value == "odd")
      wire.parity = Parity::odd;
    else if (value == "even")
      wire.parity = Parity::even;
    else
      return wrong + ": it is none, odd or even";
    return {};
  }
  if (name == "stop") {
    if (value == "1")
      wire.stop = StopBits::one;
    else if (value == "2")
      wire.stop = StopBits::two;
    else
      return wrong + " count: it is 1 or 2";
    return {};
  }
  if (name == "flow") {
    if (value == "none")
      wire.flow = FlowControl::none;
    else if (value == "software")
      wire.flow = FlowControl::software;
    else if (value == "hardware")
      wire.flow = FlowControl::hardware;
    else
      return wrong + " control: it is none, software or hardware";
    return {};
  }
  return std::string(name) +
         " is no setting of a serial port: they are baud, bits, parity, stop "
         "and flow";
}

/** The wire @p uri names, or the sentence saying why it names none.
 *  Everything between the scheme and the query is the device's own
 *  path, whole and absolute as every system that has one writes it, and
 *  the query is the settings the port is opened with. A RATE IS
 *  REQUIRED: two ends of a cable that disagree about it exchange bytes
 *  that are not the ones either of them wrote, so there is no rate to
 *  fall back on. */
ReadWire readWire(std::string_view uri) {
  if (!uri.starts_with(kScheme))
    return {.trouble = std::string(uri) +
                       " is not a serial address: a feed is opened on "
                       "serial:///dev/NAME?baud=RATE"};
  const std::string_view rest = uri.substr(kScheme.size());
  const size_t question = rest.find('?');
  const std::string_view device = rest.substr(0, question);
  if (device.empty() || device.front() != '/')
    return {.trouble = std::string(uri) +
                       " names no device: the port's own path stands whole "
                       "after the scheme, as in "
                       "serial:///dev/tty.usbmodem1101?baud=115200"};

  Wire wire;
  wire.device = std::string(device);
  std::string_view query = question == std::string_view::npos
                               ? std::string_view()
                               : rest.substr(question + 1);
  while (!query.empty()) {
    const size_t ampersand = query.find('&');
    const std::string_view setting = query.substr(0, ampersand);
    query = ampersand == std::string_view::npos ? std::string_view()
                                                : query.substr(ampersand + 1);
    if (setting.empty()) continue;
    const size_t equals = setting.find('=');
    if (equals == std::string_view::npos)
      return {.trouble = std::string(uri) + ": " + std::string(setting) +
                         " is not a setting, which is written name=value"};
    std::string trouble = readSetting(wire, setting.substr(0, equals),
                                      setting.substr(equals + 1));
    if (!trouble.empty())
      return {.trouble = std::string(uri) + ": " + std::move(trouble)};
  }
  if (wire.baud == 0)
    return {.trouble = std::string(uri) +
                       " names no baud rate: a port is opened on "
                       "serial:///dev/NAME?baud=RATE, because two ends that "
                       "disagree about the rate read each other as noise"};
  return {.wire = std::move(wire)};
}

/** THE TRANSPORT'S END OF ONE FEED: the port, the bytes one read lands
 *  in, the part of a line that has arrived, the lines still going out,
 *  and the feed the whole lines go to.
 *
 *  Every callback holds this, so a feed let go while a read is in
 *  flight leaves it standing until that callback returns. The feed
 *  itself is held weakly: when it cannot be locked there is nobody left
 *  to deliver to, and the loop ends there. */
struct Door : std::enable_shared_from_this<Door> {
  Door(std::shared_ptr<detail::IoThread> thread, std::string address,
       std::weak_ptr<Feed> feed)
      : io(std::move(thread)),
        strand(boost::asio::make_strand(io->context())),
        port(strand),
        address(std::move(address)),
        feed(std::move(feed)) {}

  /** Arms one read, which arms the next. */
  void receive();
  /** Hands over every whole line the buffer now holds. */
  void hand(Feed& into);
  void close();
  bool send(const Bytes& line);
  /** Writes the line at the front of the queue, and the ones after it. */
  void write();

  /** Held, not borrowed: the context has to outlive the port standing
   *  on it. */
  std::shared_ptr<detail::IoThread> io;
  boost::asio::strand<boost::asio::io_context::executor_type> strand;
  boost::asio::serial_port port;
  /** What the feed reports and every arrival names: the URI with its
   *  settings left off, which is what the sender IS rather than how
   *  this end was told to read it. */
  std::string address;
  std::weak_ptr<Feed> feed;
  std::array<std::byte, kReadChunk> buffer{};
  /** The bytes read and not yet ended by a newline; the strand's
   *  alone. */
  std::vector<std::byte> partial;
  /** The lines waiting to go out, the front one being written now; the
   *  strand's alone. */
  std::deque<std::vector<std::byte>> outgoing;
  /** Raised before the close is posted, so a callback the strand has
   *  already entered stops instead of re-arming a port on its way
   *  out. */
  std::atomic<bool> closed{false};
};

void Door::receive() {
  port.async_read_some(
      boost::asio::buffer(buffer),
      [self = shared_from_this()](const error_code& error, size_t count) {
        if (self->closed.load(std::memory_order_acquire)) return;
        const std::shared_ptr<Feed> feed = self->feed.lock();
        if (!feed) return;
        if (error) {
          // The cancel this door posts as it closes ends the read it
          // was waiting on: that is this end letting go, and not a
          // cable anybody has to be told about. Anything else ends the
          // port, and the feed is told in the system's own words why
          // nothing more will arrive.
          if (error != boost::asio::error::operation_aborted)
            feed->fail(error.message());
          return;
        }
        // The bytes leave the buffer before the next read is armed, and
        // the lines are handed over after it: the time a consumer takes
        // over an arrival is not time the port spends unable to read.
        self->partial.insert(self->partial.end(), self->buffer.begin(),
                             self->buffer.begin() + count);
        self->receive();
        self->hand(*feed);
      });
}

void Door::hand(Feed& into) {
  size_t opened = 0;
  for (size_t at = 0; at != partial.size(); ++at) {
    if (partial[at] != std::byte{'\n'}) continue;
    size_t ended = at;
    // A sender that ends its lines with a carriage return and a newline
    // both wrote the pair; the line is what stands in front of them.
    if (ended != opened && partial[ended - 1] == std::byte{'\r'}) --ended;
    // An empty line is a sender's blank rather than a reading, and a
    // reader handed one would read every field of it as missing.
    if (ended != opened) {
      Bytes line;
      line.bytes.assign(partial.begin() + (long)opened,
                        partial.begin() + (long)ended);
      into.deliver(std::move(line), address);
    }
    opened = at + 1;
  }
  partial.erase(partial.begin(), partial.begin() + (long)opened);
  if (partial.size() < kLineCeiling) return;
  Bytes unframed;
  unframed.bytes = std::move(partial);
  partial.clear();
  into.deliver(std::move(unframed), address);
}

void Door::close() {
  closed.store(true, std::memory_order_release);
  boost::asio::post(strand, [self = shared_from_this()] {
    error_code ignored;
    // Cancelled before it is closed, so a read waiting on the port ends
    // with its own error rather than on a descriptor already gone.
    self->port.cancel(ignored);
    self->port.close(ignored);
  });
}

bool Door::send(const Bytes& line) {
  if (closed.load(std::memory_order_acquire)) return false;
  // The port belongs to the strand, so the bytes travel there in a copy
  // of their own and the caller's Bytes are its own again as soon as
  // this returns.
  boost::asio::post(
      strand, [self = shared_from_this(), payload = line.bytes]() mutable {
        if (self->closed.load(std::memory_order_acquire)) return;
        payload.push_back(std::byte{'\n'});
        self->outgoing.push_back(std::move(payload));
        // One write is in flight at a time, and the one that finishes
        // starts the next: two overlapping writes on one port would
        // interleave two lines into bytes that are neither.
        if (self->outgoing.size() == 1) self->write();
      });
  return true;
}

void Door::write() {
  // The line stands where it is until the whole of it has gone out: a
  // port takes the room it has and no more, so what finishes a line is
  // the writes after the first rather than the first alone.
  boost::asio::async_write(
      port, boost::asio::buffer(outgoing.front()),
      [self = shared_from_this()](const error_code&, size_t) {
        if (self->closed.load(std::memory_order_acquire)) return;
        // A line the port refused is that line and not the door. What
        // ends a port is the read, which fails with the system's own
        // reason and leaves it on the feed.
        self->outgoing.pop_front();
        if (!self->outgoing.empty()) self->write();
      });
}

/** A feed whose transport could not open: the reason stands on the feed,
 *  and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's port: the device the URI names, at the settings it
 *  names, read from the first line on. */
OpenedFeed openFeed(const std::shared_ptr<detail::IoThread>& io,
                    std::string_view uri, const std::weak_ptr<Feed>& into) {
  ReadWire read = readWire(uri);
  if (!read.trouble.empty()) return refuse(into, std::move(read.trouble));

  const std::string address = std::string(kScheme) + read.wire.device;
  const auto door = std::make_shared<Door>(io, address, into);
  error_code error;
  door->port.open(read.wire.device, error);
  if (error)
    return refuse(
        into, "could not open " + read.wire.device + ": " + error.message());

  // EVERY SETTING OR NONE. A port read at a rate, a width or a parity
  // nobody asked for answers bytes that are not the ones on the wire,
  // which is worse than a door that says it did not open.
  door->port.set_option(BaudRate(read.wire.baud), error);
  if (!error) door->port.set_option(CharacterSize(read.wire.bits), error);
  if (!error) door->port.set_option(Parity(read.wire.parity), error);
  if (!error) door->port.set_option(StopBits(read.wire.stop), error);
  if (!error) door->port.set_option(FlowControl(read.wire.flow), error);
  if (error) {
    door->close();
    return refuse(into, read.wire.device + " would not take the settings in " +
                            std::string(uri) + ": " + error.message());
  }

  OpenedFeed opened;
  opened.address = address;
  opened.close = [door] { door->close(); };
  // A CABLE HOLDS ONE PEER AND IT IS TWO WAYS: what is at the other end
  // is the only thing there, so a send reaches it and there is no
  // sender to pick out by name.
  opened.send = [door](const Bytes& line) { return door->send(line); };

  // Armed last: the port belongs to the strand from the first read on,
  // so everything above runs while this thread is still the only one
  // that can touch it.
  door->receive();
  return opened;
}

}  // namespace

void registerSerial(Hub& hub) {
  // The thread is made when the first port opens, so a hub taught the
  // scheme and never asked for a feed on it starts nothing.
  auto shared = std::make_shared<detail::SharedIoThread>();
  hub.setFeedTransport(
      "serial", [shared](std::string_view uri, std::weak_ptr<Feed> into) {
        return openFeed(shared->acquire(), uri, into);
      });
}

}  // namespace sigil::io
