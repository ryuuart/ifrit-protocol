#pragma once

/** @file
 * @ingroup io-hub
 * The handle a callback stands on the hub's advance by.
 */

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

namespace sigil::io {

class Hub;

/** A movable lease that keeps a callback on the hub's advance: the
 *  lease holds the callback, and releasing it or destroying it takes the
 *  callback off the hub. A lease may outlive its Hub, having then
 *  nothing left to unregister from. */
class Lease {
 public:
  /** What an advance hands a callback: the time it was given, which is
   *  the time every replayed recording was just advanced to. */
  using Callback = std::function<void(std::chrono::duration<double> time)>;

  Lease() = default;
  /** Takes over the moved-from lease's registration. */
  Lease(Lease&&) noexcept = default;
  /** Takes over the moved-from lease's registration, dropping this
   *  one's. */
  Lease& operator=(Lease&&) noexcept = default;
  Lease(const Lease&) = delete;
  Lease& operator=(const Lease&) = delete;

  /** Takes the callback off the hub, which destroying the lease does
   *  anyway. An advance that is already running its callbacks runs this
   *  one out: it holds what it is running. */
  void release() { m_callback.reset(); }

  /** Whether a callback still stands on the hub through this lease. */
  bool registered() const { return m_callback != nullptr; }

 private:
  friend class Hub;
  explicit Lease(std::shared_ptr<Callback> callback)
      : m_callback(std::move(callback)) {}

  /** The one owner of the callback. The hub knows it weakly, so a lease
   *  that is gone leaves nothing to run. */
  std::shared_ptr<Callback> m_callback;
};

}  // namespace sigil::io
