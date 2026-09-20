#pragma once

/** @file
 * Binding resource access: a hub held outright or borrowed from the
 * host's session, and the feeds opened through it.
 */

#include <pybind11/pybind11.h>

#include <functional>
#include <memory>

namespace sigil::io {
class Hub;
class Feed;
}  // namespace sigil::io

namespace sigil::python {

/** An owned hub, or checked access to the host's session services. Native
 * operations use the same hub in both cases; no Python callbacks run on IO
 * workers. Context feed leases belong to the session, not escaped wrappers. */
class HubHandle {
 public:
  /** A hub of this handle's own, owned outright. */
  HubHandle();
  /** A handle onto the host's hub: @p access reaches it and throws
   *  when the session is gone, and @p retainFeed gives the session a
   *  lease on a feed opened through it. */
  HubHandle(std::function<io::Hub&()> access,
            std::function<void(std::shared_ptr<io::Feed>)> retainFeed);
  ~HubHandle();
  /** The hub itself: the one this handle owns, or the host's through
   *  the access it was given, which refuses once the session is gone. */
  io::Hub& get() const;
  /** The hub this handle owns, or null when it borrows the host's. */
  const std::shared_ptr<io::Hub>& owner() const { return m_owner; }
  /** How the host's hub is reached; empty on an owning handle. */
  const std::function<io::Hub&()>& access() const { return m_access; }
  /** Hands @p feed to the session to hold, so its lifetime is the
   *  session's rather than a Python wrapper's. An owning handle has no
   *  session to hand it to and does nothing. */
  void retain(const std::shared_ptr<io::Feed>& feed) const;

 private:
  std::shared_ptr<io::Hub> m_owner;
  std::function<io::Hub&()> m_access;
  std::function<void(std::shared_ptr<io::Feed>)> m_retainFeed;
};

/** Overlapping sessions share one lease for each native feed. Releasing the
 * last session lease closes it even when Python retains an obsolete wrapper. */
std::shared_ptr<void> retainSessionFeed(std::shared_ptr<io::Feed> feed);

}  // namespace sigil::python
