/* Skia's layout, pinned to the prebuilt archive's, for sanitized objects.
 *
 * workaround: instrumented Skia headers meeting an uninstrumented archive.
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
 *
 * Claiming a guard is only a claim while the name is still the one that
 * header declares, so the pin is held from both ends: the assertion at the
 * foot of this file proves the layout it produced, and vcpkg.json carries
 * a `version>=` constraint naming the sigil-vcpkg-registry skia port this
 * was read from — 151#2 — so the port cannot resolve backwards underneath
 * it. Read that port's include/private/SkASAN.h when raising either.
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

/* The pin, proved.
 *
 * TArray keeps its poisoning flag only while SK_SANITIZE_ADDRESS is
 * defined, and that one member is what displaces every field a header type
 * stores behind an array. Pinned, TArray is 16 bytes — a pointer, a size,
 * and a capacity packed with an ownership bit — which is the layout the
 * archive was compiled with; with the feature macro live it is 24. Reading
 * that size here is what makes a divergence arrive as a compile error
 * naming this constraint instead of as a value that reverts when it is
 * read back, whatever caused it: a renamed guard, a header that moved out
 * from under the claim, or a member added behind the flag.
 *
 * The array header is reached only where a target already puts Skia on its
 * include path. Elsewhere there is no layout to pin and nothing to prove.
 */
#if defined(__has_include)
#if __has_include(<include/private/SkTArray.h>)
#include <include/private/SkTArray.h>
static_assert(sizeof(skia_private::TArray<int>) == 16,
              "Skia's array layout no longer matches the prebuilt archive: "
              "this translation unit compiled TArray with its address "
              "sanitizer member, so every field a Skia header type stores "
              "behind an array sits at an offset the archive's own "
              "accessors do not use.");
#endif
#endif

#endif  // SkASAN_DEFINED
