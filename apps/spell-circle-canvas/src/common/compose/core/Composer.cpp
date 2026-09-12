/** @file
 * The Composer facade: the public retained-side surface — construct with a
 * Ticker + FontContext, render() on data change, draw(canvas) inside the
 * host's paint callback — and the Impl/Instance lifecycle under it. The
 * phase machinery lives in the sibling TUs — Reconcile, Layout, Derive,
 * Transitions and Query, and the paint phase's own files — all sharing
 * ComposeRuntime.h; the verbs that take a tree without a live composer are
 * in Measure.cpp.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkTypes.h>  // SkDebugf — the renderSlot diagnostic
#include <sigilmeasure/time/Laps.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/layout/ParagraphLayout.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <functional>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// One ElementNode is allocated per described node, every render, for the
// whole tree — so its size is a per-frame allocation cost, not a detail.
// Rare and kind-specific state therefore lives in separately allocated
// blocks (ComposeInternal.h) that most nodes never carry, and only hot
// fields sit inline. This assert is the gate: a field added to the base
// struct instead of a block fails the build rather than quietly taxing
// every node in every tree. The cap stands where the hot fields put it:
// the gap, the padding and the margin are lengths every kind carries,
// and a length that may be measured in the font is twice the float it
// replaced.
static_assert(sizeof(ElementNode) <= 832,
              "ElementNode grew — put rare fields in a block");

// ---------------------------------------------------------------------------
// Instance lifecycle

detail::Instance::~Instance() {
  if (yoga)
    YGNodeRemoveAllChildren(yoga);  // detach before children free theirs
  children.clear();
  if (yoga) YGNodeFree(yoga);
  // AnimatedFloat outputs die here; Choreograph disconnects their motions
  // automatically — unmount cancels transitions by construction.
}

// ---------------------------------------------------------------------------
// Composer public surface

Composer::Composer(motion::Ticker& ticker, sigil::weave::FontContext& fonts)
    : m_impl(std::make_unique<Impl>(ticker, fonts)) {}
Composer::~Composer() = default;

void Composer::setSize(SkSize size) {
  if (m_impl->size == size) return;
  m_impl->size = size;
  m_impl->needsLayout = true;
  m_impl->contentDirty = true;
}

void Composer::setClock(const motion::FrameClock* clock) {
  m_impl->clock = clock;
}

void Composer::setView(material::skia::Effect view) {
  m_impl->viewMaterial.reset();  // an Effect is already lowered
  m_impl->view = std::move(view);
  m_impl->contentDirty = true;  // the composite changes even if no node did
}

void Composer::setView(const sigil::material::Material& view) {
  // The program is the answer for a surface nothing is known about, and
  // is what stands until draw meets a canvas that says otherwise.
  setView(material::skia::Effect::recipe(view));
  m_impl->viewMaterial = view;
  m_impl->viewColorType = kUnknown_SkColorType;
}

void Composer::declareInputSpace(InputSpace space) {
  m_impl->inputSpace = space;
  if (space == InputSpace::EncodedSRGB)
    return;  // the declaration matches reality — nothing to say
  // Compositing happens in encoded sRGB and NO conversion follows this call.
  // The declaration exists so a mismatch between what the caller believes
  // its colours are and what the pipeline actually does with them is stated
  // rather than silent; adding a conversion here would change every existing
  // caller's pixels, so it is not done.
  //
  // Warned once per process, not once per composer: the mismatch is a fact
  // about the program's colour handling, and a line per composer would bury
  // the one sentence that matters.
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  const char* name =
      space == InputSpace::LinearSRGB ? "LinearSRGB" : "DisplayP3";
  SkDebugf(
      "[compose] declareInputSpace(%s): compose composites in ENCODED sRGB "
      "and performs NO conversion — your values will be TREATED as encoded "
      "sRGB regardless of this declaration. Under a %s declaration every "
      "channel maths in the pipeline (blending, by::luma, alpha "
      "compositing) runs on numbers it was not defined for, so your maths "
      "are wrong at the edges. The declaration changes no pixel; it exists "
      "so this mismatch is said out loud.\n",
      name, name);
}

Composer::InputSpace Composer::declaredInputSpace() const {
  return m_impl->inputSpace;
}

void Composer::render(const Element& root) {
  Impl& impl = *m_impl;
  const sigil::measure::Stopwatch reconcile;
  impl.reconciler.render(impl.root, root.node());
  // A patch may have started or stopped a transition, so its volatility
  // must be recomputed. An identical retained description does not: keeping
  // this false lets active() poll a released external binding before draw.
  // The same change may have moved a font, an ink or a property that
  // everything under the patched node inherits, so the cascade resolves
  // again before the next layout.
  if (impl.contentDirty || impl.needsLayout) {
    impl.volatileDirty = true;
    impl.cascadeDirty = true;
  }
  impl.rebuildKeyIndex();
  impl.reconcileAccumMs += reconcile.elapsedMs();
}

void Composer::renderSlot(std::string_view name, const Element& content) {
  Impl& impl = *m_impl;
  const sigil::measure::Stopwatch reconcile;
  auto it = impl.bySlot.find(name);
  if (it == impl.bySlot.end()) {
    // A miss must be loud, because the SYMPTOM points somewhere else: an
    // empty slot lays out W x 0, which reads as a layout bug and sends the
    // reader into Yoga rather than to the name they typed.
    //
    // The likely cause is the one the message names: `slot(name)` stores the
    // name in `key`, so any later `.key(...)` on that element renames the
    // slot with no type error and no second field to disagree with itself.
    // Listing the names that DO exist turns the diagnosis into one read.
    static thread_local boost::unordered_flat_set<std::string> warned;
    if (warned.insert(std::string(name)).second) {
      std::string have;
      for (const auto& [key, inst] : impl.bySlot)
        have += (have.empty() ? "" : ", ") + key;
      SkDebugf(
          "[compose] renderSlot(\"%.*s\") — no slot by that name, so "
          "nothing was rendered into it and it will lay out at zero "
          "on its content axis. Slots that DO exist: [%s]. NOTE: "
          "slot(name) stores the name in key(), so slot(\"%.*s\")"
          ".key(\"something\") RENAMES the slot to \"something\".\n",
          (int)name.size(), name.data(), have.empty() ? "none" : have.c_str(),
          (int)name.size(), name.data());
    }
    return;
  }
  Instance& slotInst = *it->second;

  // Patch or mount the slot's single content child; the reconciler
  // invalidates the slot either way.
  impl.reconciler.replaceContent(slotInst, content.node());
  impl.volatileDirty = true;
  // The content inherits from where the slot stands, however it was
  // described, so the pass resolves it under the slot's ancestors.
  impl.cascadeDirty = true;
  impl.rebuildKeyIndex();
  impl.reconcileAccumMs += reconcile.elapsedMs();
}

void Composer::setInherited(const sigil::weave::Type& font, SkColor4f ink) {
  Impl& impl = *m_impl;
  sigil::weave::Type root =
      sigil::weave::overlay(sigil::weave::initialType(), font);
  root.color = ink;
  if (root == impl.rootFont) return;
  impl.rootFont = std::move(root);
  impl.rootLineHeight = 0.0f;
  impl.cascadeDirty = true;
  impl.contentDirty = true;
}

bool Composer::dirty() const {
  return m_impl->contentDirty || m_impl->needsLayout;
}

bool Composer::active() const {
  Impl& impl = *m_impl;
  // A settled external binding has no reconciliation event to wake the
  // composer. Poll its retained value here, before a texture scene decides
  // that drawing can be skipped.
  impl.scanReleasedScalars();
  return impl.contentDirty || impl.needsLayout || impl.volatileDirty ||
         impl.ticker.active() || impl.rootVolatile;
}

void Composer::draw(SkCanvas& canvas) {
  Impl& impl = *m_impl;
  if (!impl.root) return;

  impl.stats.picturesRecorded = 0;
  impl.stats.texturesBaked = 0;
  impl.stats.nodesPainted = 0;
  impl.promotedBytesLast = impl.promotedBytes;
  impl.promotedBytes = 0;

  // Backend-aware promotion default (see ComposeRuntime.h). recorder() is
  // non-null on a Graphite canvas, recordingContext() on a Ganesh one.
  // Either means this surface is GPU-backed, and the cost signal automatic
  // promotion decides from — time spent recording draw ops — no longer
  // predicts what the surface actually pays, so promotion stays off unless
  // the host explicitly asked for it. When it flips OFF here, drop any bakes
  // taken on a previous raster frame so a host that alternates backends does
  // not blit a stale texture.
  const bool gpuBacked =
      canvas.recorder() != nullptr || canvas.recordingContext() != nullptr;
  const Composer::PromotionPolicy effective =
      (impl.promotionExplicit || !gpuBacked) ? impl.autoPromote
                                             : Composer::PromotionPolicy::Off;
  if (effective != impl.autoPromoteEffective) {
    impl.autoPromoteEffective = effective;
    if (effective == Composer::PromotionPolicy::Off && impl.root) {
      const auto clear = [](auto&& self, detail::Instance& inst) -> void {
        inst.autoTexture = false;
        inst.hotFrames = 0;
        // Cache::Group is the author's bake too, and it is not promotion:
        // dropping it here would cost a re-bake for a switch that has
        // nothing to say about it.
        if (inst.description && inst.description->cacheMode != Cache::Texture &&
            inst.description->cacheMode != Cache::Group)
          inst.textureImage.reset();
        for (auto& child : inst.children) self(self, *child);
      };
      clear(clear, *impl.root);
    }
  }

  // PaintContext::contentScale — device px per layout px under the host's
  // current transform (2.0 on a HiDPI-scaled canvas). Best effort: recordings
  // capture the scale current when they re-record.
  {
    // maxScaleOf, not the matrix diagonal: a host that rotates its canvas
    // reports getScaleX/Y == 0 at a quarter turn, and every material would
    // then be handed uContentScale = 1 no matter what the real zoom was.
    // The canvas rect is passed so the scale estimate samples the Jacobian
    // in the right place when the host matrix has perspective.
    const float s = detail::maxScaleOf(canvas.getTotalMatrix(),
                                       SkRect::MakeSize(impl.size));
    impl.hostScale = s > 0 ? s : 1.0f;
  }

  impl.stats.reconcileMs = impl.reconcileAccumMs;
  impl.reconcileAccumMs = 0;
  if (impl.profileEnabled) {
    impl.profileRows.clear();
    impl.profChildMs = 0;
    impl.profDepth = 0;
  }
  if (impl.countComposites) {
    // The plane indexes the canvas this frame is drawn on, in that
    // canvas's own device pixels, and it counts THIS draw: a frame's
    // blits are a fact about the frame, and a plane carried over from
    // the last one would report a picture nobody is looking at.
    const SkISize device = canvas.getBaseLayerSize();
    impl.compositePlane.width = device.width();
    impl.compositePlane.height = device.height();
    impl.compositePlane.counts.assign(
        (size_t)device.width() * (size_t)device.height(), 0);
  }

  sigil::measure::Laps laps;

  // The cascade before layout: an inheriting leaf must be set in its font
  // before it is measured, and a length in ems before Yoga reads it.
  if (impl.cascadeDirty) impl.runCascade();
  impl.ensureLayout();
  impl.stats.layoutMs = laps.mark("layout");

  // Volatility changes only on reconcile or while animations run (and once
  // more on the frame they settle) — skip the walk otherwise. Bindings the
  // host drives directly are the exception this scan covers: such an Output
  // can start moving while no motion is running and the walk is asleep, so
  // it has to re-declare its node volatile on the spot.
  impl.scanReleasedScalars();
  const bool active = impl.ticker.active();
  if (impl.volatileDirty || active || impl.tickerWasActive) {
    impl.releasedScalars.clear();  // the walk re-registers what stays released
    impl.rootVolatile = impl.computeVolatile(*impl.root).volatileAbove;
    impl.volatileDirty = false;
  }
  impl.tickerWasActive = active;
  impl.stats.volatileMs = laps.mark("volatile");

  // Output view transform: the composer's whole output renders into one
  // layer and composites through the view filter (an OCIO display/view baked
  // from a colour config, typically). Post-cache: per-node pictures replay
  // unchanged.
  //
  // A view described as a Material is lowered HERE, because how cheaply it
  // can run is a fact about the surface: a transform whose channels are
  // independent is a per-channel table on an eight-bit surface and a
  // full-canvas program on any other, and only the canvas knows which this
  // is. Lowered once per colour type — a host whose surface never changes
  // pays one enum comparison a frame.
  if (impl.viewMaterial) {
    const SkColorType surface = canvas.imageInfo().colorType();
    if (surface != impl.viewColorType) {
      impl.viewColorType = surface;
      impl.view = material::skia::Effect::recipe(*impl.viewMaterial, surface);
    }
  }
  const bool hasView = impl.view.imageFilter() || impl.view.colorFilter();
  if (hasView) {
    SkPaint viewPaint;
    viewPaint.setImageFilter(impl.view.imageFilter());
    viewPaint.setColorFilter(impl.view.colorFilter());
    canvas.saveLayer(nullptr, &viewPaint);
  }
  impl.paint(*impl.root, canvas);
  if (hasView) canvas.restore();
  impl.stats.paintMs = laps.mark("paint");
  impl.contentDirty = false;
  // Costliest first, AND THE LABEL BREAKS EVERY TIE. Two nodes that cost
  // the same — which most of a tree does, at or near zero — would
  // otherwise be left in whatever order an unstable sort happened to
  // produce, so a reader that prints this table draws a different table
  // on two runs of one binary and a byte-identity sweep reports it as
  // moved by a change that moved nothing. A label is the node's key, so
  // the tie-break is a fact of the description.
  if (impl.profileEnabled)
    std::stable_sort(impl.profileRows.begin(), impl.profileRows.end(),
                     [](const NodeCost& a, const NodeCost& b) {
                       if (a.selfMs != b.selfMs) return a.selfMs > b.selfMs;
                       return a.label < b.label;
                     });
}

void Composer::setProfiling(bool on) {
  m_impl->profileEnabled = on;
  if (!on) m_impl->profileRows.clear();
}

bool Composer::profiling() const { return m_impl->profileEnabled; }

void Composer::setCompositeCounting(bool on) {
  m_impl->countComposites = on;
  if (!on) m_impl->compositePlane = {};
}

bool Composer::compositeCounting() const { return m_impl->countComposites; }

const Composer::CompositePlane& Composer::compositePlane() const {
  return m_impl->compositePlane;
}

void Composer::setAutoTexturePromotion(bool on) {
  setAutoTexturePromotion(on ? PromotionPolicy::ByCost : PromotionPolicy::Off);
}

void Composer::setAutoTexturePromotion(PromotionPolicy policy) {
  m_impl->autoPromote = policy;
  m_impl->promotionExplicit = true;  // the host has an opinion; honour it on
                                     // every backend, overriding the default.
  if (policy == PromotionPolicy::Off && m_impl->root) {
    // Drop every promoted bake, and the counters that would re-promote from
    // where they left off, so turning promotion off actually exercises the
    // unpromoted path instead of blitting textures baked before the switch.
    const auto clear = [](auto&& self, detail::Instance& inst) -> void {
      inst.autoTexture = false;
      inst.hotFrames = 0;
      inst.replayMs = 0;
      inst.liveStableRate = 0;
      if (inst.description && inst.description->cacheMode != Cache::Texture &&
          inst.description->cacheMode != Cache::Group)
        inst.textureImage.reset();
      for (auto& child : inst.children) self(self, *child);
    };
    clear(clear, *m_impl->root);
  }
}

Composer::PromotionPolicy Composer::autoTexturePromotionPolicy() const {
  return m_impl->autoPromote;
}

bool Composer::autoTexturePromotion() const {
  return m_impl->autoPromote != PromotionPolicy::Off;
}

void Composer::setBakeDensity(float devicePixelsPerUnit) {
  const float density = devicePixelsPerUnit > 0 ? devicePixelsPerUnit : 0.0f;
  if (density == m_impl->bakeDensity) return;
  m_impl->bakeDensity = density;
  // The next draw has work to do, and a host that gates its draw on
  // dirty() reads exactly this flag: without it the scene keeps showing
  // rasters taken at the old density until something else moves.
  m_impl->contentDirty = true;
  // Every bake standing was taken at the old density, and none of them
  // will be re-taken by a scale change any more — so they go now, or the
  // scene keeps rasters nothing will ever revise.
  if (!m_impl->root) return;
  const auto clear = [](auto&& self, detail::Instance& inst) -> void {
    inst.textureImage.reset();
    inst.ownImage.reset();
    inst.paintDirty = true;
    inst.ownPaintDirty = true;
    for (auto& child : inst.children) self(self, *child);
  };
  clear(clear, *m_impl->root);
}

float Composer::bakeDensity() const { return m_impl->bakeDensity; }

void Composer::setPointer(SkPoint canvasPoint, bool pressed) {
  m_impl->pointerAt = canvasPoint;
  m_impl->pointerPressed = pressed;
}

void Composer::setKey(std::string_view name, int code, bool pressed) {
  KeyState& keys = m_impl->keys;
  std::erase(keys.down, code);
  if (pressed) {
    keys.down.push_back(code);
    keys.key.assign(name);
    keys.keyCode = code;
  }
  keys.pressed = !keys.down.empty();
}

const char* Composer::promotionReason(Promotion p) {
  switch (p) {
    case Promotion::Cheap:
      return "cheap enough to leave alone";
    case Promotion::Warming:
      return "expensive — counting frames before a bake";
    case Promotion::Promoted:
      return "baked by the library";
    case Promotion::AskedFor:
      return "Cache::Texture — you asked for it";
    case Promotion::OptedOut:
      return "promotion opted out";
    case Promotion::Volatile:
      return "its content changes every frame";
    case Promotion::Composited:
      return "opacity/blend — a bake would round twice; "
             "ask for Cache::Texture yourself";
    // The refusal is real: baking a rotated, mirrored or skewed node and
    // blitting the result differs from painting it live by about one least
    // significant bit on the antialiased edges of a shader fill, and the
    // library will not spend a caller's exactness without being asked. But a
    // constant small tilt is common — a band angled a fraction of a degree
    // never lies square — and such a node can be the most expensive thing in
    // the frame while looking ordinary. So the reason names the opt-in
    // instead of only describing the geometry, which would leave the author
    // with nothing to act on.
    case Promotion::Transformed:
      return "rotated, mirrored or skewed: a bake would differ by ~1 LSB on "
             "the antialiased edges, so the library will not take it for you "
             "— add .cache(Cache::Texture) if you accept that";
    case Promotion::Filtered:
      return "layer/backdrop effect or clip";
    case Promotion::ReadsBackdrop:
      return "something in this subtree blends with the canvas (a non-srcOver "
             "blend or backdrop filter, here or in a descendant)";
    case Promotion::TooBig:
      return "too large to bake, or over the bake budget";
    case Promotion::SplitBaked:
      return "own paint baked, volatile children painted live over the blit";
    case Promotion::HostsSpace:
      return "hosts a shared 3D space: its children are drawn on the plane "
             "beneath it, so it has no layer of its own to bake";
  }
  return "";
}

const std::vector<Composer::NodeCost>& Composer::profile() const {
  return m_impl->profileRows;
}

void Composer::purgeCaches() {
  Impl& impl = *m_impl;
  if (!impl.root) return;
  std::function<void(Instance&)> walk = [&walk](Instance& inst) {
    inst.picture.reset();
    inst.textureImage.reset();
    inst.ownImage.reset();  // the split bake's own-paint half is a cache too
    inst.bakedLiveShader.reset();
    inst.hasPendingLiveFill = false;
    inst.paintDirty = true;
    inst.ownPaintDirty = true;
    for (auto& child : inst.children) walk(*child);
  };
  walk(*impl.root);
  impl.contentDirty = true;
  impl.volatileDirty = true;
}

std::optional<SkRect> Composer::bounds(std::string_view key) const {
  auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end()) return std::nullopt;
  // Accumulate offsets up the yoga tree.
  SkRect rect = m_impl->instanceRect(*it->second);
  // Layout runs inside draw(), so a query issued between a render() and the
  // next draw() reads a tree that has not been laid out and whose rects are
  // non-finite. Reporting absent is the honest answer; handing back a rect
  // with a NaN extent would be a number the caller cannot tell from a real
  // one.
  if (!rect.isFinite()) return std::nullopt;
  for (Instance* p = it->second->parent; p; p = p->parent) {
    const SkRect parentRect = m_impl->instanceRect(*p);
    rect.offset(parentRect.left(), parentRect.top());
  }
  return rect;
}

const sigil::weave::ParagraphLayout* Composer::paragraphLayout(
    std::string_view key) const {
  auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end() || !it->second->paragraph) return nullptr;
  return &it->second->textLayout;
}

TextSettling Composer::settling(std::string_view key) const {
  auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end() || !it->second->paragraph) return {};
  const detail::Instance& inst = *it->second;
  return {.live = inst.description && inst.description->textData &&
                  (inst.description->textData->options.set &
                   detail::TextOptions::kLive) != 0 &&
                  inst.description->textData->options.live,
          .reused = inst.textReusedBlocks,
          .degraded = inst.textDegradedBlocks};
}

std::vector<Beat> Composer::beatsOf(std::string_view key,
                                    size_t trackIndex) const {
  auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end()) return {};
  // Logically const: resolving a schedule fills the same per-instance
  // scratch the painter does and changes nothing the next draw can see.
  // The handle is a unique_ptr, so a const method still reaches a
  // non-const Impl through it.
  Impl& impl = *m_impl;
  const TextPainterOperations* painter = Impl::textPainterOf(*it->second);
  if (!painter) return {};  // text at rest runs no schedule
  std::vector<Beat> beats = painter->beats(*it->second, trackIndex);
  if (beats.empty()) return beats;
  // Rects come out in the node's own space. Lift them into the composer's,
  // by the same walk up the tree the bounds query takes — a beat is a place
  // on the SHEET, so it can be read beside `bounds()` and `hitTest()`
  // without the caller knowing which node the glyphs belong to.
  SkPoint origin{0, 0};
  for (Instance* node = it->second; node; node = node->parent) {
    const SkRect rect = impl.instanceRect(*node);
    if (!rect.isFinite()) return {};  // laid out by nothing yet
    origin.offset(rect.left(), rect.top());
  }
  for (Beat& beat : beats) beat.rect.offset(origin.x(), origin.y());
  return beats;
}

std::vector<TextUnit> Composer::units(std::string_view key,
                                      const sigil::weave::Selector& selector,
                                      sigil::weave::Unit unit) const {
  auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end()) return {};
  // Logically const: resolving the units fills the same per-instance
  // scratch the painter does and changes nothing the next draw can see.
  Impl& impl = *m_impl;
  // A passage that dresses nothing carries no painter, and it still has
  // units to report — so the engine the typography tier registered answers
  // for it.
  const TextPainterOperations* painter = Impl::textPainterOf(*it->second);
  if (!painter) painter = detail::registeredTextEngine();
  if (!painter) return {};
  std::vector<TextUnit> units = painter->units(*it->second, selector, unit);
  if (units.empty()) return units;
  // Rects come out in the node's own space; the same walk up the tree the
  // bounds and beat queries take lifts them into the composer's, so a
  // sibling reading them stands where the glyphs do.
  SkPoint origin{0, 0};
  for (Instance* node = it->second; node; node = node->parent) {
    const SkRect rect = impl.instanceRect(*node);
    if (!rect.isFinite()) return {};  // laid out by nothing yet
    origin.offset(rect.left(), rect.top());
  }
  for (TextUnit& entry : units) {
    entry.rect.offset(origin.x(), origin.y());
    entry.axis += entry.writingMode == sigil::weave::WritingMode::kVerticalRL
                      ? origin.x()
                      : origin.y();
  }
  return units;
}

float Composer::cascadeSpanMs(std::string_view key, size_t trackIndex) const {
  auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end()) return 0.0f;
  // Logically const: resolving a schedule fills the same per-instance
  // scratch the painter does and changes nothing the next draw can see.
  Impl& impl = *m_impl;
  const TextPainterOperations* painter = Impl::textPainterOf(*it->second);
  return painter ? painter->cascadeSpanMs(*it->second, trackIndex) : 0.0f;
}

std::optional<std::string> Composer::hitTest(SkPoint canvasPoint) const {
  // Logically const; fills the same per-instance outline caches paint does
  // (memoization, not mutation of observable state).
  Impl& impl = *m_impl;
  if (!impl.root) return std::nullopt;
  return impl.hitInstance(*impl.root, canvasPoint, nullptr, nullptr);
}

const Composer::Stats& Composer::stats() const {
  // Tree tallies are computed on demand, never in the frame loop.
  size_t instances = 0, pictures = 0, textures = 0, yogaNodes = 0;
  std::function<void(const Instance&)> tally = [&](const Instance& i) {
    ++instances;
    if (i.yoga) ++yogaNodes;
    if (i.picture) ++pictures;
    if (i.textureImage) ++textures;
    for (const auto& child : i.children) tally(*child);
  };
  if (m_impl->root) tally(*m_impl->root);
  const core::ReconcileStats& reconcile = m_impl->reconciler.stats();
  m_impl->stats.describedNodes = (size_t)reconcile.describedNodes;
  m_impl->stats.memoHits = (size_t)reconcile.memoHits;
  m_impl->stats.patchedNodes = (size_t)reconcile.patchedNodes;
  m_impl->stats.instances = instances;
  m_impl->stats.yogaNodes = yogaNodes;
  m_impl->stats.picturesLive = pictures;
  m_impl->stats.texturesLive = textures;
  return m_impl->stats;
}

}  // namespace sigil::compose
