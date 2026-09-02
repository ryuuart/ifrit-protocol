/** @file
 * The Composer facade: the public retained-side surface (construct with a
 * Ticker + FontContext, render() on data change, draw(canvas) inside the
 * host's paint callback), the Impl/Instance lifecycle, and snapshot() (the
 * one-shot element-tree-as-a-brush bake). The phase machinery lives in the
 * sibling TUs — Reconcile, Layout, Derive, Transitions and Query, and the
 * paint phase's own files — all sharing ComposeRuntime.h.
 */

#include <include/core/SkBBHFactory.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkTypes.h>  // SkDebugf — the renderSlot diagnostic
#include <sigilmeasure/time/Laps.h>
#include <sigilmeasure/time/Stopwatch.h>
#include <sigilweave/choreograph/Choreograph.h>  // forEachPlacedGlyph — measureRun()
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/layout/ParagraphLayout.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// One ElementNode is allocated per described node, every render, for the
// whole tree — so its size is a per-frame allocation cost, not a detail.
// Rare and kind-specific state therefore lives in separately allocated
// blocks (ComposeInternal.h) that most nodes never carry, and only hot
// fields sit inline. This assert is the gate: a field added to the base
// struct instead of a block fails the build rather than quietly taxing
// every node in every tree.
static_assert(sizeof(ElementNode) <= 768,
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

sk_sp<SkPicture> snapshot(const Element& root, sigil::weave::FontContext& fonts,
                          SkSize maxSize) {
  motion::Ticker ticker;  // inert: nothing steps it, transitions can't run
  Composer composer(ticker, fonts);
  Composer::Impl& impl = *composer.m_impl;
  impl.liveOnly = true;  // one-shot: per-node caches would be pure waste
  composer.render(root);
  if (!impl.root) return nullptr;
  if (!maxSize.isEmpty()) {
    if (maxSize.width() > 0)
      YGNodeStyleSetMaxWidth(impl.root->yoga, maxSize.width());
    if (maxSize.height() > 0)
      YGNodeStyleSetMaxHeight(impl.root->yoga, maxSize.height());
  }
  impl.ensureLayout();
  const SkRect rect = impl.instanceRect(*impl.root);
  if (rect.isEmpty()) return nullptr;
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(SkRect::MakeWH(rect.width(), rect.height()));
  impl.paint(*impl.root, *canvas);
  return recorder.finishRecordingAsPicture();
}

namespace tiles {

SkMatrix window(SkISize tile, int index, Flow flow, Facing facing) {
  const float w = (float)tile.width();
  const float h = (float)tile.height();
  const float step = -(float)index * (flow == Flow::Down ? h : w);
  // The step runs ALONG the flow; the mirror, when asked for, runs ACROSS
  // it — the axis perpendicular to the slicing. Both are written out as
  // one matrix so no call site has to get the concat order right.
  if (flow == Flow::Down) {
    return facing == Facing::Mirrored
               ? SkMatrix::MakeAll(-1, 0, w, 0, 1, step, 0, 0, 1)
               : SkMatrix::MakeAll(1, 0, 0, 0, 1, step, 0, 0, 1);
  }
  return facing == Facing::Mirrored
             ? SkMatrix::MakeAll(1, 0, step, 0, -1, h, 0, 0, 1)
             : SkMatrix::MakeAll(1, 0, step, 0, 1, 0, 0, 0, 1);
}

sk_sp<SkPicture> sliceable(const sk_sp<SkPicture>& art) {
  if (!art) return nullptr;
  SkRTreeFactory rtree;
  SkPictureRecorder recorder;
  // playback(), NOT drawPicture(): drawPicture on a recording canvas stores
  // a nested reference the hierarchy cannot index into, which leaves the
  // tree empty and the slice exactly as expensive as before.
  art->playback(recorder.beginRecording(art->cullRect(), &rtree));
  return recorder.finishRecordingAsPicture();
}

}  // namespace tiles

TextMetrics metrics(const sigil::weave::TextStyle& style,
                    sigil::weave::FontContext& fonts) {
  SkFont font(
      style.shaping.typeface ? style.shaping.typeface : fonts.defaultTypeface(),
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

std::vector<float> measureRun(std::u8string_view utf8,
                              const sigil::weave::TextStyle& style,
                              sigil::weave::FontContext& fonts) {
  std::vector<float> advances;
  if (utf8.empty()) return advances;
  // The exact machinery a text() leaf runs (layoutText, Layout.cpp): one
  // Paragraph, one unconstrained single-line layout, the placed glyphs in
  // order. Only the Element is skipped — which is the point. A caller
  // placing glyphs by hand needs the advances, and the alternative to this
  // is a whole paragraph layout per glyph.
  sigil::weave::Paragraph paragraph;
  paragraph.appendText(utf8, style);
  static const sigil::weave::ParagraphLayoutOptions kOptions;
  sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
  sigil::weave::ParagraphLayout layout =
      sigil::weave::layoutParagraph(fonts, paragraph, flow, kOptions);
  // An inter-word space is a GAP the flow leaves between positioned runs,
  // not a glyph, so it visits nothing here. Left out, every glyph after a
  // space would be placed short by the accumulated space advances and the
  // error would grow with each word. Whatever the layout left between one
  // glyph's pen end and the next one's origin therefore rides the advance
  // of the glyph it follows, which is what makes the prefix sums reproduce
  // the pen positions the layout used.
  //
  // Two steps are not glue and are not folded: a line change (a '\n' in the
  // run) restarts the pen, and a BACKWARDS step between two words is
  // bidi reordering rather than a gap — visual order runs the other way
  // there and no prefix sum can express it. Inside one word a backwards
  // step is ordinary kerning and counts.
  float pen = 0;
  int lineIndex = -1;
  uint32_t wordIndex = 0;
  sigil::weave::forEachPlacedGlyph(
      layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const float step = placed.rest.x() - pen;
        if (placed.lineIndex != lineIndex) {
          lineIndex = placed.lineIndex;
        } else if (!advances.empty() &&
                   (placed.wordIndex == wordIndex || step > 0)) {
          advances.back() += step;
        }
        advances.push_back(placed.advance);
        pen = placed.rest.x() + placed.advance;
        wordIndex = placed.wordIndex;
      });
  return advances;
}

std::vector<float> runPens(std::u8string_view utf8,
                           const sigil::weave::TextStyle& style,
                           sigil::weave::FontContext& fonts) {
  const std::vector<float> advances = measureRun(utf8, style, fonts);
  std::vector<float> pens;
  pens.reserve(advances.size() + 1);
  float pen = 0;
  for (const float advance : advances) {
    pens.push_back(pen);
    pen += advance;
  }
  // The past-the-end entry, which is what makes the last advance readable
  // the same way every other one is and hands back the run's width for
  // free. An empty run reaches here with nothing summed and answers 0,
  // which is its width.
  pens.push_back(pen);
  return pens;
}

SkSize measure(const Element& root, sigil::weave::FontContext& fonts,
               SkSize maxSize) {
  motion::Ticker ticker;  // inert — same sampling rules as snapshot()
  Composer composer(ticker, fonts);
  Composer::Impl& impl = *composer.m_impl;
  impl.liveOnly = true;
  composer.render(root);
  if (!impl.root) return SkSize::MakeEmpty();
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
  if (m_impl->size == size) return;
  m_impl->size = size;
  m_impl->needsLayout = true;
  m_impl->contentDirty = true;
}

void Composer::setClock(const motion::FrameClock* clock) {
  m_impl->clock = clock;
}

void Composer::setView(material::skia::Effect view) {
  m_impl->view = std::move(view);
  m_impl->contentDirty = true;  // the composite changes even if no node did
}

void Composer::setView(const sigil::material::Material& view) {
  setView(material::skia::Effect::recipe(view));
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
  static bool warned = false;
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
  impl.volatileDirty = true;  // transitions may have started
  impl.rebuildKeyIndex();
  impl.reconcileAccumMs += reconcile.elapsedMs();
}

void Composer::renderSlot(std::string_view name, const Element& content) {
  Impl& impl = *m_impl;
  const sigil::measure::Stopwatch reconcile;
  auto it = impl.bySlot.find(std::string(name));
  if (it == impl.bySlot.end()) {
    // A miss must be loud, because the SYMPTOM points somewhere else: an
    // empty slot lays out W x 0, which reads as a layout bug and sends the
    // reader into Yoga rather than to the name they typed.
    //
    // The likely cause is the one the message names: `slot(name)` stores the
    // name in `key`, so any later `.key(...)` on that element renames the
    // slot with no type error and no second field to disagree with itself.
    // Listing the names that DO exist turns the diagnosis into one read.
    static std::set<std::string> warned;  // once per name, not per frame
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
  impl.rebuildKeyIndex();
  impl.reconcileAccumMs += reconcile.elapsedMs();
}

bool Composer::dirty() const {
  return m_impl->contentDirty || m_impl->needsLayout;
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
  const bool effective = impl.promotionExplicit
                             ? impl.autoPromote
                             : impl.autoPromote && !gpuBacked;
  if (effective != impl.autoPromoteEffective) {
    impl.autoPromoteEffective = effective;
    if (!effective && impl.root) {
      const auto clear = [](auto&& self, detail::Instance& inst) -> void {
        inst.autoTexture = false;
        inst.hotFrames = 0;
        // Cache::Group is the author's bake too, and it is not promotion:
        // dropping it here would cost a re-bake for a switch that has
        // nothing to say about it.
        if (inst.desc && inst.desc->cacheMode != Cache::Texture &&
            inst.desc->cacheMode != Cache::Group)
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

  sigil::measure::Laps laps;

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
    impl.computeVolatile(*impl.root);
    impl.volatileDirty = false;
  }
  impl.tickerWasActive = active;
  impl.stats.volatileMs = laps.mark("volatile");

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
  if (hasView) canvas.restore();
  impl.stats.paintMs = laps.mark("paint");
  impl.contentDirty = false;
  if (impl.profileEnabled)
    std::sort(impl.profileRows.begin(), impl.profileRows.end(),
              [](const NodeCost& a, const NodeCost& b) {
                return a.selfMs > b.selfMs;
              });
}

void Composer::setProfiling(bool on) {
  m_impl->profileEnabled = on;
  if (!on) m_impl->profileRows.clear();
}

bool Composer::profiling() const { return m_impl->profileEnabled; }

void Composer::setAutoTexturePromotion(bool on) {
  m_impl->autoPromote = on;
  m_impl->promotionExplicit = true;  // the host has an opinion; honour it on
                                     // every backend, overriding the default.
  if (!on && m_impl->root) {
    // Drop every promoted bake, and the counters that would re-promote from
    // where they left off, so turning promotion off actually exercises the
    // unpromoted path instead of blitting textures baked before the switch.
    const auto clear = [](auto&& self, detail::Instance& inst) -> void {
      inst.autoTexture = false;
      inst.hotFrames = 0;
      inst.replayMs = 0;
      inst.liveStableRate = 0;
      if (inst.desc && inst.desc->cacheMode != Cache::Texture &&
          inst.desc->cacheMode != Cache::Group)
        inst.textureImage.reset();
      for (auto& child : inst.children) self(self, *child);
    };
    clear(clear, *m_impl->root);
  }
}

bool Composer::autoTexturePromotion() const { return m_impl->autoPromote; }

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
  auto it = m_impl->byKey.find(std::string(key));
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
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end() || !it->second->paragraph) return nullptr;
  return &it->second->textLayout;
}

TextSettling Composer::settling(std::string_view key) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end() || !it->second->paragraph) return {};
  const detail::Instance& inst = *it->second;
  return {.live = inst.desc && inst.desc->textData &&
                  (inst.desc->textData->options.set &
                   detail::TextOptions::kLive) != 0 &&
                  inst.desc->textData->options.live,
          .reused = inst.textReusedBlocks,
          .degraded = inst.textDegradedBlocks};
}

std::vector<Beat> Composer::beatsOf(std::string_view key,
                                    size_t trackIndex) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end()) return {};
  // Logically const: resolving a schedule fills the same per-instance
  // scratch the painter does and changes nothing the next draw can see.
  Impl& impl = const_cast<Impl&>(*m_impl);
  const TextPainterOps* painter = Impl::textPainterOf(*it->second);
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
                                      const Selector& selector,
                                      Unit unit) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end()) return {};
  // Logically const: resolving the units fills the same per-instance
  // scratch the painter does and changes nothing the next draw can see.
  Impl& impl = const_cast<Impl&>(*m_impl);
  // A passage that dresses nothing carries no painter, and it still has
  // units to report — so the engine the typography tier registered answers
  // for it.
  const TextPainterOps* painter = Impl::textPainterOf(*it->second);
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
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end()) return 0.0f;
  // Logically const: resolving a schedule fills the same per-instance
  // scratch the painter does and changes nothing the next draw can see.
  Impl& impl = const_cast<Impl&>(*m_impl);
  const TextPainterOps* painter = Impl::textPainterOf(*it->second);
  return painter ? painter->cascadeSpanMs(*it->second, trackIndex) : 0.0f;
}

std::optional<std::string> Composer::hitTest(SkPoint canvasPoint) const {
  // Logically const; fills the same per-instance outline caches paint does
  // (memoization, not mutation of observable state).
  Impl& impl = const_cast<Impl&>(*m_impl);
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
