/** @file
 * The Composer facade: the public retained-side surface — construct with a
 * Engine + FontContext, render() on data change, draw(canvas) inside the
 * host's paint callback — and the Impl/Instance lifecycle under it. The
 * phase machinery lives in runtime/, style/, layout/, paint/ and cache/;
 * the verbs that take a tree without a live composer are in layout/.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkTypes.h>  // SkDebugf — the renderSlot diagnostic
#include <include/gpu/graphite/Recorder.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmeasure/advanced/Laps.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/layout/ParagraphLayout.h>
#if defined(SK_GRAPHITE)
#include <src/gpu/graphite/RecorderPriv.h>
#endif

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <functional>

#include "ComposeRuntime.h"
#include "paint/SurfaceMap.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** Whether every rect of @p inner stands inside one rect of @p outer. */
bool covers(const std::vector<SkRect>& outer,
            const std::vector<SkRect>& inner) {
  for (const SkRect& part : inner) {
    bool inside = false;
    for (const SkRect& whole : outer) inside |= whole.contains(part);
    if (!inside) return false;
  }
  return true;
}

}  // namespace

// Every described node allocates an ElementNode. Common fields stay inline;
// rare and kind-specific state belongs in optional blocks. The size cap
// prevents a new rare feature from increasing every node's allocation.
static_assert(sizeof(ElementNode) <= 864,
              "ElementNode grew — put rare fields in a block");

// ---------------------------------------------------------------------------
// Instance lifecycle

detail::Instance::~Instance() {
  if (yoga)
    YGNodeRemoveAllChildren(yoga);  // detach before children free theirs
  children.clear();
  if (yoga) YGNodeFree(yoga);
  // HeldMotion outputs die here; Choreograph disconnects their motions
  // automatically — unmount cancels transitions by construction.
}

// ---------------------------------------------------------------------------
// Composer public surface

Composer::Composer(motion::Engine& engine, sigil::weave::FontContext& fonts)
    : m_impl(std::make_unique<Impl>(engine, fonts)) {}
Composer::~Composer() = default;

void Composer::setSize(glm::vec2 requested) {
  const SkSize size = SkSize::Make(requested.x, requested.y);
  if (m_impl->size == size) return;
  m_impl->size = size;
  m_impl->needsLayout = true;
  m_impl->contentDirty = true;
  m_impl->changesUnknown = true;
}

void Composer::setView(material::Filter view) {
  m_impl->viewMaterial.reset();  // an Effect is already lowered
  m_impl->view = std::move(view);
  m_impl->contentDirty = true;  // the composite changes even if no node did
  m_impl->changesUnknown = true;
}

void Composer::setView(const sigil::material::Material& view) {
  // The program is the answer for a surface nothing is known about, and
  // is what stands until draw meets a canvas that says otherwise.
  setView(material::Filter::of(view));
  m_impl->viewMaterial = view;
  m_impl->viewColorType = kUnknown_SkColorType;
}

void Composer::render(const Element& root) {
  Impl& impl = *m_impl;
  const sigil::measure::Stopwatch reconcile;
  impl.reconciler.render(impl.root, root.node());
  impl.settledForDraw = false;
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
  // THE CASCADE BELONGS TO THE DESCRIBE, not to the draw that asks for a
  // layout, and the reason is the clock. Both passes start transition
  // lanes — the patch above for a node whose own description moved, this
  // one for a node whose answer moved above it — so a frame that ran them
  // at two different moments would start an ink and a fill of one duration
  // on one node a tick apart and settle them on different frames. Layout
  // asks again for anything mounted or invalidated after this point, and a
  // running ink transition leaves the flag up so the next frame resolves
  // again.
  if (impl.cascadeDirty) impl.runCascade();
  impl.reconcileAccumMs +=
      sigil::measure::Milliseconds(reconcile.elapsed()).count();
}

void Composer::renderSlot(std::string_view name, const Element& content) {
  Impl& impl = *m_impl;
  impl.settledForDraw = false;
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
  impl.reconcileAccumMs +=
      sigil::measure::Milliseconds(reconcile.elapsed()).count();
}

void Composer::setInherited(const sigil::weave::Type& font,
                            material::Color ink) {
  Impl& impl = *m_impl;
  sigil::weave::Type root =
      sigil::weave::overlay(sigil::weave::initialType(), font);
  root.color = ink;
  if (root == impl.rootFont) return;
  impl.rootFont = std::move(root);
  impl.rootLineHeight = 0.0f;
  impl.cascadeDirty = true;
  impl.contentDirty = true;
  impl.changesUnknown = true;
}

bool Composer::dirty() const {
  return m_impl->contentDirty || m_impl->needsLayout;
}

bool Composer::isRunning() const {
  Impl& impl = *m_impl;
  // A settled external binding has no reconciliation event to wake the
  // composer. Poll its retained value here, before a texture scene decides
  // that drawing can be skipped.
  impl.scanReleasedScalars();
  return impl.contentDirty || impl.needsLayout || impl.volatileDirty ||
         impl.engine.isRunning() || impl.rootVolatile;
}

void Composer::Impl::settleForDraw() {
  ensureLayout();
  if (cascadeDirty) runCascade();
  // Volatility changes only on reconcile or while animations run (and once
  // more on the frame they settle) — skip the walk otherwise. Bindings the
  // host drives directly are the exception this scan covers: such a live value
  // can start moving while no motion is running and the walk is asleep, so
  // it has to re-declare its node volatile on the spot.
  scanReleasedScalars();
  const bool active = engine.isRunning();
  if (volatileDirty || active || engineWasRunning) {
    releasedScalars.clear();  // the walk re-registers what stays released
    rootVolatile = computeVolatile(*root).volatileAbove;
    volatileDirty = false;
  }
  engineWasRunning = active;
}

std::optional<std::vector<geometry::path::Rect>> Composer::changedRegion() {
  Impl& impl = *m_impl;
  // A composer that bakes pixels keys its bakes to the clip they were cut
  // under, so a draw clipped to a region would cut them again: it answers
  // the whole canvas and keeps nothing.
  if (!impl.root || impl.bakesPixels()) return std::nullopt;
  impl.trackChanges = true;
  impl.regionPromised = true;
  impl.promisedRegion.reset();
  impl.settleForDraw();
  impl.settledForDraw = true;
  if (impl.changesUnknown) return std::nullopt;
  impl.promisedRegion = impl.changedRegion(false);
  if (!impl.promisedRegion) return std::nullopt;
  std::vector<geometry::path::Rect> out;
  out.reserve(impl.promisedRegion->size());
  for (const SkRect& part : *impl.promisedRegion)
    out.push_back(geometry::path::fromSk(part));
  return out;
}

bool Composer::drewWithinChangedRegion() const {
  return m_impl->drewWithinPromise;
}

void Composer::draw(SkCanvas& canvas) {
  Impl& impl = *m_impl;
  if (!impl.root) return;
  // A page painted as a surface map is drawn through the canvas that turns
  // content that is its own colour into the map's reading of it.
  if (impl.surfaceMap && !impl.drawingSurfaceMap) {
    detail::SurfaceMapCanvas mapped(canvas, *impl.surfaceMap,
                                    impl.surfaceMapScale);
    impl.drawingSurfaceMap = true;
    draw(mapped);
    impl.drawingSurfaceMap = false;
    return;
  }
  const SkAutoCanvasRestore restore(&canvas, true);

  const SkImageInfo destination = canvas.imageInfo();
  const SkImageInfo output = destination.colorType() == kUnknown_SkColorType
                                 ? SkImageInfo::MakeN32Premul(1, 1)
                                 : destination.makeWH(1, 1);
  skgpu::graphite::Recorder* recorder = canvas.recorder();
  uint32_t recorderId = 0;
#if defined(SK_GRAPHITE)
  if (recorder) recorderId = recorder->priv().uniqueID();
#endif
  GrRecordingContext* context = canvas.recordingContext();
  if (impl.outputInfo.colorInfo() != output.colorInfo() ||
      impl.outputRecorder != recorder || impl.outputRecorderId != recorderId ||
      impl.outputContext != context) {
    impl.outputInfo = output;
    impl.outputRecorder = recorder;
    impl.outputRecorderId = recorderId;
    impl.outputContext = context;
    purgeCaches();
  }

  impl.stats.picturesRecorded = 0;
  impl.stats.texturesBaked = 0;
  impl.stats.nodesPainted = 0;
  impl.stats.nodesBounded = 0;
  impl.bakedBytesLast = impl.bakedBytes;
  impl.bakedBytes = 0;
  impl.recording.groupPending = false;

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
  sigil::measure::Laps laps;

  if (!impl.settledForDraw) impl.settleForDraw();
  impl.settledForDraw = false;
  impl.stats.layoutMs =
      sigil::measure::Milliseconds(laps.mark("layout")).count();
  impl.stats.layoutCalculations = impl.layoutCalculations;
  impl.layoutCalculations = 0;
  impl.stats.volatileMs =
      sigil::measure::Milliseconds(laps.mark("volatile")).count();

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
      impl.view = material::skia::lowered(*impl.viewMaterial, surface);
    }
  }
  const bool hasView = material::skia::imageFilter(impl.view) ||
                       material::skia::colorFilter(impl.view);
  if (hasView) {
    SkPaint viewPaint;
    viewPaint.setImageFilter(material::skia::imageFilter(impl.view));
    viewPaint.setColorFilter(material::skia::colorFilter(impl.view));
    canvas.saveLayer(nullptr, &viewPaint);
  }
  impl.paint(*impl.root, canvas);
  if (hasView) canvas.restore();
  // The extents this draw left, kept for the next question; and whether
  // what it painted stayed inside the region the host was told before it.
  if (impl.trackChanges) {
    const std::optional<std::vector<SkRect>> painted = impl.changedRegion(true);
    impl.drewWithinPromise =
        !impl.regionPromised || !impl.promisedRegion ||
        (painted && covers(*impl.promisedRegion, *painted));
    impl.changesUnknown = hasView;
  }
  impl.regionPromised = false;
  impl.stats.paintMs = sigil::measure::Milliseconds(laps.mark("paint")).count();
  impl.contentDirty = impl.recording.groupPending;
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

void Composer::setBakeDensity(float devicePixelsPerUnit) {
  const float density = devicePixelsPerUnit > 0 ? devicePixelsPerUnit : 0.0f;
  if (density == m_impl->bakeDensity) return;
  m_impl->bakeDensity = density;
  m_impl->changesUnknown = true;
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
    inst.paintDirty = true;
    for (auto& child : inst.children) self(self, *child);
  };
  clear(clear, *m_impl->root);
}

float Composer::bakeDensity() const { return m_impl->bakeDensity; }

void Composer::setPointer(glm::vec2 canvasPoint, bool pressed) {
  m_impl->pointerAt = SkPoint{canvasPoint.x, canvasPoint.y};
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

const std::vector<Composer::NodeCost>& Composer::profile() const {
  return m_impl->profileRows;
}

void Composer::setSurfaceMap(std::optional<material::texture::Role> role) {
  Impl& impl = *m_impl;
  if (impl.surfaceMap == role) return;
  impl.surfaceMap = role;
  purgeCaches();
  // Every paint a lit site resolved for the map before is for that map;
  // the cascade resolves the ink again and the fill is keyed by the map.
  if (impl.root) {
    std::function<void(Instance&)> walk = [&walk](Instance& inst) {
      inst.inkPaint.paint.reset();
      inst.surfaceMapFill.reset();
      for (auto& child : inst.children) walk(*child);
    };
    walk(*impl.root);
  }
  impl.cascadeDirty = true;
}

void Composer::setSurfaceMapScale(float scale) {
  const float before = m_impl->surfaceMapScale;
  m_impl->surfaceMapScale = std::isfinite(scale) && scale > 0 ? scale : 1.0f;
  if (m_impl->surfaceMapScale != before) m_impl->changesUnknown = true;
}

float Composer::surfaceMapScale() const { return m_impl->surfaceMapScale; }

std::optional<material::texture::Role> Composer::surfaceMap() const {
  return m_impl->surfaceMap;
}

void Composer::purgeCaches() {
  Impl& impl = *m_impl;
  impl.changesUnknown = true;
  impl.bakeSurfaces.clear();
  impl.bakedBytes = 0;
  impl.bakedBytesLast = 0;
  if (!impl.root) return;
  std::function<void(Instance&)> walk = [&walk](Instance& inst) {
    inst.picture.reset();
    inst.textureImage.reset();
    inst.bakedLiveShader.reset();
    inst.hasPendingLiveFill = false;
    inst.stampCache = {};
    inst.paintDirty = true;
    for (auto& child : inst.children) walk(*child);
  };
  walk(*impl.root);
  impl.contentDirty = true;
  impl.volatileDirty = true;
}

std::optional<geometry::path::Rect> Composer::bounds(
    std::string_view key) const {
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
  return geometry::path::fromSk(rect);
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
  return {.live = (inst.textOptions.set & detail::TextOptions::kLive) != 0 &&
                  inst.textOptions.live,
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
  for (Beat& beat : beats)
    beat.rect = beat.rect.translated(geometry::path::fromSk(origin));
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
    entry.rect = entry.rect.translated(geometry::path::fromSk(origin));
    entry.axis += entry.writingMode == sigil::weave::WritingMode::kVerticalRL
                      ? origin.x()
                      : origin.y();
  }
  return units;
}

motion::Duration Composer::scheduleSpan(std::string_view key,
                                        size_t trackIndex) const {
  auto it = m_impl->byKey.find(key);
  if (it == m_impl->byKey.end()) return {};
  // Logically const: resolving a schedule fills the same per-instance
  // scratch the painter does and changes nothing the next draw can see.
  Impl& impl = *m_impl;
  const TextPainterOperations* painter = Impl::textPainterOf(*it->second);
  return painter ? painter->scheduleSpan(*it->second, trackIndex)
                 : motion::Duration{};
}

std::optional<std::string> Composer::hitTest(glm::vec2 canvasPoint) const {
  // Logically const; fills the same per-instance outline caches paint does
  // (memoization, not mutation of observable state).
  Impl& impl = *m_impl;
  if (!impl.root) return std::nullopt;
  return impl.hitInstance(*impl.root, SkPoint{canvasPoint.x, canvasPoint.y},
                          nullptr, nullptr);
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
