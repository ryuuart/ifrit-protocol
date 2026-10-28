#pragma once

/** @file
 * @ingroup core-follow
 *
 * Following — what a describe read that can change after it: a value that
 * counts its own changes, the record of which of them one describe read
 * and at which count, and the scope a host opens around a describe so
 * every read inside it is recorded.
 */

/** @defgroup core-follow Following
 *  A describe is a function of its properties and of whatever shared
 *  values it reads while it runs. A shared value that can change between
 *  describes counts its changes (`Followable`); a host that wants to know
 *  when a describe went stale opens a `FollowScope` around it, and every
 *  such value read inside lands in a `FollowedReads` with the count it
 *  stood at. The describe is stale exactly when one of those counts has
 *  moved, which a host asks with `FollowedReads::moved` whenever it
 *  likes — nothing is pushed, so a value nobody followed costs its writer
 *  one comparison. */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace sigil::core {

class FollowedReads;

/** The recording the calling thread's describe is being followed into, or
 *  null when no describe is: what a read asks before it records itself,
 *  and what a write asks before it lands. */
[[nodiscard]] FollowedReads* following();

/** A SHARED VALUE A DESCRIBE CAN FOLLOW: its revision advances on every
 *  write that changes it, so a describe that read it at one revision is
 *  stale once the revision is another. The value itself is the deriving
 *  type's; this holds the count and the one report a refused write makes. */
class Followable {
 public:
  Followable() = default;
  Followable(const Followable&) = delete;
  Followable& operator=(const Followable&) = delete;

  /** How many changing writes this value has taken. */
  [[nodiscard]] uint64_t revision() const { return m_revision; }

  /** WHETHER A WRITE MAY LAND NOW. Always, unless the describe being
   *  followed on this thread has already read this value: a describe that
   *  writes what it read would be stale the moment it finished, and
   *  describing it again would write again — so the write is refused, and
   *  the refusal is reported once for this value through the reporting
   *  door. */
  [[nodiscard]] bool admitsWrite() const;

 protected:
  /** Says the value changed: every describe that read it before now is
   *  stale. A deriving type calls this after a write that changed it. */
  void advance() { ++m_revision; }

 private:
  uint64_t m_revision = 0;
  mutable bool m_refusalReported = false;
};

/** One value a describe read, and the revision it read it at. Holding the
 *  value keeps it alive for as long as the describe it fed is retained,
 *  which is what lets a stale question still be asked of a value every
 *  other holder has let go of. */
struct FollowedRead {
  std::shared_ptr<const Followable> source;
  uint64_t revision = 0;
};

/** EVERYTHING ONE DESCRIBE READ THAT CAN CHANGE AFTER IT: one entry per
 *  value however often the describe read it, at the revision of its first
 *  read. Empty, it allocates nothing. */
class FollowedReads {
 public:
  /** Records a read of @p source, once per value. The value is copied in
   *  only the first time it is read. */
  template <class Source>
  void note(const std::shared_ptr<Source>& source) {
    if (!source || contains(*source)) return;
    m_reads.push_back({source, source->revision()});
  }

  /** Whether @p source has been read into this recording. */
  [[nodiscard]] bool contains(const Followable& source) const {
    for (const FollowedRead& read : m_reads)
      if (read.source.get() == &source) return true;
    return false;
  }

  /** WHETHER THE DESCRIBE IS STALE: any value it read has changed since. */
  [[nodiscard]] bool moved() const {
    for (const FollowedRead& read : m_reads)
      if (read.source->revision() != read.revision) return true;
    return false;
  }

  [[nodiscard]] bool empty() const { return m_reads.empty(); }
  [[nodiscard]] size_t size() const { return m_reads.size(); }
  [[nodiscard]] const std::vector<FollowedRead>& reads() const {
    return m_reads;
  }
  void clear() { m_reads.clear(); }

 private:
  std::vector<FollowedRead> m_reads;
};

/** RECORDS EVERY FOLLOWABLE READ ON THIS THREAD into @p into while it
 *  lives, starting from an empty recording. Scopes nest: an inner one
 *  records into its own recording, and the outer one resumes when it
 *  ends. */
class FollowScope {
 public:
  explicit FollowScope(FollowedReads& into);
  ~FollowScope();
  FollowScope(const FollowScope&) = delete;
  FollowScope& operator=(const FollowScope&) = delete;

 private:
  FollowedReads* m_outer;
};

/** Records a read of @p source into the describe being followed on this
 *  thread, when one is: what a followable value's reader calls before it
 *  answers. */
template <class Source>
void noteRead(const std::shared_ptr<Source>& source) {
  if (FollowedReads* reads = following()) reads->note(source);
}

}  // namespace sigil::core
