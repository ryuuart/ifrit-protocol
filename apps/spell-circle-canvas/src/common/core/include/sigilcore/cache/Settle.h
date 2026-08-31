#pragma once

/** @file
 * The stability release: the proof that a node which DECLARES volatility
 * is nevertheless holding still, and the three sides of the protocol that
 * keeps that proof honest.
 */

#include <utility>

namespace sigil::core {

/** A NODE'S HOLD ON A SET OF VALUES, and the release it earns.
 *
 *  A binding stays connected for the whole life of the node it drives, so
 *  a declaration alone can never say that a motion has stopped — an
 *  entrance that played once and settled declares exactly what a loop
 *  running at full speed declares. This is the observation that separates
 *  them: once the values a node's drawing depends on resolve identically
 *  for `hold` consecutive frames, the node stops declaring the volatility
 *  they caused, and every ancestor may cache across it.
 *
 *  `Values` is whatever the host can read back OUTSIDE of drawing, bounded
 *  per node, and compare by value — that last requirement is what makes it
 *  a cache key at all. It must be equality-comparable and cheap to copy.
 *
 *  THE PROTOCOL HAS THREE SIDES AND ALL THREE ARE REQUIRED. Skipping any
 *  one of them does not fail loudly; it replays a frame that has already
 *  changed.
 *
 *   - `observe` runs where the host draws, because that is the only place
 *     that happens once per frame. It counts the hold.
 *   - `release` runs inside the proof, which re-runs on its own schedule
 *     and so cannot count frames itself. It converts a warmed-up hold into
 *     the actual release.
 *   - `moved` runs once per draw over the released nodes. A value driven
 *     from OUTSIDE — an output assigned between two proofs — must
 *     re-declare the frame it moves, before anything holding the old
 *     reading replays. Without it the release is a promise the host cannot
 *     keep. */
template <typename Values>
class Settle {
 public:
  /** WRITE SIDE, once per drawn frame. @p stable is the host's own answer
   *  to "did this node's artefact stay exact" — its inputs unchanged and
   *  nothing else having dirtied it.
   *
   *  @return true on the single observation that crosses @p hold, which is
   *  the host's cue to re-run the proof: the release itself happens there,
   *  and this side never performs one. Any instability restarts the count,
   *  so a value that is genuinely moving pays this machinery nothing
   *  beyond the compare the host already made. */
  bool observe(bool stable, const Values& now, int hold) {
    if (!stable) {
      m_frames = 0;
      return false;
    }
    m_held = now;
    if (m_frames >= hold) return false;
    return ++m_frames == hold;
  }

  /** READ SIDE, inside the proof. Honours a warmed-up hold.
   *
   *  @p read is called ONLY once the count has reached @p hold, so a node
   *  that is plainly moving never pays for resolving its values here.
   *  @return true when the node is provably holding still and may stop
   *  declaring — at which point the host registers it for `moved`. A
   *  reading that differs restarts the hold from the new value. */
  template <typename Read>
  bool release(int hold, Read&& read) {
    if (m_frames < hold) return false;
    Values now = read();
    if (now == m_held) return true;
    m_frames = 0;
    m_held = std::move(now);
    return false;
  }

  /** RESCAN SIDE, once per draw over the released nodes.
   *  @return true when @p now differs from the held reading — the node
   *  must re-declare its volatility and stale everything above it THIS
   *  frame. The hold restarts from the new value. */
  bool moved(Values now) {
    if (now == m_held) return false;
    m_frames = 0;
    m_held = std::move(now);
    return true;
  }

  /** Consecutive stable observations so far. */
  [[nodiscard]] int frames() const { return m_frames; }
  /** The reading the hold is against. */
  [[nodiscard]] const Values& held() const { return m_held; }
  /** Restart the hold, keeping the reading — for a host that learned from
   *  somewhere else that the node moved. */
  void restart() { m_frames = 0; }

 private:
  Values m_held{};
  int m_frames = 0;
};

}  // namespace sigil::core
