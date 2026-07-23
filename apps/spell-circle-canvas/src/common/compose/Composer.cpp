// The Composer facade: the public retained-side surface (construct with a
// Ticker + FontContext, render() on data change, draw(canvas) inside the
// host's paint callback), the Impl/Instance lifecycle, and snapshot() (the
// one-shot element-tree-as-a-brush bake). The phase machinery lives in the
// sibling TUs: Reconcile / Layout / Derive / Paint / Transitions / Query.cpp,
// all sharing ComposeRuntime.h.

#include "ComposeRuntime.h"

#include <sigilweave/FontContext.h>

#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkTypes.h> // SkDebugf — the renderSlot diagnostic

#include <algorithm>
#include <set>
#include <chrono>
#include <cmath>
#include <functional>

namespace sigil::compose {

using namespace detail;

// The rare-field block split (ComposeInternal.h) took ElementNode from
// 2752 B to 1288 B, and the compact PropValue (Compose.h) to 688 B; this
// guard keeps casual field additions honest — new rare/kind-specific
// state belongs in a block, not the base struct.
static_assert(sizeof(ElementNode) <= 768,
              "ElementNode grew — put rare fields in a block");

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

TextMetrics metrics(const sigil::weave::TextStyle &style,
                    sigil::weave::FontContext &fonts) {
  SkFont font(style.shaping.typeface ? style.shaping.typeface
                                     : fonts.defaultTypeface(),
              style.shaping.fontSize);
  SkFontMetrics fm;
  font.getMetrics(&fm);
  TextMetrics out;
  out.ascent = -fm.fAscent;  // Skia reports ascent negative (above baseline)
  out.descent = fm.fDescent;
  out.leading = fm.fLeading > 0 ? fm.fLeading : 0.0f;
  // Some faces report zero for these; fall back to the conventional
  // fractions of the ascent rather than handing back a zero that reads as
  // a measurement.
  out.capHeight = fm.fCapHeight > 0 ? fm.fCapHeight : out.ascent * 0.72f;
  out.xHeight = fm.fXHeight > 0 ? fm.fXHeight : out.ascent * 0.52f;
  out.lineHeight = out.ascent + out.descent + out.leading;
  return out;
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
  const auto start = std::chrono::steady_clock::now();
  impl.stats.describedNodes = 0;
  impl.stats.memoHits = 0;
  impl.stats.patchedNodes = 0;

  if (!impl.root)
    impl.root = impl.mount(root.node(), nullptr);
  else
    impl.patch(*impl.root, root.node());

  impl.volatileDirty = true; // transitions may have started
  impl.rebuildKeyIndex();
  impl.reconcileAccumMs += std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
}

void Composer::renderSlot(std::string_view name, Element content) {
  Impl &impl = *m_impl;
  const auto start = std::chrono::steady_clock::now();
  auto it = impl.bySlot.find(std::string(name));
  if (it == impl.bySlot.end()) {
    // Silence here costs an hour, reliably, because the SYMPTOM is not
    // "my slot is empty" — it is "my slot lays out W x 0", which reads as
    // a layout bug and sends you into Yoga. It was filed as exactly that,
    // twice, before anyone looked at the name.
    //
    // The overwhelmingly likely cause is the one named below: `slot(name)`
    // stores the name in `key`, so any later `.key(...)` on that element
    // RENAMES the slot, silently, with no type error and no second field
    // to disagree with itself. Listing the names that DO exist turns the
    // diagnosis into one read.
    static std::set<std::string> warned; // once per name, not per frame
    if (warned.insert(std::string(name)).second) {
      std::string have;
      for (const auto &[key, inst] : impl.bySlot)
        have += (have.empty() ? "" : ", ") + key;
      SkDebugf("[compose] renderSlot(\"%.*s\") — no slot by that name, so "
               "nothing was rendered into it and it will lay out at zero "
               "on its content axis. Slots that DO exist: [%s]. NOTE: "
               "slot(name) stores the name in key(), so slot(\"%.*s\")"
               ".key(\"something\") RENAMES the slot to \"something\".\n",
               (int)name.size(), name.data(),
               have.empty() ? "none" : have.c_str(), (int)name.size(),
               name.data());
    }
    return;
  }
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
  impl.rebuildKeyIndex();
  impl.reconcileAccumMs += std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
}

bool Composer::dirty() const {
  return m_impl->contentDirty || m_impl->needsLayout;
}

void Composer::draw(SkCanvas &canvas) {
  Impl &impl = *m_impl;
  if (!impl.root)
    return;

  impl.stats.picturesRecorded = 0;
  impl.stats.texturesBaked = 0;
  impl.stats.nodesPainted = 0;
  impl.promotedBytesLast = impl.promotedBytes;
  impl.promotedBytes = 0;

  // Backend-aware promotion default (see ComposeRuntime.h). recorder() is
  // non-null on a Graphite canvas, recordingContext() on a Ganesh one;
  // either means the profiler's op-recording measurement does not describe
  // what this surface costs, so automatic promotion is off unless the host
  // asked for it. When it flips OFF here, drop any bakes taken on a previous
  // raster frame so a mixed-backend host does not blit a stale texture.
  const bool gpuBacked =
      canvas.recorder() != nullptr || canvas.recordingContext() != nullptr;
  const bool effective =
      impl.promotionExplicit ? impl.autoPromote : impl.autoPromote && !gpuBacked;
  if (effective != impl.autoPromoteEffective) {
    impl.autoPromoteEffective = effective;
    if (!effective && impl.root) {
      const auto clear = [](auto &&self, detail::Instance &inst) -> void {
        inst.autoTexture = false;
        inst.hotFrames = 0;
        if (inst.desc && inst.desc->cacheMode != Cache::Texture)
          inst.textureImage.reset();
        for (auto &child : inst.children)
          self(self, *child);
      };
      clear(clear, *impl.root);
    }
  }

  // PaintContext::contentScale — device px per layout px under the host's
  // current transform (2.0 on a HiDPI-scaled canvas). Best effort: recordings
  // capture the scale current when they re-record.
  {
    // maxScaleOf, not the diagonal: a host that rotates its canvas reports
    // getScaleX/Y == 0 at a quarter turn and every material would have been
    // handed uContentScale = 1 regardless of the real zoom.
    const float s = detail::maxScaleOf(canvas.getTotalMatrix());
    impl.hostScale = s > 0 ? s : 1.0f;
  }

  impl.stats.reconcileMs = impl.reconcileAccumMs;
  impl.reconcileAccumMs = 0;
  if (impl.profileEnabled) {
    impl.profileRows.clear();
    impl.profChildMs = 0;
    impl.profDepth = 0;
  }

  auto mark = std::chrono::steady_clock::now();
  const auto lap = [&mark] {
    const auto now = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(now - mark).count();
    mark = now;
    return ms;
  };

  impl.ensureLayout();
  impl.stats.layoutMs = lap();

  // Volatility changes only on reconcile or while animations run (and once
  // more on the settling frame) — skip the walk otherwise.
  const bool active = impl.ticker.active();
  if (impl.volatileDirty || active || impl.tickerWasActive) {
    impl.computeVolatile(*impl.root);
    impl.volatileDirty = false;
  }
  impl.tickerWasActive = active;
  impl.stats.volatileMs = lap();

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
  impl.stats.paintMs = lap();
  impl.contentDirty = false;
  if (impl.profileEnabled)
    std::sort(impl.profileRows.begin(), impl.profileRows.end(),
              [](const NodeCost &a, const NodeCost &b) {
                return a.selfMs > b.selfMs;
              });
}

void Composer::setProfiling(bool on) {
  m_impl->profileEnabled = on;
  if (!on)
    m_impl->profileRows.clear();
}

bool Composer::profiling() const { return m_impl->profileEnabled; }

void Composer::setAutoTexturePromotion(bool on) {
  m_impl->autoPromote = on;
  m_impl->promotionExplicit = true; // the host has an opinion; honour it on
                                    // every backend, overriding the default.
  if (!on && m_impl->root) {
    // Drop every promoted bake so a study can prove the promotion is what
    // changed its numbers, rather than measuring a stale texture.
    const auto clear = [](auto &&self, detail::Instance &inst) -> void {
      inst.autoTexture = false;
      inst.hotFrames = 0;
      inst.replayMs = 0;
      inst.liveStableRate = 0;
      if (inst.desc && inst.desc->cacheMode != Cache::Texture)
        inst.textureImage.reset();
      for (auto &child : inst.children)
        self(self, *child);
    };
    clear(clear, *m_impl->root);
  }
}

bool Composer::autoTexturePromotion() const { return m_impl->autoPromote; }

const char *Composer::promotionReason(Promotion p) {
  switch (p) {
  case Promotion::Cheap:      return "cheap enough to leave alone";
  case Promotion::Warming:    return "expensive — counting frames before a bake";
  case Promotion::Promoted:   return "baked by the library";
  case Promotion::AskedFor:   return "Cache::Texture — you asked for it";
  case Promotion::OptedOut:   return "promotion opted out";
  case Promotion::Volatile:   return "its content changes every frame";
  case Promotion::Composited: return "opacity/blend — a bake would round twice; "
                                     "ask for Cache::Texture yourself";
  // "rotated, mirrored or skewed" describes the GEOMETRY and leaves the
  // author with nothing to do about it. A study lost 24.5 ms of a 29.9 ms
  // frame to a scroll band tilted a constant −0.42°, because a scroll does
  // not lie square, and two .cache(Cache::Texture) calls took the frame
  // from 29.92 to 5.81 ms with no visual change. The refusal is real — an
  // automatic bake at an angle differs by 1 LSB on the antialiased edges of
  // a shader fill, and 1 LSB is not agreement — but it is the author's to
  // accept, and they can only accept it if they are told it exists.
  case Promotion::Transformed:
    return "rotated, mirrored or skewed: a bake would differ by ~1 LSB on "
           "the antialiased edges, so the library will not take it for you "
           "— add .cache(Cache::Texture) if you accept that (often 5x)";
  case Promotion::Filtered:   return "layer/backdrop effect or clip";
  case Promotion::ReadsBackdrop:
    return "something in this subtree blends with the canvas (a non-srcOver "
           "blend or backdrop filter, here or in a descendant)";
  case Promotion::TooBig:     return "too large to bake, or over the bake budget";
  case Promotion::SplitBaked:
    return "own paint baked, volatile children painted live over the blit";
  }
  return "";
}

const std::vector<Composer::NodeCost> &Composer::profile() const {
  return m_impl->profileRows;
}

void Composer::purgeCaches() {
  Impl &impl = *m_impl;
  if (!impl.root)
    return;
  std::function<void(Instance &)> walk = [&walk](Instance &inst) {
    inst.picture.reset();
    inst.textureImage.reset();
    inst.ownImage.reset(); // §15's split bake is a cache like any other
    inst.bakedLiveShader.reset();
    inst.hasPendingLiveFill = false;
    inst.paintDirty = true;
    inst.ownPaintDirty = true;
    for (auto &child : inst.children)
      walk(*child);
  };
  walk(*impl.root);
  impl.contentDirty = true;
  impl.volatileDirty = true;
}

std::optional<SkRect> Composer::bounds(std::string_view key) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end())
    return std::nullopt;
  // Accumulate offsets up the yoga tree.
  SkRect rect = m_impl->instanceRect(*it->second);
  // Layout runs inside draw(), so a query issued in the same update() as
  // the render() before it reads an unlaid tree — which used to hand
  // back left=0, top=0, width=NaN for EVERY key. A study lost an
  // iteration and a debug harness localising that. Absent is a far
  // better answer than a number that is not one.
  if (!rect.isFinite())
    return std::nullopt;
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
