/** @file
 * The chunk split, over oneTBB.
 *
 * THIS IS WHERE THE TASK RUNTIME IS NAMED. It is linked privately and
 * appears in no header, so a consumer that divides a range acquires a
 * seam and not a dependency, and a runtime swapped here is swapped for
 * every consumer at once.
 */

#include <sigilcore/schedule/Parallel.h>

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

namespace sigil::core::schedule::detail {

void overChunks(size_t count, size_t grain, void* body, ChunkBody run) {
  if (count == 0) return;
  const size_t items = grain == 0 ? 1 : grain;
  // One chunk is the calling thread's, so the smallest range a caller can
  // describe is also the cheapest one to run: no task is created, and the
  // grain alone decides where that begins.
  if (count <= items) {
    run(body, 0, count);
    return;
  }
  oneapi::tbb::parallel_for(
      oneapi::tbb::blocked_range<size_t>(0, count, items),
      [body, run](const oneapi::tbb::blocked_range<size_t>& range) {
        run(body, range.begin(), range.end());
      });
}

}  // namespace sigil::core::schedule::detail
