#pragma once
// The global-namespace spelling of sigil::skia::GraphiteContext, for a
// consumer that still spells the old name or forward-declares it as a
// class. A subclass rather than an alias, because an alias cannot
// satisfy a `class SkiaGraphiteContext;` forward declaration. Nothing
// new includes this: spell <sigilskia/graphite/GraphiteContext.h> and
// the sigil::skia names, and delete this header once nothing does.

#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/qt/QtInterop.h>

#include <memory>

class SkiaGraphiteContext final : public sigil::skia::GraphiteContext {
 public:
  static std::unique_ptr<SkiaGraphiteContext> create(QRhi* rhi) {
    return takeOver(sigil::skia::createGraphiteContext(rhi));
  }
#ifdef __APPLE__
  static std::unique_ptr<SkiaGraphiteContext> createMetal(
      void* mtlDevice, void* mtlCommandQueue) {
    return takeOver(GraphiteContext::createMetal(mtlDevice, mtlCommandQueue));
  }
#endif

 private:
  explicit SkiaGraphiteContext(GraphiteContext&& base)
      : GraphiteContext(std::move(base)) {}
  static std::unique_ptr<SkiaGraphiteContext> takeOver(
      std::unique_ptr<GraphiteContext> made) {
    if (!made) return nullptr;
    return std::unique_ptr<SkiaGraphiteContext>(
        new SkiaGraphiteContext(std::move(*made)));
  }
};
