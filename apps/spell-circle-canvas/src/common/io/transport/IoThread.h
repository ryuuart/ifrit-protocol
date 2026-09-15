#pragma once

/** @file
 * The thread every socket this feature opens runs its work on, and the
 * one of them a whole registration shares. Private to the transport
 * feature.
 */

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>
#include <mutex>
#include <thread>

namespace sigil::io::detail {

/** AN IO CONTEXT AND THE ONE THREAD THAT RUNS IT, with the work guard
 *  that keeps that run loop from returning while no socket has anything
 *  to do.
 *
 *  Every socket a transport opens shares one of these and holds it, so
 *  the context outlives the last socket that could touch it: an Asio io
 *  object destroyed after its context would call into services that are
 *  already gone.
 *
 *  Destruction releases the guard, stops the context and joins. */
class IoThread {
 public:
  IoThread();
  ~IoThread();
  IoThread(const IoThread&) = delete;
  IoThread& operator=(const IoThread&) = delete;

  boost::asio::io_context& context() { return m_running->context; }

 private:
  /** What the run loop stands on, held by the thread as well as by this
   *  object, so the context is destroyed after run() has returned
   *  however the last reference to it falls. */
  struct Running {
    boost::asio::io_context context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work = boost::asio::make_work_guard(context);
  };

  std::shared_ptr<Running> m_running = std::make_shared<Running>();
  std::thread m_thread;
};

/** THE THREAD ONE REGISTRATION'S WORK SHARES — the sockets a UDP
 *  registration opened, the timers a shared memory registration looks
 *  on. It is made when the first feed opens, so a hub given a transport
 *  and never asked for a feed of that scheme starts no thread, and it
 *  stands until the hub drops the transport or the last work opened
 *  through it is gone, whichever is later. */
class SharedIoThread {
 public:
  std::shared_ptr<IoThread> acquire() {
    const std::lock_guard<std::mutex> lock(m_gate);
    if (!m_thread) m_thread = std::make_shared<IoThread>();
    return m_thread;
  }

 private:
  std::mutex m_gate;
  std::shared_ptr<IoThread> m_thread;
};

}  // namespace sigil::io::detail
