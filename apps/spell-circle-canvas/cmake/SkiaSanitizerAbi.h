/* Skia's layout, pinned to the prebuilt archive's, for sanitized objects.
 *
 * Skia's TArray carries an extra member when its headers are compiled with
 * SK_SANITIZE_ADDRESS, which Skia derives from the compiler's
 * address_sanitizer feature. That member enlarges every header type holding
 * a TArray by value — SkPathBuilder among them — so an instrumented
 * translation unit and an uninstrumented Skia archive place the members
 * that follow at different offsets: a fill type stored through the inline
 * setter is read back from elsewhere and silently reverts, and the same
 * applies to every other field behind an array.
 *
 * Forced into each sanitized translation unit ahead of any Skia header,
 * this file claims SkASAN.h's include guard and supplies that header's
 * interface with the feature macro left undefined, so this repository's
 * objects see the layout the archive was built with. The cost is Skia's
 * own container poisoning, which reports nothing against an uninstrumented
 * archive in any case; every check in the ASan runtime itself — the
 * allocator's, the interceptors' — is untouched.
 */

#ifndef SkASAN_DEFINED
#define SkASAN_DEFINED

#include <cstddef>

static inline void sk_asan_poison_memory_region(
    [[maybe_unused]] void const volatile* addr, [[maybe_unused]] size_t size) {}

static inline void sk_asan_unpoison_memory_region(
    [[maybe_unused]] void const volatile* addr, [[maybe_unused]] size_t size) {}

static inline int sk_asan_address_is_poisoned(
    [[maybe_unused]] void const volatile* addr) {
  return 0;
}

#endif  // SkASAN_DEFINED
