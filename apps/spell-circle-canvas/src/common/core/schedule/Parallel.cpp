/** @file
 * The chunk split, over oneTBB.
 *
 * THIS IS WHERE THE TASK RUNTIME IS NAMED. It is linked privately and
 * appears in no header, so a consumer that divides a range acquires a
 * seam and not a dependency, and a runtime swapped here is swapped for
 * every consumer at once.
 */

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>
#include <sigilcore/schedule/Parallel.h>

// This translation unit is compiled with the thread sanitizer's
// instrumentation off and this macro on instead, so what follows is
// still stated where the build watches threads. A build that reaches
// here instrumented anyway states it too.
#if !defined(SIGIL_SCHEDULE_THREAD_SANITIZER) && defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define SIGIL_SCHEDULE_THREAD_SANITIZER 1
#endif
#endif

#ifdef SIGIL_SCHEDULE_THREAD_SANITIZER
#include <sanitizer/tsan_interface.h>
#endif

namespace sigil::core::schedule::detail {

namespace {

#ifdef SIGIL_SCHEDULE_THREAD_SANITIZER

// workaround: the task runtime is a prebuilt archive, so a thread
// sanitizer watches what a chunk writes and what its caller reads
// afterwards but not the runtime's join between the two, and reads an
// ordered handoff as a race. The two edges of one divided range are
// stated here in terms the sanitizer does model.
//
// They are two addresses rather than one, and both are this call's own.
// A chunk takes what the caller published and publishes to the caller
// alone: no chunk ever takes what another chunk published, so two
// chunks writing one item stay unordered and stay reported, which is
// the rule a body is held to.
struct Edges {
  char toChunks = 0;
  char toCaller = 0;

  /** Everything the caller wrote before the range was divided. */
  void publishToChunks() { __tsan_release(&toChunks); }

  /** Everything every chunk wrote, taken when the range is done —
   *  however it ended, so a body that throws still leaves what it wrote
   *  ordered before the caller's next read. */
  ~Edges() { __tsan_acquire(&toCaller); }

  /** Held for the length of one chunk. */
  struct Chunk {
    Edges& edges;
    explicit Chunk(Edges& held) : edges(held) {
      __tsan_acquire(&held.toChunks);
    }
    ~Chunk() { __tsan_release(&edges.toCaller); }
  };
};

#else

/** What a build with no thread sanitizer states about a divided range:
 *  nothing, at no cost. */
struct Edges {
  void publishToChunks() {}
  struct Chunk {
    explicit Chunk(Edges&) {}
  };
};

#endif

}  // namespace

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
  Edges edges;
  edges.publishToChunks();
  oneapi::tbb::parallel_for(
      oneapi::tbb::blocked_range<size_t>(0, count, items),
      [&edges, body, run](const oneapi::tbb::blocked_range<size_t>& range) {
        const Edges::Chunk chunk(edges);
        run(body, range.begin(), range.end());
      });
}

}  // namespace sigil::core::schedule::detail
