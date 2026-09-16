#pragma once

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
  HubHandle();
  HubHandle(std::function<io::Hub&()> access,
            std::function<void(std::shared_ptr<io::Feed>)> retainFeed);
  ~HubHandle();
  io::Hub& get() const;
  const std::shared_ptr<io::Hub>& owner() const { return m_owner; }
  const std::function<io::Hub&()>& access() const { return m_access; }
  void retain(const std::shared_ptr<io::Feed>& feed) const;

 private:
  std::shared_ptr<io::Hub> m_owner;
  std::function<io::Hub&()> m_access;
  std::function<void(std::shared_ptr<io::Feed>)> m_retainFeed;
};

/** Overlapping sessions share one lease for each native feed. Releasing the
 * last session lease closes it even when Python retains an obsolete wrapper. */
std::shared_ptr<void> retainSessionFeed(std::shared_ptr<io::Feed> feed);
void bindIO(pybind11::module_& module);

}  // namespace sigil::python
