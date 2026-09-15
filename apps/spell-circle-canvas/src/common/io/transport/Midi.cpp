/** @file
 * The MIDI transport: the port a feed's URI names, the port it opens on
 * a controller or makes for other software to reach, the callback the
 * driver runs every arriving message on, and the bytes an output writes
 * back down the cable.
 *
 * A PORT IS NAMED, NEVER NUMBERED. Which port a machine calls its
 * second depends on what else was plugged in this morning, so a URI
 * names a piece of the port's OWN name and the first port whose name
 * holds that piece is the one opened; a URI naming no piece at all
 * takes the first port there is, which is the one controller on a desk
 * that has one. A name nobody answers to opens nothing and leaves the
 * names that DO exist on the feed, so a name typed from memory is
 * corrected by reading the sentence.
 *
 * THE MESSAGES ARRIVE ON THE DRIVER'S OWN THREAD. RtMidi hands each
 * message to a callback the system runs, so a door here holds no thread
 * and no loop of its own: the callback locks the feed and delivers, and
 * a feed nobody holds any more is a message with nowhere to go and a
 * callback that ends there.
 *
 * WHAT A MESSAGE MEANS IS NOT DECIDED HERE. An arrival carries the
 * bytes the wire carried, status byte first, and the library that owns
 * the format reads them.
 */

#include <rtmidi/RtMidi.h>

#include <atomic>
#include <cctype>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/source/Source.h"
#include "sigilio/transport/Transport.h"

namespace sigil::io {
namespace {

/** What the ports this transport opens are grouped under where the
 *  system shows a person its MIDI devices, and what one of them is
 *  called there. A virtual port carries the name the URI gave it
 *  instead. */
constexpr const char* kClientName = "SigilIO";
constexpr const char* kPortName = "SigilIO";

constexpr std::string_view kScheme = "midi://";
/** What a name is prefixed with to MAKE the port rather than find it. */
constexpr std::string_view kVirtualMark = "virtual:";

/** WHICH END OF A CABLE A URI NAMES. */
enum class Direction { In, Out };

/** WHAT A MIDI URI NAMES: the end of the cable, the piece of a port's
 *  name to look for, and whether that name is a port to make rather
 *  than one to find. */
struct PortWanted {
  Direction direction = Direction::In;
  std::string name;
  bool made = false;
};

/** The word an address of @p direction is spelled with, which is also
 *  the word a sentence about it uses. */
std::string_view wordFor(Direction direction) {
  return direction == Direction::In ? "in" : "out";
}

/** The port @p uri names, or nothing when it names none: the end of the
 *  cable is one word, `in` or `out`, and everything past the slash
 *  after it is the name — empty for the first port there is, and
 *  `virtual:NAME` for a port to make. */
std::optional<PortWanted> parsePort(std::string_view uri) {
  if (!uri.starts_with(kScheme)) return std::nullopt;
  std::string_view rest = uri.substr(kScheme.size());
  PortWanted wanted;
  if (rest.starts_with("in")) {
    wanted.direction = Direction::In;
    rest = rest.substr(2);
  } else if (rest.starts_with("out")) {
    wanted.direction = Direction::Out;
    rest = rest.substr(3);
  } else {
    return std::nullopt;
  }
  // The end of the cable is a whole word, so what follows it is the
  // slash before the name and never more of the word.
  if (rest.empty()) return wanted;
  if (!rest.starts_with('/')) return std::nullopt;
  rest = rest.substr(1);
  if (rest.starts_with(kVirtualMark)) {
    wanted.made = true;
    rest = rest.substr(kVirtualMark.size());
    // A port to make is a port to call something: there is no first
    // port to fall back on when nobody named it.
    if (rest.empty()) return std::nullopt;
  }
  wanted.name = std::string(rest);
  return wanted;
}

char lowered(char letter) {
  return static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
}

/** Whether @p name holds @p wanted, letter for letter with case left
 *  out of it. A port is named by whoever built it and read off the box
 *  by whoever plugged it in, so the two spellings agree about the
 *  letters and about nothing else. Every name holds the empty piece. */
bool holds(std::string_view name, std::string_view wanted) {
  if (wanted.empty()) return true;
  if (wanted.size() > name.size()) return false;
  for (size_t at = 0; at + wanted.size() <= name.size(); ++at) {
    size_t step = 0;
    while (step != wanted.size() &&
           lowered(name[at + step]) == lowered(wanted[step]))
      ++step;
    if (step == wanted.size()) return true;
  }
  return false;
}

/** Every port at this end of the cable, in the order the system lists
 *  them. A system that stops answering partway leaves what it did
 *  answer: the list is what a sentence names and what a match is looked
 *  for in, and neither is better for having nothing in it. */
std::vector<std::string> portNames(RtMidi& midi) {
  std::vector<std::string> names;
  try {
    const unsigned int count = midi.getPortCount();
    for (unsigned int at = 0; at != count; ++at)
      names.push_back(midi.getPortName(at));
  } catch (const RtMidiError&) {
  }
  return names;
}

/** The first of @p names holding @p wanted, or nothing when none does.
 *  An empty piece is held by every name, so it answers the first port
 *  there is. */
std::optional<size_t> firstPort(const std::vector<std::string>& names,
                                std::string_view wanted) {
  for (size_t at = 0; at != names.size(); ++at)
    if (holds(names[at], wanted)) return at;
  return std::nullopt;
}

/** Why a URI naming a port nobody answers to opens nothing, in words
 *  that can be acted on: what was looked for, and the ports that do
 *  exist. A machine with no port at all is told that instead, a list of
 *  none being no help and the name that was looked for beside the point
 *  where nothing could have answered it. */
std::string noSuchPort(std::string_view uri, Direction direction,
                       std::string_view wanted,
                       const std::vector<std::string>& names) {
  const std::string end(wordFor(direction));
  if (names.empty())
    return std::string(uri) + " opens nothing: this machine has no midi " +
           end + " port at all";
  std::string why = std::string(uri) + " opens nothing: no midi " + end +
                    " port's name holds \"" + std::string(wanted) + "\"";
  why += ". The ports that exist are ";
  for (size_t at = 0; at != names.size(); ++at) {
    if (at != 0) why += ", ";
    why += "\"" + names[at] + "\"";
  }
  return why;
}

/** WHAT A CALLBACK TOUCHES, and the whole of it. The driver runs the
 *  callback on a thread of its own, so what that callback reaches is
 *  held by shared_ptr and holds the feed weakly: a feed nobody holds
 *  any more is a message with nowhere to go, and the callback ends
 *  there. */
struct Delivery {
  std::weak_ptr<Feed> feed;
  /** The sender every arrival names, which is the port itself spelled
   *  the way the URI that opened it is. */
  std::string from;
  /** Raised before the port is closed, so a message the driver is
   *  already inside the callback with is dropped rather than delivered
   *  into a feed that is giving its port back. */
  std::atomic<bool> closed{false};
};

/** THE TRANSPORT'S END OF ONE FEED: the port, and what the callback
 *  delivering through it needs.
 *
 *  The ports stand AFTER the delivery here, so they are the members
 *  destroyed first: closing a port is what stops its callbacks, and a
 *  callback reading a delivery that had already gone would be reading a
 *  door taken apart around it. */
struct Door {
  std::shared_ptr<Delivery> delivery = std::make_shared<Delivery>();
  std::unique_ptr<RtMidiIn> input;
  std::unique_ptr<RtMidiOut> output;
  /** Whether the callback stands, which is what says there is one to
   *  take back: RtMidi complains in its own words about a callback
   *  cancelled twice, and a door refused between opening its port and
   *  arming it never armed one. */
  bool listening = false;

  ~Door() { close(); }

  void close() {
    delivery->closed.store(true, std::memory_order_release);
    try {
      if (listening) {
        input->cancelCallback();
        listening = false;
      }
      if (input) input->closePort();
      if (output) output->closePort();
    } catch (const RtMidiError&) {
      // A port the system has already taken back is a port that is
      // closed. There is nothing here to say and nobody left to say it
      // to: the feed is on its way out, which is why this ran.
    }
  }

  bool send(const Bytes& message) {
    if (!output || message.bytes.empty()) return false;
    if (delivery->closed.load(std::memory_order_acquire)) return false;
    try {
      output->sendMessage(
          reinterpret_cast<const unsigned char*>(message.bytes.data()),
          message.bytes.size());
    } catch (const RtMidiError&) {
      // One message the driver refused is one message and not the port:
      // the cable is still there and the next send is as good as this
      // one was.
      return false;
    }
    return true;
  }
};

/** ONE MESSAGE, ON THE DRIVER'S OWN THREAD. The delivery is COPIED out
 *  of the door, so it stands for as long as this call does however the
 *  door is let go meanwhile. */
void arrived(double, std::vector<unsigned char>* message, void* which) {
  if (!message || message->empty() || !which) return;
  const std::shared_ptr<Delivery> delivery =
      *static_cast<std::shared_ptr<Delivery>*>(which);
  if (delivery->closed.load(std::memory_order_acquire)) return;
  const std::shared_ptr<Feed> feed = delivery->feed.lock();
  if (!feed) return;
  Bytes bytes;
  const auto* const first = reinterpret_cast<const std::byte*>(message->data());
  bytes.bytes.assign(first, first + message->size());
  feed->deliver(std::move(bytes), delivery->from);
}

/** A feed whose transport could not open: the reason stands on the
 *  feed, and there is no door to close or to send through. */
OpenedFeed refuse(const std::weak_ptr<Feed>& into, std::string why) {
  if (const std::shared_ptr<Feed> feed = into.lock())
    feed->fail(std::move(why));
  return {};
}

/** Opens one feed's port: the first port whose name holds what the URI
 *  named, or a port of that name made for other software to reach.
 *
 *  Every call into RtMidi may throw — a system with no MIDI at all, a
 *  port another program is holding, a machine that refuses a port made
 *  rather than found — and every one of them is the same thing to a
 *  feed: a door that did not open, with the driver's own words for
 *  why. */
OpenedFeed openFeed(std::string_view uri, const std::weak_ptr<Feed>& into) {
  const std::optional<PortWanted> wanted = parsePort(uri);
  if (!wanted)
    return refuse(into, std::string(uri) +
                            " is not a midi address: a feed is opened on "
                            "midi://in/NAME and midi://out/NAME, NAME being "
                            "a piece of the port's name or nothing at all, "
                            "and on midi://in/virtual:NAME to make a port "
                            "other software can reach");

  const auto door = std::make_shared<Door>();
  std::string fullName;
  try {
    if (wanted->direction == Direction::In) {
      door->input = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED,
                                               std::string(kClientName));
      // EVERY MESSAGE THE WIRE CARRIES BUT THE TWO A SCENE CANNOT USE.
      // A clock beats twenty-four times a quarter note and a sensing
      // byte arrives several times a second whether or not anybody
      // played anything, so a feed taking both is a feed of heartbeat
      // with the performance somewhere inside it. System exclusive
      // stays: it is what a controller answers a question with, and it
      // arrives only when something asked.
      door->input->ignoreTypes(false, true, true);
      if (wanted->made) {
        door->input->openVirtualPort(wanted->name);
        fullName = wanted->name;
      } else {
        const std::vector<std::string> names = portNames(*door->input);
        const std::optional<size_t> found = firstPort(names, wanted->name);
        if (!found)
          return refuse(
              into, noSuchPort(uri, wanted->direction, wanted->name, names));
        door->input->openPort(static_cast<unsigned int>(*found),
                              std::string(kPortName));
        fullName = names[*found];
      }
    } else {
      door->output = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED,
                                                 std::string(kClientName));
      if (wanted->made) {
        door->output->openVirtualPort(wanted->name);
        fullName = wanted->name;
      } else {
        const std::vector<std::string> names = portNames(*door->output);
        const std::optional<size_t> found = firstPort(names, wanted->name);
        if (!found)
          return refuse(
              into, noSuchPort(uri, wanted->direction, wanted->name, names));
        door->output->openPort(static_cast<unsigned int>(*found),
                               std::string(kPortName));
        fullName = names[*found];
      }
    }
  } catch (const RtMidiError& trouble) {
    return refuse(into,
                  std::string(uri) + " opens nothing: " + trouble.getMessage());
  }

  // The address is written before anything can arrive at it: a message
  // names its sender, and the sender is this port.
  door->delivery->from = std::string(kScheme) +
                         std::string(wordFor(wanted->direction)) + "/" +
                         fullName;

  OpenedFeed opened;
  opened.address = door->delivery->from;
  opened.close = [door] { door->close(); };
  if (wanted->direction == Direction::Out) {
    opened.send = [door](const Bytes& message) { return door->send(message); };
    return opened;
  }

  // An input is one way: what comes back down a cable is the other
  // cable, which is a door of its own.
  door->delivery->feed = into;
  try {
    door->input->setCallback(&arrived, &door->delivery);
    door->listening = true;
  } catch (const RtMidiError& trouble) {
    return refuse(into,
                  std::string(uri) + " opens nothing: " + trouble.getMessage());
  }
  return opened;
}

}  // namespace

void registerMidi(Hub& hub) {
  // No thread is made here: the driver runs the callback that delivers,
  // and an output is written on the thread that asked.
  hub.setFeedTransport("midi",
                       [](std::string_view uri, std::weak_ptr<Feed> into) {
                         return openFeed(uri, into);
                       });
}

}  // namespace sigil::io
