#pragma once

/** @file
 * The order stills are drawn in, and the one worker that draws them.
 */

#include <sigilsketch/plate/Thumbnails.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace sigil::sketch {

/** ONE STILL AT A TIME, IN THE ORDER THEY WERE ASKED FOR.
 *
 *  A browser fills its thumbnails while nothing is being presented, and
 *  what it needs from a queue is exactly this: a worker of its own, one
 *  render in flight, an ask that jumps the queue because the row is on
 *  screen, an ask withdrawn because it scrolled away, and a way to stop
 *  that does not wait out the walk it is stopping.
 *
 *  IT DRAWS NOTHING. The render is the caller's callable, so what is
 *  ordered here is separable from what a sketch costs to photograph —
 *  and a case can assert the order without opening a sketch. It knows no
 *  window either: the report lands ON THE WORKER, and a host with a
 *  thread of its own marshals from there. */
class ThumbnailQueue {
 public:
  /** Draws the still for @p index. @p stop is read between frames and
   *  raising it abandons the walk, which is answered as `Stopped`. */
  using Render =
      std::function<ThumbnailOutcome(int index, const std::atomic_bool& stop)>;
  /** WHAT ONE FINISHED STILL LEFT BEHIND, and how many are still queued
   *  behind it. Called on the worker thread, once per render that was
   *  not abandoned. */
  using Report =
      std::function<void(int index, ThumbnailOutcome outcome, int remaining)>;

  ThumbnailQueue(Render render, Report report);
  ~ThumbnailQueue();
  ThumbnailQueue(const ThumbnailQueue&) = delete;
  ThumbnailQueue& operator=(const ThumbnailQueue&) = delete;

  /** THE WHOLE FILL: @p indices in the order given, behind whatever is
   *  already queued. Nothing is queued once the fill has been ended. */
  void fill(std::vector<int> indices);

  /** ASK FOR ONE, AT THE FRONT — what a row on screen calls, so the one
   *  render in flight is followed by the sketch a reader is looking at
   *  rather than by whatever the fill reached. An index already in
   *  flight, already drawn and failed, or asked for after the fill was
   *  ended, is not queued; the answer says which happened. */
  bool request(int index);

  /** Drops a pending ask. A render already in flight is left to finish:
   *  a walk abandoned partway leaves nothing on disk, so stopping one
   *  that is nearly done spends the whole of its cost for nothing. */
  void cancel(int index);

  /** ENDS THE FILL. The walk in flight is let go at its next frame, the
   *  queue is dropped, and nothing is queued from here on — which is what
   *  opening a sketch says: the canvas is what draws now, and a second
   *  renderer beside it is what makes opening one feel slow. */
  void endFill();

  /** ENDS THE FILL AND JOINS THE WORKER, after which nothing of this
   *  object's is running. Idempotent; the destructor calls it. It costs
   *  one frame of whatever was being walked rather than the rest of the
   *  walk. */
  void stop();

  /** What is being drawn right now, or -1. */
  [[nodiscard]] int inFlight() const;
  /** How many asks are waiting behind it. */
  [[nodiscard]] size_t pending() const;
  /** Whether the fill has been ended — after which nothing is queued. */
  [[nodiscard]] bool ended() const;

 private:
  void loop();

  Render m_render;
  Report m_report;

  mutable std::mutex m_mutex;
  std::condition_variable m_wake;
  std::deque<int> m_pending;
  std::set<int> m_queued;  // what is pending or in flight, to dedupe
  std::set<int> m_failed;  // rendered once and failed — never retried
  int m_inFlight = -1;
  bool m_stop = false;

  /** RAISED TO LET GO OF THE RENDER ITSELF, and read by it between
   *  frames. Outside the mutex because the render reads it while the
   *  worker holds nothing, and because whoever raises it is about to wait
   *  for the worker to answer.
   *
   *  It is never lowered. Both things that raise it — the fill ending and
   *  this object going — are one-way: after either, no still is wanted
   *  from this worker again, and a flag that could be lowered would have
   *  to be raised again by whoever races it. */
  std::atomic_bool m_abandon{false};

  std::thread m_worker;
};

}  // namespace sigil::sketch
