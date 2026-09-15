/** @file
 * The shared memory transport: the region a feed's URI names, the
 * read-only mapping opened over it, the timer that looks at that
 * mapping a whole number of times a second, and the writer's end
 * standing beside the reader.
 *
 * ONE WRITER AND ANY READERS, NEITHER WAITING FOR THE OTHER. A region
 * opens with a fixed header carrying a count of the messages written
 * into it. The writer raises that count to an odd number before it
 * touches the payload and to the next even one once the message is
 * whole; a reader copies the payload between two reads of the count and
 * keeps what it copied only when both reads are the same even number.
 * So a reader can never hand on half of one message and half of the
 * next, and a writer is never held up by a reader that is mid-copy.
 *
 * The count is what says a message is NEW, so the same bytes written
 * twice are two arrivals, and a reader that looked too slowly to see
 * one of them has missed it rather than queued it: a region holds the
 * message that stands now.
 *
 * POSIX IS THE PLATFORM HERE: shm_open, ftruncate and mmap are what a
 * region is made of and mapped with. Windows names the same things
 * differently and would carry a file of its own behind this same door,
 * the layout and the count being the whole of what the two ends of a
 * region agree on.
 */

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "IoThread.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

using boost::system::error_code;

/** How often a feed looks at its region where its URI names no rate.
 *  Fast enough that a scene drawing at the rate a screen refreshes sees
 *  each message on the frame after it was written, and slow enough that
 *  a region nobody is writing to costs one look a frame and nothing
 *  else. */
constexpr unsigned int kPollRate = 120;

/** WHAT EVERY REGION OPENS WITH, at fixed offsets and in the byte order
 *  of the machine both ends run on, so a writer in another language is
 *  a structure definition and not a library. The payload begins where
 *  this ends.
 *
 *  `sequence` is the count of messages written: odd while one is being
 *  written, even while one stands whole. `capacity` is how many payload
 *  bytes the region was made to hold, `size` how many of them the
 *  message standing now takes, and `written_at_nanoseconds` when it was
 *  put there, against the clock a wall clock keeps. */
struct Header {
  char magic[16];
  std::uint32_t capacity;
  std::uint32_t reserved;
  std::uint64_t sequence;
  std::uint64_t size;
  std::uint64_t writtenAtNanoseconds;
  /** The rest of the sixty-four bytes: a payload begins at one offset
   *  whatever a compiler would otherwise have padded this to, and the
   *  fields a writer and a reader hammer stand in a line of their own
   *  rather than in the one the payload's first bytes are in. */
  unsigned char padding[16];
};

static_assert(sizeof(Header) == 64,
              "a region's payload begins sixty-four bytes in, which is what a "
              "writer in another language spells as a number");
static_assert(offsetof(Header, capacity) == 16 &&
                  offsetof(Header, sequence) == 24 &&
                  offsetof(Header, size) == 32 &&
                  offsetof(Header, writtenAtNanoseconds) == 40,
              "every field of a region's header stands at a fixed offset, "
              "which is what both ends of it agree on");
static_assert(std::atomic_ref<std::uint64_t>::is_always_lock_free,
              "the count a region is read and written by is raised and read "
              "without a lock, there being no lock two processes share");

/** The sixteen bytes a region opens with, zero-filled past the name. */
constexpr char kMagic[16] = {'s', 'i', 'g', 'i', 'l', '-', 's',
                             'h', 'a', 'r', 'e', 'd', '-', '1'};

/** Where a region's payload begins: one header in, always. */
std::byte* payloadOf(Header* header) {
  return reinterpret_cast<std::byte*>(header) + sizeof(Header);
}

/** What the system says went wrong, in its own words. */
std::string systemMessage(int reason) {
  return std::error_code(reason, std::generic_category()).message();
}

/** The name a shared memory object is opened under: one segment, under
 *  the slash every one of them stands beneath. */
std::string objectName(std::string_view name) {
  return "/" + std::string(name);
}

/** Now, against the clock that means the same thing in two processes. */
std::uint64_t nowNanoseconds() {
  return (std::uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

/** WHAT A REGION URI NAMES: the object to map, and how often to look at
 *  it. */
struct Region {
  std::string name;
  unsigned int rate = kPollRate;
};

/** The region @p uri names, or nothing when it names none: everything
 *  after the scheme is the object's name, and a query behind it may
 *  name the rate to look at that region at and nothing else. A name is
 *  one segment, a shared memory object standing in no directory, and a
 *  rate is a whole number of looks a second, of which none is no rate
 *  at all. */
std::optional<Region> parseRegion(std::string_view uri) {
  constexpr std::string_view kOpening = "shm://";
  if (!uri.starts_with(kOpening)) return std::nullopt;
  std::string_view rest = uri.substr(kOpening.size());
  Region region;
  const size_t query = rest.find('?');
  if (query != std::string_view::npos) {
    constexpr std::string_view kRate = "rate=";
    const std::string_view named = rest.substr(query + 1);
    if (!named.starts_with(kRate)) return std::nullopt;
    const std::string_view digits = named.substr(kRate.size());
    unsigned int rate = 0;
    const char* const end = digits.data() + digits.size();
    const std::from_chars_result read =
        std::from_chars(digits.data(), end, rate);
    if (read.ec != std::errc() || read.ptr != end || rate == 0)
      return std::nullopt;
    region.rate = rate;
    rest = rest.substr(0, query);
  }
  if (rest.empty() || rest.find('/') != std::string_view::npos)
    return std::nullopt;
  region.name = std::string(rest);
  return region;
}

/** THE READER'S END OF ONE FEED: the mapping, the timer that looks at
 *  it, and the feed each message goes to.
 *
 *  Every callback holds this, so the mapping stands for as long as
 *  anything could still read it and is unmapped once the last callback
 *  has returned. The feed itself is held weakly: when it cannot be
 *  locked there is nobody left to deliver to, and the looking ends
 *  there. */
struct Door : std::enable_shared_from_this<Door> {
  Door(std::shared_ptr<detail::IoThread> thread, std::string named,
       std::chrono::nanoseconds every, std::weak_ptr<Feed> feed)
      : io(std::move(thread)),
        strand(boost::asio::make_strand(io->context())),
        timer(strand),
        address(std::move(named)),
        interval(every),
        feed(std::move(feed)) {}

  ~Door() {
    if (region != nullptr) ::munmap(region, length);
  }

  Door(const Door&) = delete;
  Door& operator=(const Door&) = delete;

  /** Arms one look, which arms the next. */
  void look();
  /** One look at the region: the message standing there, where it is
   *  whole and this feed has not had it. */
  void read(Feed& into);
  void close();

  /** Held, not borrowed: the context has to outlive the timer standing
   *  on it. */
  std::shared_ptr<detail::IoThread> io;
  boost::asio::strand<boost::asio::io_context::executor_type> strand;
  boost::asio::steady_timer timer;
  /** What the feed reports and every arrival is named by: the region,
   *  spelled as a URI of this scheme, without the rate — which is this
   *  reader's own arrangement and no part of what the region is
   *  called. */
  std::string address;
  std::chrono::nanoseconds interval;
  std::weak_ptr<Feed> feed;
  /** The mapping. It is read-only, so nothing here may store through
   *  it: a reader of a region cannot disturb the writer of one. */
  Header* region = nullptr;
  size_t length = 0;
  /** What the region was made to hold, read once as it was mapped. The
   *  size a message claims is measured against this rather than against
   *  the region's own field, so a claim can name no byte outside the
   *  mapping however the region is written to meanwhile. */
  size_t capacity = 0;
  /** The count the last delivered message was written under. */
  std::uint64_t delivered = 0;
  /** Raised before the close is posted, so a callback the strand has
   *  already entered stops instead of arming another look on its way
   *  out. */
  std::atomic<bool> closed{false};
};

void Door::read(Feed& into) {
  std::atomic_ref<std::uint64_t> written(region->sequence);
  const std::uint64_t before = written.load(std::memory_order_acquire);
  // An odd count is a message half written, and an unchanged one is the
  // message this feed already has: neither is anything to deliver.
  if ((before & 1) != 0 || before == delivered) return;
  const size_t claimed = (size_t)std::atomic_ref<std::uint64_t>(region->size)
                             .load(std::memory_order_relaxed);
  if (claimed > capacity) return;
  Bytes message;
  message.bytes.resize(claimed);
  if (claimed != 0)
    std::memcpy(message.bytes.data(), payloadOf(region), claimed);
  // The count is read again only once the copy is finished: what makes
  // the copy a whole message is that the count did not move across it,
  // and a copy the writer overtook is dropped rather than delivered in
  // pieces. The next look reads the message that overtook it.
  std::atomic_thread_fence(std::memory_order_acquire);
  if (written.load(std::memory_order_relaxed) != before) return;
  delivered = before;
  into.deliver(std::move(message), address);
}

void Door::look() {
  timer.expires_after(interval);
  timer.async_wait([self = shared_from_this()](const error_code& error) {
    // A cancelled wait is this door closing, and a closed door arms no
    // further look.
    if (error || self->closed.load(std::memory_order_acquire)) return;
    // A feed nobody holds is nobody to deliver to, so the looking ends
    // rather than reading a region for no one.
    const std::shared_ptr<Feed> into = self->feed.lock();
    if (!into) return;
    self->read(*into);
    self->look();
  });
}

void Door::close() {
  closed.store(true, std::memory_order_release);
  // The timer belongs to the strand, so the cancel travels there; a
  // look already in flight sees the flag above and stops on its own.
  boost::asio::post(strand,
                    [self = shared_from_this()] { self->timer.cancel(); });
}

/** A feed whose transport could not open: the reason stands on the
 *  feed, and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's region: the object the URI names, mapped read-only,
 *  checked for the bytes a region of this layout opens with, and looked
 *  at from then on at the rate the URI asked for. */
OpenedFeed openFeed(const std::shared_ptr<detail::IoThread>& io,
                    std::string_view uri, const std::weak_ptr<Feed>& into) {
  const std::optional<Region> named = parseRegion(uri);
  if (!named)
    return refuse(into,
                  std::string(uri) +
                      " is not a shared memory address: a feed is opened on "
                      "shm://name, and on shm://name?rate=hertz to look at "
                      "that region a whole number of times a second");

  // A region is read where its writer made it, so a name nothing has
  // written under is a door with nothing behind it.
  const int descriptor = ::shm_open(objectName(named->name).c_str(), O_RDONLY);
  if (descriptor < 0)
    return refuse(into, "could not open " + std::string(uri) + ": " +
                            systemMessage(errno));

  struct stat status = {};
  if (::fstat(descriptor, &status) != 0) {
    const std::string why = systemMessage(errno);
    ::close(descriptor);
    return refuse(into, "could not measure " + std::string(uri) + ": " + why);
  }
  const size_t length = status.st_size > 0 ? (size_t)status.st_size : 0;
  if (length < sizeof(Header)) {
    ::close(descriptor);
    return refuse(into, std::string(uri) +
                            " is smaller than the header a region opens with, "
                            "so there is no message in it to read");
  }

  void* const mapped =
      ::mmap(nullptr, length, PROT_READ, MAP_SHARED, descriptor, 0);
  const std::string trouble =
      mapped == MAP_FAILED ? systemMessage(errno) : std::string();
  // The mapping holds the region on its own, so the descriptor has
  // nothing left to do the moment the mapping is made.
  ::close(descriptor);
  if (mapped == MAP_FAILED)
    return refuse(into, "could not map " + std::string(uri) + ": " + trouble);

  const auto door = std::make_shared<Door>(
      io, "shm://" + named->name,
      std::chrono::nanoseconds(std::chrono::seconds(1)) / named->rate, into);
  // Kept from here on, so every way out of this function unmaps it.
  door->region = static_cast<Header*>(mapped);
  door->length = length;
  if (std::memcmp(door->region->magic, kMagic, sizeof(kMagic)) != 0)
    return refuse(into, std::string(uri) +
                            " does not open with the bytes a shared memory "
                            "region opens with, so it holds something else");
  door->capacity = door->region->capacity;
  if (door->capacity > length - sizeof(Header))
    return refuse(into, std::string(uri) +
                            " says it holds more payload than there is room "
                            "for in it, so it is not a region to read");

  OpenedFeed opened;
  opened.address = door->address;
  opened.close = [door] { door->close(); };
  // Neither way out is filled: a reader maps what a writer left and has
  // nothing to write back through, so a scene that must answer holds
  // another door for that.
  door->look();
  return opened;
}

}  // namespace

void registerSharedMemory(Hub& hub) {
  // The looks of every region opened through one registration share one
  // thread, made when the first of them opens.
  auto shared = std::make_shared<detail::SharedIoThread>();
  hub.setFeedTransport("shm",
                       [shared = std::move(shared)](std::string_view uri,
                                                    std::weak_ptr<Feed> into) {
                         return openFeed(shared->acquire(), uri, into);
                       });
}

SharedMemoryWriter::SharedMemoryWriter(std::string_view name, size_t capacity)
    : m_capacity(capacity), m_name(objectName(name)) {
  // The capacity is one field of the header, so a region larger than
  // that field can count is a region no reader could be told the size
  // of.
  if (capacity > std::numeric_limits<std::uint32_t>::max()) {
    m_name.clear();
    return;
  }
  // Whatever stood under the name is gone first: a region is sized as
  // it is made, and an object that already has a size cannot be given
  // another one under a reader holding it mapped.
  ::shm_unlink(m_name.c_str());
  const int descriptor =
      ::shm_open(m_name.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
  if (descriptor < 0) {
    m_name.clear();
    return;
  }

  // A whole number of pages: what the system maps is pages either way,
  // so the region is made the size it will be mapped at.
  const long grain = ::sysconf(_SC_PAGESIZE);
  const size_t page = grain > 0 ? (size_t)grain : sizeof(Header);
  const size_t wanted = ((sizeof(Header) + capacity + page - 1) / page) * page;
  void* mapped = MAP_FAILED;
  if (::ftruncate(descriptor, (off_t)wanted) == 0)
    mapped = ::mmap(nullptr, wanted, PROT_READ | PROT_WRITE, MAP_SHARED,
                    descriptor, 0);
  ::close(descriptor);
  if (mapped == MAP_FAILED) {
    ::shm_unlink(m_name.c_str());
    m_name.clear();
    return;
  }

  m_region = mapped;
  m_length = wanted;
  auto* const header = static_cast<Header*>(mapped);
  std::memset(header, 0, sizeof(Header));
  std::memcpy(header->magic, kMagic, sizeof(kMagic));
  header->capacity = (std::uint32_t)capacity;
  // The count stands even with nothing written, so a reader that maps a
  // region before its first message finds no message rather than one of
  // no bytes.
}

SharedMemoryWriter::~SharedMemoryWriter() {
  if (m_region != nullptr) ::munmap(m_region, m_length);
  if (!m_name.empty()) ::shm_unlink(m_name.c_str());
}

bool SharedMemoryWriter::write(std::span<const std::byte> bytes) {
  if (m_region == nullptr || bytes.size() > m_capacity) return false;
  auto* const header = static_cast<Header*>(m_region);
  std::atomic_ref<std::uint64_t> written(header->sequence);
  const std::uint64_t standing = written.load(std::memory_order_relaxed);
  // Odd while the message is being written: a reader that finds an odd
  // count is looking at a message nobody could read whole, and looks
  // again rather than copying half of one.
  written.store(standing + 1, std::memory_order_relaxed);
  // The odd count reaches a reader before the payload it stands over is
  // touched, which is the whole of what makes a torn copy visible.
  std::atomic_thread_fence(std::memory_order_release);
  std::atomic_ref<std::uint64_t>(header->size)
      .store(bytes.size(), std::memory_order_relaxed);
  std::atomic_ref<std::uint64_t>(header->writtenAtNanoseconds)
      .store(nowNanoseconds(), std::memory_order_relaxed);
  if (!bytes.empty())
    std::memcpy(payloadOf(header), bytes.data(), bytes.size());
  // Even again, and the message stands whole: everything above is in
  // the region before the count that says so is.
  written.store(standing + 2, std::memory_order_release);
  return true;
}

}  // namespace sigil::io
