/** @file
 * The thread a transport's sockets run on: started with the object,
 * stopped and joined with it.
 */

#include "IoThread.h"

namespace sigil::io::detail {

IoThread::IoThread()
    : m_thread([running = m_running] { running->context.run(); }) {}

IoThread::~IoThread() {
  m_running->work.reset();
  m_running->context.stop();
  // The last socket sharing this thread may be released by a handler the
  // thread is running, which leaves the thread holding the object it
  // stands on. It cannot wait for itself, and the run loop it is inside
  // still owns the context until it returns: its own reference to the
  // context is what carries that context to the end of the loop.
  if (m_thread.get_id() == std::this_thread::get_id()) {
    m_thread.detach();
    return;
  }
  m_thread.join();
}

}  // namespace sigil::io::detail
