#pragma once

/** @file
 * WHAT CAME DOWN A WIRE, KEPT: the messages drained off one feed, in the
 * order they arrived, with the oldest let go once the log is full.
 *
 * The log is the reader's own memory and not the feed's. A feed hands
 * each message out once, so a log that did not keep it could not show it
 * again; and a feed that is full drops the oldest it holds, which is a
 * different bound from this one. Both are counted, so a reader can see
 * whether what is missing was never taken or was taken and forgotten.
 */

#include <sigilio/hub/Feed.h>
#include <sigilio/source/Source.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

namespace sigil::seer {

/** ONE MESSAGE AS THE LOG KEEPS IT: when it arrived, who sent it,
 *  which arrival it was on its feed, how big it is, and the bytes
 *  themselves. The bytes are shared with everyone else holding that
 *  message, so a log of a thousand entries copies none of them. */
struct LogEntry {
  /** Seconds, as the feed stamped the arrival: since the feed was made
   *  on a live wire, and the recorded time on a replayed one. */
  double at = 0;
  uint64_t generation = 0;
  size_t size = 0;
  std::shared_ptr<const io::Bytes> bytes;
  /** The address the message came from, spelled the way a URI of the
   *  wire's own scheme is; empty on a recording, which has nobody at
   *  the other end, and on a transport with no way of knowing. One
   *  wire carries messages from many senders, so this belongs to the
   *  message and not to the wire. */
  std::string from;
};

/** A BOUNDED LOG OF ONE WIRE'S MESSAGES, oldest first. */
class Log {
 public:
  /** @p capacity is how many entries are kept before the oldest is let
   *  go. A capacity of zero keeps nothing and counts everything it is
   *  handed as forgotten. */
  explicit Log(size_t capacity = 512);

  /** Takes every message @p feed has not handed out yet, in order, and
   *  answers how many were taken. */
  size_t drain(io::Feed& feed);

  /** Every entry, oldest first. */
  const std::deque<LogEntry>& entries() const { return m_entries; }

  /** How many entries are kept before the oldest is let go. */
  size_t capacity() const { return m_capacity; }

  /** Entries let go because the log was full. */
  uint64_t forgotten() const { return m_forgotten; }

  /** Empties the log. What it counted stays counted: a reader who
   *  cleared the view did not un-receive the messages. */
  void clear();

 private:
  size_t m_capacity;
  std::deque<LogEntry> m_entries;
  uint64_t m_forgotten = 0;
};

}  // namespace sigil::seer
