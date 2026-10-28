/** @file
 * The recording the calling thread's describe is followed into, and the
 * one report a refused write makes.
 */

#include <sigilcore/follow/Follow.h>
#include <sigilcore/report/Report.h>

namespace sigil::core {

namespace {

// One per thread: a describe runs on one thread from start to end, and a
// read on another thread belongs to no describe this one is following.
constinit thread_local FollowedReads* tFollowing = nullptr;

}  // namespace

FollowedReads* following() { return tFollowing; }

bool Followable::admitsWrite() const {
  const FollowedReads* reads = tFollowing;
  if (!reads || !reads->contains(*this)) return true;
  if (!m_refusalReported) {
    m_refusalReported = true;
    report("SigilCore",
           "a memo's describe wrote a live value it had read, and the write "
           "is refused: the describe would be stale as it finished and would "
           "write again each time it ran. Write the value outside the memo, "
           "or read it with peek() so the describe does not follow it.");
  }
  return false;
}

FollowScope::FollowScope(FollowedReads& into) : m_outer(tFollowing) {
  into.clear();
  tFollowing = &into;
}

FollowScope::~FollowScope() { tFollowing = m_outer; }

}  // namespace sigil::core
