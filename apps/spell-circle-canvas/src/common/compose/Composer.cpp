// The Composer facade: the public retained-side surface (construct with a
// Ticker + FontContext, render() on data change, draw(canvas) inside the
// host's paint callback), the Impl/Instance lifecycle, and snapshot() (the
// one-shot element-tree-as-a-brush bake). The phase machinery lives in the
// sibling TUs: Reconcile / Layout / Derive / Paint / Transitions / Query.cpp,
// all sharing ComposeRuntime.h.

#include "ComposeRuntime.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>

#include <algorithm>
#include <cmath>
#include <functional>

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Instance lifecycle

detail::Instance::~Instance() {
  if (yoga)
    YGNodeRemoveAllChildren(yoga); // detach before children free theirs
  children.clear();
  if (yoga)
    YGNodeFree(yoga);
  // AnimatedFloat outputs die here; Choreograph disconnects their motions
  // automatically — unmount cancels transitions by construction.
}

// ---------------------------------------------------------------------------
// Composer public surface

Composer::Composer(motion::Ticker &ticker, sigil::weave::FontContext &fonts)
    : m_impl(std::make_unique<Impl>(ticker, fonts)) {}
Composer::~Composer() = default;

sk_sp<SkPicture> snapshot(Element root, sigil::weave::FontContext &fonts,
                          SkSize maxSize) {
  motion::Ticker ticker; // inert: nothing steps it, transitions can't run
  Composer composer(ticker, fonts);
  Composer::Impl &impl = *composer.m_impl;
  impl.liveOnly = true; // one-shot: per-node caches would be pure waste
  composer.render(std::move(root));
  if (!impl.root)
    return nullptr;
  if (!maxSize.isEmpty()) {
    if (maxSize.width() > 0)
      YGNodeStyleSetMaxWidth(impl.root->yoga, maxSize.width());
    if (maxSize.height() > 0)
      YGNodeStyleSetMaxHeight(impl.root->yoga, maxSize.height());
  }
  impl.ensureLayout();
  const SkRect rect = impl.instanceRect(*impl.root);
  if (rect.isEmpty())
    return nullptr;
  SkPictureRecorder recorder;
  SkCanvas *canvas =
      recorder.beginRecording(SkRect::MakeWH(rect.width(), rect.height()));
  impl.paint(*impl.root, *canvas);
  return recorder.finishRecordingAsPicture();
}

SkSize measure(Element root, sigil::weave::FontContext &fonts,
               SkSize maxSize) {
  motion::Ticker ticker; // inert — same sampling rules as snapshot()
  Composer composer(ticker, fonts);
  Composer::Impl &impl = *composer.m_impl;
  impl.liveOnly = true;
  composer.render(std::move(root));
  if (!impl.root)
    return SkSize::MakeEmpty();
  if (!maxSize.isEmpty()) {
    if (maxSize.width() > 0)
      YGNodeStyleSetMaxWidth(impl.root->yoga, maxSize.width());
    if (maxSize.height() > 0)
      YGNodeStyleSetMaxHeight(impl.root->yoga, maxSize.height());
  }
  impl.ensureLayout();
  const SkRect rect = impl.instanceRect(*impl.root);
  return {rect.width(), rect.height()};
}

void Composer::setSize(SkSize size) {
  if (m_impl->size == size)
    return;
  m_impl->size = size;
  m_impl->needsLayout = true;
  m_impl->contentDirty = true;
}

void Composer::setClock(const motion::FrameClock *clock) {
  m_impl->clock = clock;
}

void Composer::setView(Effect view) {
  m_impl->view = std::move(view);
  m_impl->contentDirty = true; // the composite changes even if no node did
}

void Composer::render(Element root) {
  Impl &impl = *m_impl;
  impl.stats.describedNodes = 0;
  impl.stats.memoHits = 0;
  impl.stats.patchedNodes = 0;

  if (!impl.root)
    impl.root = impl.mount(root.node(), nullptr);
  else
    impl.patch(*impl.root, root.node());

  impl.computeVolatile(*impl.root);
  impl.volatileDirty = true; // transitions may have started
  impl.rebuildKeyIndex();
}

void Composer::renderSlot(std::string_view name, Element content) {
  Impl &impl = *m_impl;
  auto it = impl.byKey.find(std::string(name));
  if (it == impl.byKey.end() || it->second->desc->kind != Kind::Slot)
    return;
  Instance &slotInst = *it->second;

  // Patch or mount the slot's single content child.
  if (slotInst.children.size() == 1) {
    impl.patch(*slotInst.children.front(), content.node());
  } else {
    slotInst.children.clear();
    slotInst.children.push_back(impl.mount(content.node(), &slotInst));
    YGNodeRemoveAllChildren(slotInst.yoga);
    YGNodeInsertChild(slotInst.yoga, slotInst.children.front()->yoga, 0);
    slotInst.paintOrder = {0};
    impl.needsLayout = true;
  }
  slotInst.markPaintDirtyUp();
  impl.contentDirty = true;
  impl.volatileDirty = true;
  if (impl.root)
    impl.computeVolatile(*impl.root);
  impl.rebuildKeyIndex();
}

bool Composer::dirty() const {
  return m_impl->contentDirty || m_impl->needsLayout;
}

void Composer::draw(SkCanvas &canvas) {
  Impl &impl = *m_impl;
  if (!impl.root)
    return;

  impl.stats.picturesRecorded = 0;
  impl.stats.nodesPainted = 0;

  // PaintContext::contentScale — device px per layout px under the host's
  // current transform (2.0 on a HiDPI-scaled canvas). Best effort: recordings
  // capture the scale current when they re-record.
  {
    const SkMatrix &m = canvas.getTotalMatrix();
    const float s = std::max(std::abs(m.getScaleX()), std::abs(m.getScaleY()));
    impl.hostScale = s > 0 ? s : 1.0f;
  }

  impl.ensureLayout();

  // Volatility changes only on reconcile or while animations run (and once
  // more on the settling frame) — skip the walk otherwise.
  const bool active = impl.ticker.active();
  if (impl.volatileDirty || active || impl.tickerWasActive) {
    impl.computeVolatile(*impl.root);
    impl.volatileDirty = false;
  }
  impl.tickerWasActive = active;

  // Output view transform: the composer's whole output renders into one
  // layer and composites through the view filter (an OCIO display/view baked
  // to a LUT, typically). Post-cache: per-node pictures replay unchanged.
  const bool hasView = (bool)impl.view.imageFilter();
  if (hasView) {
    SkPaint viewPaint;
    viewPaint.setImageFilter(impl.view.imageFilter());
    canvas.saveLayer(nullptr, &viewPaint);
  }
  impl.paint(*impl.root, canvas);
  if (hasView)
    canvas.restore();
  impl.contentDirty = false;
}

std::optional<SkRect> Composer::bounds(std::string_view key) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end())
    return std::nullopt;
  // Accumulate offsets up the yoga tree.
  SkRect rect = m_impl->instanceRect(*it->second);
  YGNodeRef node = YGNodeGetParent(it->second->yoga);
  while (node) {
    rect.offset(YGNodeLayoutGetLeft(node), YGNodeLayoutGetTop(node));
    node = YGNodeGetParent(node);
  }
  return rect;
}

const sigil::weave::ParagraphLayout *
Composer::paragraphLayout(std::string_view key) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end() || !it->second->paragraph)
    return nullptr;
  return &it->second->textLayout;
}

std::optional<std::string> Composer::hitTest(SkPoint canvasPoint) const {
  // Logically const; fills the same per-instance outline caches paint does
  // (memoization, not mutation of observable state).
  Impl &impl = const_cast<Impl &>(*m_impl);
  if (!impl.root)
    return std::nullopt;
  return impl.hitInstance(*impl.root, canvasPoint, nullptr);
}

const Composer::Stats &Composer::stats() const {
  // Tree tallies are computed on demand, never in the frame loop.
  size_t instances = 0, pictures = 0, textures = 0;
  std::function<void(const Instance &)> tally = [&](const Instance &i) {
    ++instances;
    if (i.picture)
      ++pictures;
    if (i.textureImage)
      ++textures;
    for (const auto &child : i.children)
      tally(*child);
  };
  if (m_impl->root)
    tally(*m_impl->root);
  m_impl->stats.instances = instances;
  m_impl->stats.picturesLive = pictures;
  m_impl->stats.texturesLive = textures;
  return m_impl->stats;
}

} // namespace sigil::compose
