#pragma once

/** @file
 * EVERY WIRE AT ONCE: the door each feed is opened through, the feeds
 * that are open, and what each of them is doing right now.
 *
 * A wire is one URI a message arrives on or leaves by. Opening one adds
 * it to a list that keeps the order it was opened in, so a reader looks
 * down the same column from one frame to the next. A URI nothing can
 * open is still a wire: the feed exists and carries the sentence that
 * says why, which is what a reader has to see to correct it.
 *
 * Nothing here has a thread or a clock of its own. `dispatch()` moves
 * every replayed recording to the caller's seconds and `tick()` reads
 * the wires and writes down what they are doing, both of them driven by
 * whatever loop the host runs.
 */

#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::seer {

/** WHAT ONE WIRE IS DOING, as of the tick that read it. A plain value:
 *  a reader compares this frame's against the last one's rather than
 *  asking the feed a second time, and the feed is free to take another
 *  message while the reader is looking. */
struct Vitals {
  /** The URI the wire was opened on. */
  std::string uri;
  /** The local end the transport bound, or empty where it bound none. */
  std::string address;
  /** Why the door could not be opened, or what ended it; empty when
   *  nothing went wrong. */
  std::string error;
  /** How many messages have arrived since the wire was opened. */
  uint64_t generation = 0;
  /** Messages that fell off the front because nobody drained them
   *  before the feed was full. */
  uint64_t dropped = 0;
  /** Messages a second, over the last second of ticks; zero until a
   *  second of ticks has gone by. */
  double arrivalsPerSecond = 0;
  /** Whether the feed takes anything more. A recording that has played
   *  out closes itself, and so does a socket the system ended. */
  bool closed = false;
  /** The newest message, or null before the first one arrives. */
  std::shared_ptr<const io::Bytes> newest;
};

/** THE WIRES BEING WATCHED, and the one hub they are opened on.
 *
 *  Every transport this build carries is registered on that hub when
 *  this is made, so a udp:// URI opens a socket without the caller
 *  naming a transport. A URI that resolves onto a file is played back
 *  from it instead, which is what `mountRecording()` arranges. */
class Wires {
 public:
  Wires();
  ~Wires();

  Wires(const Wires&) = delete;
  Wires& operator=(const Wires&) = delete;

  /** Opens @p uri and keeps the feed, so the wire stays open until it is
   *  closed rather than for as long as the caller holds the answer.
   *  A URI already open answers the feed that is already there, and its
   *  policy is the one it was opened with. A URI nothing can open
   *  answers a feed whose `error()` says so. */
  std::shared_ptr<io::Feed> open(std::string_view uri,
                                 io::FeedPolicy policy = {});

  /** Closes the wire at @p uri and drops it from the list; false when no
   *  wire is open on it. A feed that closes itself — a recording played
   *  out, a socket the system ended — stays in the list and says it is
   *  closed, because it is still a wire somebody opened. */
  bool close(std::string_view uri);

  /** Every feed that is open, in the order the wires were opened. */
  std::vector<std::shared_ptr<io::Feed>> feeds() const;

  /** The feed open on @p uri, or null. */
  std::shared_ptr<io::Feed> feed(std::string_view uri) const;

  /** Resolves @p uri onto the recording at @p path, so the next open()
   *  on that URI plays the file back instead of opening a socket. A wire
   *  already open on the URI is unaffected: it holds the door it was
   *  given, so it is closed first by whoever wants the file. */
  void mountRecording(std::string_view uri, const std::filesystem::path& path);

  /** Moves every replayed recording to @p seconds on the caller's clock,
   *  delivering each arrival the file stamped at or before it. A live
   *  wire is unaffected — its transport delivers on its own. */
  void dispatch(double seconds);

  /** Reads every wire and writes down what it is doing at @p seconds on
   *  the caller's clock. The rate is worked out from the generations
   *  earlier ticks read, so it is a property of how often this is
   *  called and needs no thread behind it. */
  void tick(double seconds);

  /** What the last tick read, one entry per wire in opening order. */
  const std::vector<Vitals>& vitals() const { return m_vitals; }

  /** What the last tick read about the wire at @p uri, or null when no
   *  wire is open on it. */
  const Vitals* vitalsOf(std::string_view uri) const;

  /** The hub the wires are opened on, for a caller that mounts
   *  something of its own or asks it for bytes that do not arrive. */
  io::Hub& hub() { return m_hub; }

 private:
  /** A generation as of a moment: the pair a rate is worked out from. */
  struct Sample {
    double at = 0;
    uint64_t generation = 0;
  };

  /** One wire: the feed, and the recent generations its rate is read
   *  off. */
  struct Watch {
    std::shared_ptr<io::Feed> feed;
    std::deque<Sample> samples;
  };

  /** The wire open on @p uri, or nothing. */
  const Watch* watchOf(std::string_view uri) const;

  io::Hub m_hub;
  std::vector<Watch> m_watches;
  std::vector<Vitals> m_vitals;
};

}  // namespace sigil::seer
