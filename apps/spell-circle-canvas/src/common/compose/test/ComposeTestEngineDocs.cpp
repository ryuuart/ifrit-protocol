#include "ComposeTestSupport.h"

TEST(ComposeBrushEngine, PipelineStylesEveryLayer) {
  Host host;
  Brush b;
  b.shaped(kit::brush::shapers::Wave{.amplitude = 8, .wavelength = 24})
      .layer(util::stroke(2, green()))
      .layer([] {
        brush::Scatter s;
        s.art = box().width(6).height(6).fill(red());
        s.spacing = 40;
        return s;
      }());
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  int off = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) off += host.pixel(x, 100 + dy) == SK_ColorGREEN;
  EXPECT_GT(off, 10);  // the stroke layer rides the waved pipeline
  int reds = 0;        // and the scatter layer rides the SAME waved geometry
  for (int x = 24; x < 176; ++x)
    for (int y = 84; y < 116; ++y) reds += host.pixel(x, y) == SK_ColorRED;
  EXPECT_GT(reds, 30);
}

TEST(ComposeBrushEngine, BrushPrunesAsOneValue) {
  Host host;
  auto tree = [] {
    Brush b;
    b.shaped(kit::brush::shapers::Rounded{6})
        .shaped(kit::brush::shapers::Wave{.amplitude = 3, .wavelength = 30})
        .layer(lines::cased(3, Fill::color({0, 1, 0, 1}), 5));
    return box().child(
        box().absolute().inset(40, 40, 40, 40).stroke(std::move(b)));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());  // fresh Elements, identical brush values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeBrushEngine, SketchyKeepsOpenContoursOpen) {
  // Under a fill StrokeRec, SkDiscretePathEffect force-closes open
  // contours — the phantom-channel bug. Hairline rec keeps them open.
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(300, 0);
  const SkPath jittered =
      kit::brush::shapers::Jitter{8, 2, 11}.shape(b.detach());
  SkContourMeasureIter iter(jittered, false);
  float total = 0;
  bool anyClosed = false;
  while (sk_sp<SkContourMeasure> c = iter.next()) {
    total += c->length();
    anyClosed |= c->isClosed();
  }
  EXPECT_FALSE(anyClosed);
  EXPECT_LT(total, 400.0f);  // a closed loop would be ~2× the 300px run
}

TEST(ComposeBrushEngine, PerLayerShapersRideTheSharedPipeline) {
  // One Brush, two layers offset to opposite sides — the asymmetric casing
  // as a single material value. Positive `px` is LEFT of travel.
  Host host;
  Brush b;
  b.layer(util::stroke(3, green()), {kit::brush::shapers::Offset{12}})
      .layer(util::stroke(3, blue()), {kit::brush::shapers::Offset{-12}});
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 88), SK_ColorGREEN);   // left-of-travel rail
  EXPECT_EQ(host.pixel(100, 112), SK_ColorBLUE);   // right-of-travel rail
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // nothing on the axis
}

TEST(ComposeBrushEngine, SquareWaveHoldsPlateausAndEndsOnAxis) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(320, 0);
  const SkPath boxy = kit::brush::shapers::Square{8, 80}.shape(b.detach());
  // Plateaus hold ±8 for half-wavelength runs; endpoints return to 0.
  const SkRect bounds = boxy.getBounds();
  EXPECT_NEAR(bounds.top(), -8, 0.5f);
  EXPECT_NEAR(bounds.bottom(), 8, 0.5f);
  SkPoint last;
  SkContourMeasureIter iter(boxy, false);
  sk_sp<SkContourMeasure> c = iter.next();
  ASSERT_TRUE(c);
  ASSERT_TRUE(c->getPosTan(c->length(), &last, nullptr));
  EXPECT_NEAR(last.y(), 0, 0.5f);  // zero-phase exit
  EXPECT_NEAR(last.x(), 320, 1.0f);
}

TEST(ComposeBrushEngine, AnExplicitIntervalIsNotOverriddenBySpacing) {
  // `spacing` is Interval-mode SUGAR: it stands in for an interval the
  // author did not set. That is why `interval` is an OPTIONAL rather than a
  // float with a default — with a default, "unset" would be a number an
  // author can type by accident, and typing exactly that number would
  // silently hand the placement over to `spacing` instead.
  auto stamps = [](std::optional<float> interval, float spacing) {
    Host host;
    brush::Scatter b;
    b.art = box().width(6).height(6).fill(red());
    b.spacing = spacing;
    b.place = {brush::Placement::Mode::Interval, interval};
    b.alignToPath = false;
    host.composer.render(box().child(box()
                                         .absolute()
                                         .inset(20, 20, 20, 20)
                                         .shape([](SkSize s) {
                                           SkPathBuilder p;
                                           p.moveTo(0, 0);
                                           p.lineTo(s.width(), 0);
                                           return p.detach();
                                         })
                                         .stroke(std::move(b))));
    host.frame();
    int runs = 0;
    bool inRun = false;
    for (int x = 0; x < 200; ++x) {
      const bool ink = host.pixel(x, 20) == SK_ColorRED;
      runs += ink && !inRun;
      inRun = ink;
    }
    return runs;
  };
  // 160 px of contour: ~7 stamps at 24 px, ~2 at 80 px.
  EXPECT_EQ(stamps(24.0f, 80.0f), stamps(24.0f, 24.0f))
      << "an explicit 24 means 24, whatever spacing says";
  EXPECT_GT(stamps(24.0f, 80.0f), stamps(std::nullopt, 80.0f))
      << "and unset still takes spacing";
}

TEST(ComposeBrushEngine, PlacementGrammarLandsOnRealVertices) {
  // Vertex family reads the path's actual verbs — stamps sit ON the bends.
  Host host;
  brush::Scatter b;
  b.art = box().width(8).height(8).fill(red());
  b.place = {brush::Placement::Mode::InnerVertices};
  b.alignToPath = false;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(40, 40, 40, 40)
                      .shape([](SkSize s) {
                        SkPathBuilder p;  // three segments, two bends
                        p.moveTo(0, s.height());
                        p.lineTo(60, s.height());
                        p.lineTo(60, 0);
                        p.lineTo(s.width(), 0);
                        return p.detach();
                      })
                      .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 160), SK_ColorRED);   // bend 1 (60,120)+40
  EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);    // bend 2 (60,0)+40
  EXPECT_EQ(host.pixel(40, 160), SK_ColorBLACK);  // endpoints excluded

  Host centers;
  brush::Scatter c;
  c.art = box().width(8).height(8).fill(blue());
  c.place = {brush::Placement::Mode::SegmentCenter};
  c.alignToPath = false;
  centers.composer.render(box().child(box()
                                          .absolute()
                                          .inset(40, 40, 40, 40)
                                          .shape([](SkSize s) {
                                            SkPathBuilder p;
                                            p.moveTo(0, 0);
                                            p.lineTo(s.width(), 0);
                                            return p.detach();
                                          })
                                          .stroke(std::move(c))));
  centers.frame();
  EXPECT_EQ(centers.pixel(100, 40), SK_ColorBLUE);  // the segment midpoint
}

TEST(ComposeBrushEngine, AlongGradientRampsOverTheArc) {
  Host host;
  lines::Line grad;
  grad.width = 8;
  grad.alongStops = {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  host.composer.render(straightRun(std::move(grad)));
  host.frame();
  const SkColor start = host.pixel(30, 100);
  const SkColor end = host.pixel(170, 100);
  EXPECT_GT(SkColorGetR(start), 200u);  // red end
  EXPECT_LT(SkColorGetB(start), 60u);
  EXPECT_GT(SkColorGetB(end), 200u);  // blue end
  EXPECT_LT(SkColorGetR(end), 60u);
}

// Value semantics and cache invalidation around Element itself.

TEST(ComposeElement, MutatingRenderedValueDetachesDescription) {
  Host host;
  Element panel = box().width(100).height(100).fill(red());

  host.composer.render(box().child(panel));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);

  // Composer retains the first description. Mutating the caller's value must
  // create a new description so pointer-identity pruning cannot preserve the
  // old cached picture.
  panel.fill(blue());
  host.composer.render(box().child(panel));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);
}

TEST(ComposeElement, CopiedValuesMutateIndependently) {
  Host host;
  Element left = box().width(100).height(100).fill(red());
  Element right = left;
  right.fill(blue());

  host.composer.render(box().row().child(left).child(right));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
  EXPECT_EQ(host.pixel(150, 50), SK_ColorBLUE);
}

TEST(ComposeLines, OffsetAlongClampsNonPositiveStep) {
  SkPathBuilder builder;
  builder.moveTo(10, 50);
  builder.lineTo(190, 50);
  const SkPath route = builder.detach();

  for (float step : {0.0f, -4.0f}) {
    const SkPath shifted = lines::offsetAcross(route, -10.0f, step);
    ASSERT_FALSE(shifted.isEmpty()) << "step=" << step;
    EXPECT_NEAR(shifted.getBounds().top(), 60.0f, 0.01f);
    EXPECT_NEAR(shifted.getBounds().bottom(), 60.0f, 0.01f);
  }
}

TEST(ComposeBrushes, PatternCopyRebakesAllChangedArt) {
  auto art = [](Fill fill) { return box().width(12).height(12).fill(fill); };
  brush::Pattern base;
  base.side = box().width(16).height(4).fill(green());
  base.start = art(red());
  base.end = art(red());
  base.corner = brush::CornerArt{art(red()), brush::CornerAlign::Bisector};
  base.advance = 16;

  // Aggregate copies intentionally share the memoization cache. The cache
  // therefore has to key every art slot, not only the side tile.
  brush::Pattern variant = base;
  variant.start = art(blue());
  variant.end = art(blue());
  variant.corner = brush::CornerArt{art(blue()), brush::CornerAlign::Bisector};

  auto lRun = [](brush::Pattern brush) {
    return box().child(box()
                           .absolute()
                           .inset(30)
                           .shape([](SkSize size) {
                             SkPathBuilder path;
                             path.moveTo(0, 0);
                             path.lineTo(size.width(), 0);
                             path.lineTo(size.width(), size.height());
                             return path.detach();
                           })
                           .stroke(std::move(brush)));
  };

  Host donor;
  donor.composer.render(lRun(base));
  donor.frame();  // primes the shared cache with red start/end/corner art
  EXPECT_EQ(donor.pixel(38, 30), SK_ColorRED);
  EXPECT_EQ(donor.pixel(170, 162), SK_ColorRED);

  Host changed;
  changed.composer.render(lRun(variant));
  changed.frame();
  EXPECT_EQ(changed.pixel(38, 30), SK_ColorBLUE);    // changed start art
  EXPECT_EQ(changed.pixel(170, 32), SK_ColorBLUE);   // changed corner art
  EXPECT_EQ(changed.pixel(170, 162), SK_ColorBLUE);  // changed end art
}

TEST(ComposeDecorations, ContourWalkCopyRebakesChangedStamp) {
  ContourWalk base;
  base.spacing = 1000.0f;  // one stamp at the open route's first point
  base.stamp = box().width(12).height(12).fill(red());
  ContourWalk variant = base;
  variant.stamp = box().width(12).height(12).fill(blue());

  Host donor;
  donor.composer.render(straightRun(base));
  donor.frame();  // primes the shared cache with the red stamp
  EXPECT_EQ(donor.pixel(20, 100), SK_ColorRED);

  Host changed;
  changed.composer.render(straightRun(variant));
  changed.frame();
  EXPECT_EQ(changed.pixel(20, 100), SK_ColorBLUE);
}

TEST(ComposeTrim, PathFormatOpenContourWrapKeepsTwoPieces) {
  // PathFormat owns a separate wrapping trim window. It must apply the same
  // open-contour rule as node-level trim instead of connecting both pieces.
  Host host;
  PathFormat format;
  format.width = 6;
  format.strokeFill = green();
  format.trimStart = 0.9f;
  format.trimEnd = 1.2f;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20, 80, 20, 80)
                                       .shape([](SkSize s) {
                                         SkPathBuilder b;
                                         b.moveTo(0, s.height() / 2);
                                         b.lineTo(s.width(), s.height() / 2);
                                         return b.detach();
                                       })
                                       .stroke(format)));
  host.frame();
  EXPECT_EQ(host.pixel(170, 100), SK_ColorGREEN);  // tail piece [0.9, 1]
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);   // head piece [0, 0.2]
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // NO invented chord
}

namespace {
/** A corner tile whose EXTENT is symmetric but whose colour is not: a 24x8
 *  bar, red on its local -x half and green on its local +x half. The stamp
 *  centres on the art's cull rect, so the midpoint of the two blobs is the
 *  placement and the vector between them is the rotation — both readable
 *  off the pixels, neither inferred. */
Element directedCornerTile() {
  return box()
      .width(24)
      .height(8)
      .child(box().absolute().left(0).top(0).width(12).height(8).fill(
          Fill::color({1, 0, 0, 1})))
      .child(box().absolute().left(12).top(0).width(12).height(8).fill(
          Fill::color({0, 1, 0, 1})));
}
struct Blob {
  double x = 0, y = 0;
  size_t n = 0;
  void add(int px, int py) {
    x += px;
    y += py;
    ++n;
  }
  SkPoint centre() const {
    return {(float)(x / (double)n), (float)(y / (double)n)};
  }
};
}  // namespace

// ---------------------------------------------------------------------------
// The documented call surface, compiled.
//
// A documented signature that does not compile is worse than no
// documentation: it is a confident wrong answer, and a reader trusts it over
// the header. Prose reads correctly with the arguments in the wrong order,
// so nothing short of compiling the call catches it.
//
// Every call spelled in the line and border documentation is spelled here
// too. This test asserts nothing at runtime; its value is entirely that it
// must build.
TEST(ComposeDocs, EverySignatureInTheLineAndBorderDocsCompiles) {
  Fill ink = Fill::color({1, 1, 1, 1});

  auto border = decorations::border(1.4f, ink, 0.0f);
  // Brackets and gapped rules are span claims on the node's boundary.
  // Border's own modes stay spellable for the shapes a claim cannot reach:
  // inset rules, doubleBorder layers, onEdges adaptors.
  auto brackets =
      box().stroke(spans::corners(18.0f, 12.0f), brush::solid(2.0f, ink));
  auto gapped =
      box().stroke(spans::edges(14.0f, 12.0f), brush::solid(1.4f, ink));
  auto bracketValue = Border{.width = 2.0f,
                             .fill = ink,
                             .inset = 4.0f,
                             .mode = Border::Mode::Bracket,
                             .corner = 18.0f,
                             .cornerAngleDeg = 12.0f};
  auto weighted =
      decorations::weightedCorners(1.0f, 3.4f, ink, 18.0f, 0.0f, 12.0f);
  auto doubled = decorations::doubleBorder(border, border);

  auto symmetric = lines::rails(3, 1.6f, ink, 5.0f);
  auto explicitSet = lines::rails({{.across = 3, .width = 1.6f, .fill = ink},
                                   {.across = 0,
                                    .width = 0.6f,
                                    .fill = ink,
                                    .dash = {0.01f, 9.4f},
                                    .cap = SkPaint::kRound_Cap}});

  auto ngon = shapes::polygon(20, -90.0f);
  auto chamfer = shapes::chamfered(22.0f, shapes::Corner::All);
  auto notch = shapes::notched(26.0f, 9.0f, shapes::Corner::Diagonal);
  auto edges = shapes::onEdges(shapes::Edge::Top, util::stroke(2.0f, ink));

  auto glow =
      kit::brush::presets::filament({0.4f, 0.8f, 1, 1}, {0.9f, 1, 1, 1}, 1.0f);
  auto trace = kit::brush::presets::circuit({0.2f, 0.9f, 0.8f, 1}, 1);
  auto cord = kit::brush::presets::rope(1, 1.0f);
  // OP FIRST, decoration second. The two arguments read equally well in
  // either order in prose, which is why the call is spelled here.
  auto restyled = brush::restyle(kit::brush::shapers::Jitter{8.0f, 2.0f, 7},
                                 util::stroke(1.0f, ink), 8.0f);

  lines::Hatch hatch;
  choreograph::Output<float> pitch{6.0f}, angle{0.0f};
  hatch.spacingBinding = &pitch;
  hatch.angleBinding = &angle;
  EXPECT_TRUE(hatch.isAnimated());  // a binding IS the volatility declaration

  kit::brush::shapers::Wave wave{.amplitude = 3.5f, .wavelength = 22};
  kit::brush::shapers::Rounded rounded{};
  kit::brush::shapers::Jitter jitter{};
  kit::brush::shapers::Square square{};
  kit::brush::shapers::Offset offset{};

  (void)brackets;
  (void)gapped;
  (void)bracketValue;
  (void)weighted;
  (void)doubled;
  (void)symmetric;
  (void)explicitSet;
  (void)ngon;
  (void)chamfer;
  (void)notch;
  (void)edges;
  (void)glow;
  (void)trace;
  (void)cord;
  (void)restyled;
  (void)wave;
  (void)rounded;
  (void)jitter;
  (void)square;
  (void)offset;
}

TEST(ComposeDocs, EverySignatureInTheCachingDocsCompiles) {
  // The same mechanism as its sibling above, pointed at the caching and
  // automatic-promotion surface. An author reaches for that documentation
  // precisely when a scene is too slow, so a wrong call there costs more
  // than a wrong call anywhere else — it turns "I do not know why this is
  // slow" into a confident wrong answer.
  //
  // Asserts almost nothing at runtime by design. Its value is that it must
  // BUILD, so a rename or a signature change cannot land without someone
  // reading the prose beside it.
  sigil::motion::Ticker ticker;
  Composer composer(ticker, fonts());
  composer.setSize({100, 100});

  composer.setAutoTexturePromotion(false);
  EXPECT_FALSE(composer.autoTexturePromotion());
  composer.setAutoTexturePromotion(true);
  composer.setProfiling(true);
  EXPECT_TRUE(composer.profiling());
  composer.purgeCaches();  // the host's one hook on device loss

  // The per-node overrides the section names, all five spellings.
  (void)box().cache(Cache::Auto);
  (void)box().cache(Cache::None);
  (void)box().cache(Cache::Picture);
  (void)box().cache(Cache::Texture);
  (void)box().cache(Cache::Group);
  (void)box().bakeScale(0.5f);

  // Every Promotion value the doc lists, so a renamed or removed
  // enumerator fails here rather than in a study's --bench output.
  for (Composer::Promotion p :
       {Composer::Promotion::Cheap, Composer::Promotion::Warming,
        Composer::Promotion::Promoted, Composer::Promotion::AskedFor,
        Composer::Promotion::OptedOut, Composer::Promotion::Volatile,
        Composer::Promotion::Composited, Composer::Promotion::Transformed,
        Composer::Promotion::Filtered, Composer::Promotion::ReadsBackdrop,
        Composer::Promotion::TooBig, Composer::Promotion::SplitBaked})
    EXPECT_STRNE(Composer::promotionReason(p), "")
        << "a Promotion value with no phrase reaches an author as blank "
           "space under the node that is costing them the frame";

  // …and every CacheState, for the same reason: `--bench` switches over
  // this exhaustively, so a value added without a case is a build break
  // for whoever adds it and a silent mislabel if the switch grows a
  // `default:`.
  for (Composer::CacheState s :
       {Composer::CacheState::Live, Composer::CacheState::Picture,
        Composer::CacheState::Texture, Composer::CacheState::Promoted,
        Composer::CacheState::SplitOwn, Composer::CacheState::Group}) {
    Composer::NodeCost row;
    row.cacheState = s;
    EXPECT_EQ(row.cached(), s != Composer::CacheState::Live);
  }

  // The refusal mask, exactly as the doc spells it.
  Composer::NodeCost row;
  EXPECT_EQ(row.refusals, 0u);
  EXPECT_FALSE(row.refused(Composer::Promotion::Volatile));
  for (const auto& r : composer.profile()) {
    if (r.cacheState != Composer::CacheState::Live) continue;
    (void)Composer::promotionReason(r.promotion);
    for (auto also :
         {Composer::Promotion::Volatile, Composer::Promotion::Filtered,
          Composer::Promotion::ReadsBackdrop})
      if (also != r.promotion && r.refused(also))
        (void)Composer::promotionReason(also);
    (void)r.selfMs;
    (void)r.totalMs;
    (void)r.depth;
    (void)r.label;
  }
  const Composer::Stats& stats = composer.stats();
  (void)stats.picturesRecorded;
  (void)stats.texturesBaked;
  (void)stats.nodesPainted;
}

TEST(ComposeDocs, EverySignatureInTheDecorationAndLayoutDocsCompiles) {
  // The same device widened to the decoration and layout surface, because
  // covering one documented section leaves every other one exactly as
  // unchecked as before. PathFormat is the worst place for that: its member
  // names are close enough to guess wrongly — `effect` is singular and the
  // colour member is `strokeFill`, not `paint` — and it is the primitive an
  // author meets first.
  //
  // Every declaration below corresponds to a documented spelling. If one
  // stops compiling, the prose beside it has to be read.
  Fill ink = Fill::color({0.9f, 0.9f, 1, 1});

  // ---- PathFormat, all seven documented spellings ------------------------
  PathFormat plain{.width = 3.0f, .strokeFill = ink};
  PathFormat dashed{
      .width = 1.0f, .strokeFill = ink, .dashIntervals = {4.0f, 3.0f}};
  PathFormat capped{.width = 2.0f,
                    .strokeFill = ink,
                    .cap = SkPaint::kRound_Cap,
                    .join = SkPaint::kRound_Join};
  PathFormat inner = util::stroke(2.0f, ink, PathFormat::Align::Inner);
  PathFormat outer = util::stroke(2.0f, ink, PathFormat::Align::Outer);
  PathFormat material{.width = 2.0f};
  material.strokeMaterial = Material::solid({0.7f, 0.6f, 0.3f, 1});
  PathFormat stamped{.width = 1.0f, .strokeFill = ink};
  stamped.stampPath = SkPath::Circle(0, 0, 3);
  stamped.stampAdvance = 12.0f;
  PathFormat effected{.width = 1.0f, .strokeFill = ink};
  effected.effect = SkDiscretePathEffect::Make(6.0f, 2.0f);
  choreograph::Output<float> phase{0.0f};
  PathFormat marching{
      .width = 1.0f, .strokeFill = ink, .dashIntervals = {6.0f, 4.0f}};
  marching.dashPhaseBinding = &phase;
  EXPECT_TRUE(Decoration(marching).isAnimated())
      << "a bound dash phase IS the volatility declaration";

  // ---- ContourWalk and PathSample --------------------------------------
  // `position`, not `pos`; `fraction` is per CONTOUR, `distance` is not.
  ContourWalk walk{.spacing = 18.0f};
  walk.draw = [](SkCanvas& c, const PathSample& s, const PaintContext&) {
    SkPaint p;
    c.drawCircle(s.position.fX + s.tangent.fX * 0.0f + s.distance * 0.0f +
                     s.fraction * 0.0f,
                 s.position.fY, 1.0f, p);
  };
  ContourWalk stampWalk{.spacing = 24.0f};
  stampWalk.stamp = box().width(4).height(4).fill(ink);

  // ---- the documented decoration constructors ---------------------------
  auto wash = decorations::wash(Material::solid({1, 1, 1, 0.2f}),
                                SkBlendMode::kOverlay, 0.5f);
  // shadow(color, OFFSET, blur) — the offset is the second argument.
  auto shadow = util::shadow({0, 0, 0, 0.5f}, {0, 2}, 8.0f);

  // ---- shapes, exactly as the "shapes" block spells them -----------------
  auto circle = shapes::circle();
  auto annulus = shapes::annulus(0.6f);
  auto sector = shapes::sector(0.0f, 90.0f, 0.4f);
  auto arrow = shapes::arrow(0.6f, 0.3f);
  auto poly = shapes::polygon(6, -90.0f);
  auto star = shapes::star(5, 0.45f, 0.2f);
  auto chamfer = shapes::chamfered(10.0f, shapes::Corner::All);
  auto notched = shapes::notched(10.0f, 4.0f, shapes::Corner::Diagonal);
  auto onEdges = shapes::onEdges(shapes::Edge::Top, plain);
  auto inset = shapes::inset(6.0f, Decoration(plain));

  // ---- EVERY layout scheme in Layouts.h, not only the documented ones ----
  layouts::Radial radial;
  layouts::AlongPath alongPath;
  layouts::ModularGrid modular;
  layouts::Diagonal diagonal;
  layouts::BaselineGrid baseline;  // the only consumer of childBaselines
  layouts::Scatter scatter;

  // ---- text on a path: all THREE orientations ---------------------------
  for (TextPath::Orient o :
       {TextPath::Orient::Tangent, TextPath::Orient::Radial,
        TextPath::Orient::Upright}) {
    TextPath spec;
    spec.path = [](SkSize s) { return shapes::circle()(s); };
    spec.at = 0.25f;
    spec.orient = o;
    (void)box().child(text(u8"ring", styleAt(12)).onPath(spec));
  }

  // ---- patterns: grain's FIVE parameters --------------------------------
  (void)patterns::grain(0.02f, 4, 7.0f);              // the documented three
  (void)patterns::grain(0.02f, 4, 7.0f, 1.6f, 3.0f);  // contrast + stretch
  // The last parameter is a BOOL selecting fractal or turbulence mode, not
  // an amount. Passing a float compiles — it converts silently to `true` —
  // so a documented call that reads like `noise(…, 0.5f)` is wrong in a way
  // only a human reader can catch. Spelled correctly here, once.
  (void)patterns::noise(0.02f, 4, 1.0f, true);

  (void)plain;
  (void)dashed;
  (void)capped;
  (void)inner;
  (void)outer;
  (void)material;
  (void)stamped;
  (void)effected;
  (void)walk;
  (void)stampWalk;
  (void)wash;
  (void)shadow;
  (void)circle;
  (void)annulus;
  (void)sector;
  (void)arrow;
  (void)poly;
  (void)star;
  (void)chamfer;
  (void)notched;
  (void)onEdges;
  (void)inset;
  (void)radial;
  (void)alongPath;
  (void)modular;
  (void)diagonal;
  (void)baseline;
  (void)scatter;
}

TEST(ComposeDocs, EverySignatureInTheMaterialDocsCompiles) {
  // The Material surface and its cost model. Materials are the most
  // expensive objects a scene carries and the most easily mis-specified, and
  // two of their names are exactly the ones a reader guesses wrongly: `Stop`
  // is a free struct in the namespace rather than a Material member, and the
  // flat-colour factory is `solid`, not `color`.
  auto stops = std::vector<Stop>{
      {0.0f, {1, 0, 0, 1}}, {0.5f, {0, 1, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  Material flat = Material::solid({0.2f, 0.3f, 0.4f, 1});
  Material lin = Material::linear({0, 0}, {100, 0}, stops);
  Material rad = Material::radial({50, 50}, 40.0f, stops);
  Material sweep = Material::sweep({50, 50}, stops);
  // The Unit forms take node-relative coordinates, so they are the only
  // ones an author can write for a box whose size is decided by its
  // content — absolute coordinates would have to be guessed.
  Material linU = Material::linearUnit({0, 0}, {1, 0}, stops);
  Material radU = Material::radialUnit({0.5f, 0.5f}, 0.7f, stops);
  Material glowU = Material::glowUnit(
      {0.5f, 0.5f}, 1.0f, {{0.0f, {1, 1, 1, 1}}, {1.0f, {1, 1, 1, 0}}});
  Material sksl = Material::sksl(sharedHeavyEffect());
  Material blended = Material::blend(
      {{flat, SkBlendMode::kSrc}, {lin, SkBlendMode::kOverlay}});

  // The liveness contract the cost model rests on: a material is "live"
  // only when something actually drives it, and `quantizeTime` is what
  // makes a live one cacheable between its ticks.
  EXPECT_FALSE(flat.isAnimated());
  EXPECT_FALSE(lin.isAnimated());
  Material timed = Material::sksl(heavyEffect(true));
  EXPECT_TRUE(timed.isAnimated())
      << "liveness is read off the DECLARATION of "
         "uTime, not off whether anything drives it";
  timed.quantizeTime(10.0f);
  EXPECT_TRUE(timed.isAnimated());

  // A material is a value: it converts to a Fill, and it compares by RECIPE.
  // That comparison is load-bearing for every cache in the library — a
  // material rebuilt from equal values must compare equal, or its node is
  // dirtied on every describe and no bake of any kind can hold.
  (void)flat.toFill();
  EXPECT_TRUE(flat == Material::solid({0.2f, 0.3f, 0.4f, 1}));
  EXPECT_FALSE(flat == lin);
  EXPECT_TRUE(lin == Material::linear({0, 0}, {100, 0}, stops));

  // Every place a Material is accepted, spelled once each.
  (void)box().fill(flat);
  (void)box().textFill(linU);
  (void)util::stroke(2.0f, Fill::color({1, 1, 1, 1}));
  PathFormat stroked = util::stroke(2.0f, Fill::color({1, 1, 1, 1}));
  stroked.strokeMaterial = radU;  // a stroke takes a Material, not only a Fill
  (void)decorations::wash(glowU, SkBlendMode::kOverlay, 0.5f);

  (void)sweep;
  (void)sksl;
  (void)blended;
  (void)stroked;
}

TEST(ComposeBrushes, PatternCornerLandsOnTheVertexAndFacesTheBisector) {
  // Corner placement has two independent ways to go subtly wrong, and a
  // large corner tile against short sides is what makes both visible.
  //
  //  a. The tangent scan STRADDLES the vertex — it compares the tangent at
  //     d − step with the tangent at d — so a bend is first detected one
  //     step AFTER it happens. Taking the midpoint of that bracket as the
  //     vertex puts the art up to half a step along the outgoing leg. The
  //     corner has to be recovered from the two legs, not from the scan
  //     position.
  //  b. A bisector built by re-probing tangents at d ± ε is wrong for the
  //     same reason: from a point already past the vertex, both probes land
  //     on the same leg and every corner ends up facing the outgoing
  //     tangent. The one exception is a closed contour's seam at d = 0,
  //     whose probes wrap onto both legs — so on a rectangle three corners
  //     agree with each other and the fourth sits 45 degrees off, which
  //     reads as "the seam is special" rather than as a general error.
  //
  // A rectangle's vertices are exact, so both claims here are arithmetic
  // rather than approximate.
  Host host(400, 340);
  brush::Pattern brush;
  brush.side = box().width(24).height(10).child(
      box().absolute().left(2).top(4).width(20).height(2).fill(
          Fill::color({0.35f, 0.45f, 0.95f, 1})));
  brush.corner =
      brush::CornerArt{directedCornerTile(), brush::CornerAlign::Bisector};
  brush.advance = 24;
  brush.cornerLength = 40;
  brush.reach = 40;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(0)
                                       .shape([](SkSize) {
                                         SkPathBuilder p;
                                         p.moveTo(100, 100);
                                         p.lineTo(300, 100);
                                         p.lineTo(300, 240);
                                         p.lineTo(100, 240);
                                         p.close();
                                         return p.detach();
                                       })
                                       .stroke(std::move(brush))));
  host.frame();

  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(400, 340));
  ASSERT_TRUE(host.surface->readPixels(bm.pixmap(), 0, 0));
  // Quadrant assignment about the rect's centre — unbiased, and every tile
  // sits within 30 px of its own vertex, nowhere near the middle.
  Blob red[4], green[4];
  for (int y = 0; y < 340; ++y)
    for (int x = 0; x < 400; ++x) {
      const SkColor c = bm.getColor(x, y);
      const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                b = (int)SkColorGetB(c);
      const int q = (x > 200 ? 1 : 0) + (y > 170 ? 2 : 0);
      if (r > 150 && r > 2 * g && r > 2 * b)
        red[q].add(x, y);
      else if (g > 150 && g > 2 * r && g > 2 * b)
        green[q].add(x, y);
    }

  // quadrant order: 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = BR
  const SkPoint vertex[4] = {{100, 100}, {300, 100}, {100, 240}, {300, 240}};
  for (int q = 0; q < 4; ++q) {
    ASSERT_GT(red[q].n, 20u) << "no corner tile in quadrant " << q;
    ASSERT_GT(green[q].n, 20u) << "no corner tile in quadrant " << q;
    const SkPoint rc = red[q].centre(), gc = green[q].centre();
    const SkPoint place{(rc.x() + gc.x()) * 0.5f, (rc.y() + gc.y()) * 0.5f};
    EXPECT_NEAR(place.x(), vertex[q].x(), 1.5f)
        << "corner " << q << " landed off its vertex in x";
    EXPECT_NEAR(place.y(), vertex[q].y(), 1.5f)
        << "corner " << q << " landed off its vertex in y";
    // Every vertex of an axis-aligned rectangle bisects to a diagonal, so
    // |dx| == |dy|. The outgoing tangent is axis aligned and one of them
    // would be ~0.
    const float dx = std::abs(gc.x() - rc.x()), dy = std::abs(gc.y() - rc.y());
    EXPECT_NEAR(dx, dy, (dx + dy) * 0.2f)
        << "corner " << q << " faces (" << (gc.x() - rc.x()) << ", "
        << (gc.y() - rc.y()) << ") — not the bisector";
  }
}

TEST(ComposeBrushes, PatternCornerAlignOutgoingIsStillAvailable) {
  // The other alignment is what a directional marker wants — an arrow that
  // turns a corner should keep pointing the way it is going.
  Host host(400, 340);
  brush::Pattern brush;
  brush.side =
      box().width(24).height(2).fill(Fill::color({0.2f, 0.2f, 0.6f, 1}));
  brush.corner =
      brush::CornerArt{directedCornerTile(), brush::CornerAlign::Outgoing};
  brush.advance = 24;
  brush.cornerLength = 40;
  brush.reach = 40;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(0)
                                       .shape([](SkSize) {
                                         SkPathBuilder p;
                                         p.moveTo(100, 100);
                                         p.lineTo(300, 100);
                                         p.lineTo(300, 240);
                                         p.lineTo(100, 240);
                                         p.close();
                                         return p.detach();
                                       })
                                       .stroke(std::move(brush))));
  host.frame();
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(400, 340));
  ASSERT_TRUE(host.surface->readPixels(bm.pixmap(), 0, 0));
  Blob red, green;
  for (int y = 0; y < 170; ++y)
    for (int x = 201; x < 400; ++x) {  // the top-right vertex only
      const SkColor c = bm.getColor(x, y);
      const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                b = (int)SkColorGetB(c);
      if (r > 150 && r > 2 * g && r > 2 * b)
        red.add(x, y);
      else if (g > 150 && g > 2 * r && g > 2 * b)
        green.add(x, y);
    }
  ASSERT_GT(red.n, 20u);
  ASSERT_GT(green.n, 20u);
  // Leaving (300,100) the contour heads straight DOWN: dx ~ 0, dy > 0.
  const SkPoint rc = red.centre(), gc = green.centre();
  EXPECT_NEAR(gc.x() - rc.x(), 0.0f, 2.0f);
  EXPECT_GT(gc.y() - rc.y(), 6.0f);
}

TEST(ComposeMaterials, StableLiveResolveReplaysThePicture) {
  // The resolve memo: a live material whose bound inputs did not change
  // returns the SAME shader pointer, and a node whose only volatility is
  // that material replays its picture instead of re-recording. So a slow or
  // stepped material repaints at ITS rate, not at the frame rate.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uPhase; half4 main(float2 p) {"
      "  return half4(fract(uPhase), 0.2, 1.0 - fract(uPhase), 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  choreograph::Output<float> phase{0.25f};
  host.composer.render(box().child(box().width(100).height(100).fill(
      Material::sksl(fx).uniform("uPhase", &phase))));
  host.frame();  // records once
  const SkColor before = host.pixel(50, 50);
  host.frame();  // same phase → stable resolve → pure replay
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().nodesPainted, 1u);  // just the root shim
  EXPECT_EQ(host.pixel(50, 50), before);
  phase = 0.75f;  // the material actually changed
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u);
  EXPECT_NE(host.pixel(50, 50), before);
}

TEST(ComposeMaterials, BoundUniformOwnsItsSlotOverInjection) {
  // Binding uTime to an Output is the documented stepping idiom — the
  // auto-inject must not overwrite it with continuous clock time.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uTime; half4 main(float2 p) {"
               "  return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  choreograph::Output<float> stepped{0.5f};
  Material m = Material::sksl(fx).uniform("uTime", &stepped);
  PaintContext ctx;
  ctx.size = {4, 4};
  ctx.elapsedSeconds = 123.789;  // continuous clock — must be IGNORED
  Fill f = m.resolve(ctx);
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  SkPaint p;
  p.setShader(f.shaderValue);
  s->getCanvas()->drawPaint(p);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  s->readPixels(bm.pixmap(), 0, 0);
  EXPECT_NEAR(SkColorGetR(bm.getColor(0, 0)), 128, 3);  // fract(0.5), not .789
}

TEST(ComposeMaterials, BakeScaleUpscalesThroughTheSameRect) {
  // bakeScale(0.5) rasterizes the texture bake at half resolution; the
  // blit stretches it back through the same dst rect — same coverage,
  // same color, a quarter of the evaluated pixels.
  Host host;
  host.composer.render(box().child(box()
                                       .left(20)
                                       .top(20)
                                       .width(100)
                                       .height(100)
                                       .cache(Cache::Texture)
                                       .bakeScale(0.5f)
                                       .fill(red())));
  host.frame();  // bake at half scale
  host.frame();  // blit
  EXPECT_EQ(host.pixel(70, 70), SkColorSetARGB(255, 255, 0, 0));
  // inboard of the AA edge on every side — coverage must not shrink
  EXPECT_EQ(host.pixel(23, 23), SkColorSetARGB(255, 255, 0, 0));
  EXPECT_EQ(host.pixel(116, 116), SkColorSetARGB(255, 255, 0, 0));
  EXPECT_EQ(host.pixel(140, 70), SK_ColorBLACK);  // outside stays empty
}

TEST(ComposeMaterials, StableLiveResolveBlitsTheTexture) {
  // The texture flavour of the resolve memo, which matters most for a large
  // shader-filled area: bake at the material's own rate and BLIT in between.
  // Replaying a picture re-executes the SkSL on raster; blitting a texture
  // does not.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uPhase; half4 main(float2 p) {"
               "  return half4(fract(uPhase), 0.4, 0.2, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  choreograph::Output<float> phase{0.25f}, sibling{0.0f};
  host.composer.render(
      box()
          .child(box()
                     .width(100)
                     .height(100)
                     .cache(Cache::Texture)
                     .fill(Material::sksl(fx).uniform("uPhase", &phase)))
          // An always-animating sibling keeps the ROOT live, which is the
          // ordinary case in a real scene: the shader-filled node must still
          // blit even though the frame as a whole is repainting.
          .child(box().width(10).height(10).fill(red()).translateX(&sibling)));
  host.frame();  // bakes
  const unsigned recordedAfterBake = host.composer.stats().picturesRecorded;
  EXPECT_GE(recordedAfterBake, 1u);
  host.frame();  // stable phase → blit, no re-bake
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  phase = 0.75f;
  host.frame();  // real change → one re-bake
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
}

// ---------------------------------------------------------------------------
// instances() — the flyweight repeat layer (<sigilcompose/Instances.h>)

#include <sigilcompose/Instances.h>

TEST(ComposeInstances, StampsAtlasCellsAtPoolPositionsWithTint) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {20, 20});
  auto pool = std::make_shared<Pool>();
  pool->add({50, 50});                                // white cell, untinted
  pool->add({150, 50}, 0, 0.0f, 1.0f, {1, 0, 0, 1});  // tinted red
  // The bake itself: sheet exists and the cell's center is opaque white.
  ASSERT_TRUE(atlas->ensureBaked(fonts()));
  ASSERT_TRUE(atlas->image());
  {
    SkBitmap probe;
    probe.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    ASSERT_TRUE(atlas->image()->readPixels(nullptr, probe.pixmap(), 20, 20));
    EXPECT_EQ(probe.getColor(0, 0), SK_ColorWHITE);
  }
  host.composer.render(box().child(instances(atlas, pool)));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(150, 50), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // between stamps: nothing
}

TEST(ComposeInstances, DataModePrunesUntilTouched) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {16, 16});
  auto pool = std::make_shared<Pool>();
  pool->add({40, 40});
  auto describe = [&] { return box().child(instances(atlas, pool)); };
  host.composer.render(describe());
  host.frame();
  // Unchanged pool: the re-describe prunes (memo hit), the cached picture
  // replays, nothing re-records.
  host.composer.render(describe());
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  // Mutate + touch + render: repaints exactly once, pixels move.
  pool->positions()[0] = {120, 40};
  pool->commit();
  host.composer.render(describe());
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(120, 40), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);
}

TEST(ComposeInstances, LiveModeReadsThePoolEveryFrame) {
  using namespace sigil::compose::instancing;
  Host host;
  auto atlas = std::make_shared<Atlas>();
  atlas->cell(box().fill(Fill::color({1, 1, 1, 1})), {16, 16});
  auto pool = std::make_shared<Pool>();
  pool->add({40, 40});
  host.composer.render(box().child(instances(atlas, pool, Mode::Live)));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE);
  // No touch(), no render() — the Cache::None leaf reads the latest data.
  pool->positions()[0] = {140, 140};
  host.frame();
  EXPECT_EQ(host.pixel(140, 140), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);
}

TEST(ComposeInstances, RepeaterLawExponentialScaleLinearEverythingElse) {
  using namespace sigil::compose::instancing;
  Pool pool;
  place::repeat(pool, 4, {10, 10}, {5, 0}, 0.1f, 0.5f, 1.0f, 0.25f);
  ASSERT_EQ(pool.size(), 4u);
  EXPECT_FLOAT_EQ(pool.positions()[3].fX, 25.0f);  // linear translate
  EXPECT_FLOAT_EQ(pool.rotations()[3], 0.3f);      // linear rotate
  EXPECT_FLOAT_EQ(pool.scales()[3], 0.125f);       // pow(0.5, 3)
  // Opacity and tint are separate lanes: the opacity ramp writes alphas[],
  // and tints[].fA stays exactly what the author put there. Folding one into
  // the other would make a tinted pool silently un-tintable.
  EXPECT_FLOAT_EQ(pool.alphas()[0], 1.0f);  // opacity lerp endpoints
  EXPECT_FLOAT_EQ(pool.alphas()[3], 0.25f);
  EXPECT_FLOAT_EQ(pool.tints()[3].fA, 1.0f);  // untouched
}

// ---------------------------------------------------------------------------
// The edge store: node→routes back-index + flat derive lists

TEST(ComposeEdgeStore, RoutesAtReturnsAnchoredRoutesInTreeOrder) {
  Host host;
  auto describe = [] {
    return box()
        .child(box().key("a").width(30).height(30).absolute().inset(10, 10, 160,
                                                                    160))
        .child(box().key("b").width(30).height(30).absolute().inset(160, 160,
                                                                    10, 10))
        .child(connector("a", "b").key("edge1"))
        .child(rail({{"a", {0.5f, 0.5f}}, {"b", {0.5f, 0.5f}}}).key("edge2"))
        .child(connector("a", "b"));  // keyless: anchored but unaddressable
  };
  host.composer.render(describe());
  host.frame();
  const std::vector<std::string> atA = host.composer.routesAt("a");
  ASSERT_EQ(atA.size(), 2u);  // the keyless route is omitted
  EXPECT_EQ(atA[0], "edge1");
  EXPECT_EQ(atA[1], "edge2");
  EXPECT_EQ(host.composer.routesAt("b").size(), 2u);
  EXPECT_TRUE(host.composer.routesAt("nowhere").empty());
}

TEST(ComposeEdgeStore, IndexClearsWhenRoutesUnmount) {
  Host host;
  bool withRoute = true;
  auto describe = [&] {
    auto tree = box()
                    .child(box().key("a").width(30).height(30).absolute().inset(
                        10, 10, 160, 160))
                    .child(box().key("b").width(30).height(30).absolute().inset(
                        160, 160, 10, 10));
    if (withRoute) tree.child(connector("a", "b").key("edge"));
    return tree;
  };
  host.composer.render(describe());
  host.frame();
  ASSERT_EQ(host.composer.routesAt("a").size(), 1u);
  withRoute = false;
  host.composer.render(describe());
  host.frame();
  EXPECT_TRUE(host.composer.routesAt("a").empty());
}

// ---------------------------------------------------------------------------
// The brush-arc tail: art warp (SkVertices), hatch (Sk2D), gloss (table)

#include <sigilcompose/Brushes.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Lines.h>

TEST(ComposeBrushTail, BrushArtWarpsArtAlongTheOutline) {
  Host host;
  // A straight horizontal outline through the node's middle: the warped
  // ribbon must be a horizontal band of the art's height around it.
  auto lineOutline = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() / 2);
    b.lineTo(s.width(), s.height() / 2);
    return b.detach();
  };
  brush::Art brush = brush::artAlong(
      box().width(40).height(20).fill(Fill::color({1, 1, 1, 1})), 20);
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20, 60, 20, 60)
                                       .shape(lineOutline)
                                       .foreground(brush)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorWHITE);  // on the ribbon
  EXPECT_EQ(host.pixel(100, 130), SK_ColorBLACK);  // 30px off: outside height
}

TEST(ComposeBrushTail, HatchFillsInteriorSparsely) {
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(50, 50, 50, 50)
          .background(lines::hatch(Fill::color({1, 1, 1, 1}), 8, 1.5f, 45))));
  host.frame();
  // Count lit pixels in the hatched interior: strictly between "empty"
  // and "solid fill" — the lattice is present but sparse.
  int lit = 0;
  const int total = 100 * 100;
  for (int y = 50; y < 150; y += 2)
    for (int x = 50; x < 150; x += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) ++lit;
  const float coverage = (float)lit / (float)(total / 4);
  EXPECT_GT(coverage, 0.05f);
  EXPECT_LT(coverage, 0.75f);
  // Nothing escapes the clip.
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLACK);
}

TEST(ComposeBrushTail, GlossContourBandsInsideTheShape) {
  Host plain, glossed;
  auto shape = [] {
    return box()
        .absolute()
        .inset(50, 50, 50, 50)
        .corners({24})
        .fill(Fill::color({0.2f, 0.3f, 0.5f, 1}));
  };
  plain.composer.render(box().child(shape()));
  plain.frame();
  glossed.composer.render(
      box().child(shape().foreground(styles::gloss({1, 1, 1, 1}, 8, {0, -4}))));
  glossed.frame();
  // The band brightens SOME interior pixels but not the deep center
  // (table peaks at mid-coverage, so the middle of the shape stays fill).
  int changed = 0;
  for (int y = 52; y < 148; y += 2)
    for (int x = 52; x < 148; x += 2)
      if (plain.pixel(x, y) != glossed.pixel(x, y)) ++changed;
  EXPECT_GT(changed, 40);  // a real band appeared
  EXPECT_EQ(plain.pixel(100, 100), glossed.pixel(100, 100));  // center: fill
  EXPECT_EQ(plain.pixel(30, 30), glossed.pixel(30, 30));      // outside: clip
}

// ---------------------------------------------------------------------------
// VariationDrive — draw-time variable-font axes, gated by the advance probe

TEST(ComposeVariationDrive, GradDrivesPaintOnlyWhenAdvanceInvariant) {
  // The San Francisco system face carries the advance-invariant GRAD axis
  // on modern macOS; find a face that passes the probe or skip honestly.
  sk_sp<SkFontMgr> manager = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> ui;
  for (const char* family :
       {".AppleSystemUIFont", ".SF NS", "SF Pro Text", "SF Pro"}) {
    ui = manager->matchFamilyStyle(family, SkFontStyle());
    if (ui && fonts().axisIsAdvanceInvariant(ui, "GRAD")) break;
    ui = nullptr;
  }
  if (!ui) GTEST_SKIP() << "no advance-invariant GRAD face on this system";
  // The probe proves advances HOLD; it cannot prove the clone RESPONDS
  // (a hidden system face can accept the axis and render identically).
  // Check a glyph's outline actually moves across the range, else skip.
  {
    const int n = ui->getVariationDesignParameters({});
    std::vector<SkFontParameters::Variation::Axis> axes((size_t)n);
    ui->getVariationDesignParameters({axes.data(), axes.size()});
    float lo = 0, hi = 0;
    for (const auto& a : axes)
      if (a.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
        lo = a.min;
        hi = a.max;
      }
    const sigil::weave::FontVariation vLo("GRAD", lo), vHi("GRAD", hi);
    SkFont fLo(fonts().variedTypeface(ui, {&vLo, 1}), 48);
    SkFont fHi(fonts().variedTypeface(ui, {&vHi, 1}), 48);
    SkGlyphID glyph = fLo.unicharToGlyph('W');
    auto rasterize = [&](const SkFont& f) {
      sk_sp<SkSurface> s =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
      s->getCanvas()->clear(SK_ColorBLACK);
      SkPaint paint;
      paint.setColor(SK_ColorWHITE);
      paint.setAntiAlias(true);
      const SkPoint at{10, 60};
      s->getCanvas()->drawGlyphs(SkSpan(&glyph, 1), SkSpan(&at, 1), {0, 0}, f,
                                 paint);
      SkBitmap bm;
      bm.allocPixels(s->imageInfo());
      s->readPixels(bm.pixmap(), 0, 0);
      return bm;
    };
    SkBitmap rLo = rasterize(fLo), rHi = rasterize(fHi);
    int rasterDelta = 0;
    for (int y = 0; y < 80; ++y)
      for (int x = 0; x < 100; ++x)
        if (rLo.getColor(x, y) != rHi.getColor(x, y)) ++rasterDelta;
    if (rasterDelta == 0)
      GTEST_SKIP() << "GRAD clone is rendering-inert on this system face";
  }
  // Drive the axis's REAL design range (SF's GRAD span is font-defined;
  // hardcoded values can land clamped onto the default = no visual delta).
  float gradeMin = 0, gradeMax = 0;
  {
    const int n = ui->getVariationDesignParameters({});
    std::vector<SkFontParameters::Variation::Axis> axes((size_t)n);
    ui->getVariationDesignParameters({axes.data(), axes.size()});
    for (const auto& a : axes)
      if (a.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
        gradeMin = a.min;
        gradeMax = a.max;
      }
  }

  choreograph::Output<float> grade{gradeMin};
  Host host;
  auto describe = [&] {
    sigil::weave::TextStyle style = styleAt(48);
    style.shaping.typeface = ui;
    style.paint.foreground.setColor(SK_ColorWHITE);  // black-on-black otherwise
    return box().child(text(u8"WEIGHT", style)
                           .key("t")
                           .variationDrive("GRAD", &grade)
                           .absolute()
                           .inset(20, 60, 20, 60));
  };
  host.composer.render(describe());
  host.frame();
  const SkRect before = *host.composer.bounds("t");
  SkBitmap lo;
  lo.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(lo.pixmap(), 0, 0);

  grade = gradeMax;  // heavy grade — glyphs thicken, advances hold
  host.frame();
  const SkRect after = *host.composer.bounds("t");
  SkBitmap hi;
  hi.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(hi.pixmap(), 0, 0);

  EXPECT_EQ(before, after);  // no relayout — paint-only volatility
  int changed = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      if (lo.getColor(x, y) != hi.getColor(x, y)) ++changed;
  EXPECT_GT(changed, 20) << "GRAD range " << gradeMin << ".."
                         << gradeMax;  // visible thickening
}

namespace {
/** Does @p face DECLARE @p tag at all? `axisIsAdvanceInvariant` answers
 *  FALSE both for "the axis moves advances" and for "there is no such
 *  axis", and the test below has to tell those apart: on a face without a
 *  wght axis the refusal fires for the wrong reason, and the pixels hold no
 *  matter what the drive does — so the test would pass while checking
 *  nothing. */
bool faceDeclaresAxis(const sk_sp<SkTypeface>& face, SkFourByteTag tag) {
  if (!face) return false;
  const int count = face->getVariationDesignParameters({});
  if (count <= 0) return false;
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
  face->getVariationDesignParameters({axes.data(), axes.size()});
  for (const auto& axis : axes)
    if (axis.tag == tag) return true;
  return false;
}
}  // namespace

TEST(ComposeVariationDrive, AdvanceVariantAxisIsRefused) {
  // This needs a face whose wght axis genuinely CHANGES advances, so that
  // the drive has something to refuse. A system face may not offer one — on
  // macOS the UI face declares no wght axis at all — so the fallback is a
  // committed instrument, test/assets/AdvanceVariant.ttf: a generated
  // two-master variable font whose wght interpolates advances (built by
  // make_advance_variant_vf.py). Both preconditions are asserted below, so
  // the test cannot pass by running against a face that has no axis.
  const SkFourByteTag wght = SkSetFourByteTag('w', 'g', 'h', 't');
  sk_sp<SkTypeface> ui = fonts().defaultTypeface();
  if (!faceDeclaresAxis(ui, wght) || fonts().axisIsAdvanceInvariant(ui, "wght"))
    ui = fonts().fontManager()->makeFromFile(SIGILCOMPOSE_TEST_ASSET_DIR
                                             "/AdvanceVariant.ttf");
  ASSERT_TRUE(ui) << "test asset AdvanceVariant.ttf failed to load";
  ASSERT_TRUE(faceDeclaresAxis(ui, wght));
  ASSERT_FALSE(fonts().axisIsAdvanceInvariant(ui, "wght"))
      << "the instrument face's wght must move advances";

  choreograph::Output<float> weight{400.0f};
  Host host;
  sigil::weave::TextStyle style = styleAt(48);
  style.shaping.typeface = ui;
  style.paint.foreground.setColor(SK_ColorWHITE);  // black-on-black otherwise
  host.composer.render(box().child(text(u8"WEIGHT", style)
                                       .key("t")
                                       .variationDrive("wght", &weight)
                                       .absolute()
                                       .inset(20, 60, 20, 60)));
  host.frame();
  SkBitmap base;
  base.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(base.pixmap(), 0, 0);

  // Liveness guard: the baseline really has ink, so "the pixels hold" below
  // is a claim about glyphs rather than about two identical blank grids.
  int inked = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      if (base.getColor(x, y) != SK_ColorBLACK) ++inked;
  ASSERT_GT(inked, 20) << "baseline text never drew";

  weight = 900.0f;
  host.frame();  // refused: draws at shaped coordinates, pixels hold
  SkBitmap after;
  after.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(after.pixmap(), 0, 0);
  for (int y = 0; y < 200; y += 4)
    for (int x = 0; x < 200; x += 4)
      ASSERT_EQ(base.getColor(x, y), after.getColor(x, y));
}

TEST(ComposeVariationDrive, TheVerbIsATrackAndComposesWithOtherTracks) {
  // variationDrive() is sugar over fx(): the same axis coordinate reached by
  // hand as a track must draw the same pixels. The equivalence is the point
  // — if the verb kept a text path of its own, a track drawn over it would
  // hide the drive entirely, which is exactly what it used to do.
  sk_sp<SkFontMgr> manager = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> ui;
  for (const char* family :
       {".AppleSystemUIFont", ".SF NS", "SF Pro Text", "SF Pro"}) {
    ui = manager->matchFamilyStyle(family, SkFontStyle());
    if (ui && fonts().axisIsAdvanceInvariant(ui, "GRAD")) break;
    ui = nullptr;
  }
  if (!ui) GTEST_SKIP() << "no advance-invariant GRAD face on this system";
  float gradeMax = 0;
  {
    const int n = ui->getVariationDesignParameters({});
    std::vector<SkFontParameters::Variation::Axis> axes((size_t)n);
    ui->getVariationDesignParameters({axes.data(), axes.size()});
    for (const auto& a : axes)
      if (a.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) gradeMax = a.max;
  }

  sigil::weave::TextStyle style = styleAt(48);
  style.shaping.typeface = ui;
  style.paint.foreground.setColor(SK_ColorWHITE);
  choreograph::Output<float> grade{gradeMax};

  Host verb;
  verb.composer.render(box().child(text(u8"GRADE", style)
                                       .key("t")
                                       .variationDrive("GRAD", &grade)
                                       .absolute()
                                       .inset(20, 60, 20, 60)));
  verb.frame();

  Host byHand;
  byHand.composer.render(
      box().child(text(u8"GRADE", style)
                      .key("t")
                      .fx({.effect = fx::axis("GRAD", gradeMax)})
                      .absolute()
                      .inset(20, 60, 20, 60)));
  byHand.frame();

  SkBitmap fromVerb, fromTrack;
  fromVerb.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  fromTrack.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  verb.surface->readPixels(fromVerb.pixmap(), 0, 0);
  byHand.surface->readPixels(fromTrack.pixmap(), 0, 0);
  constexpr size_t kRowBytes = 200 * sizeof(uint32_t);
  for (int y = 0; y < 200; ++y)
    ASSERT_EQ(std::memcmp(fromVerb.getAddr32(0, y), fromTrack.getAddr32(0, y),
                          kRowBytes),
              0)
        << "the verb and the hand-built axis track disagreed on row " << y;

  // …and the drive is no longer hidden by a track drawn over it: a second
  // track that moves the glyphs leaves the grade in place.
  Host stacked;
  stacked.composer.render(box().child(text(u8"GRADE", style)
                                          .key("t")
                                          .variationDrive("GRAD", &grade)
                                          .fx({.effect = fx::rise(0)})
                                          .absolute()
                                          .inset(20, 60, 20, 60)));
  stacked.frame();
  SkBitmap composed;
  composed.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  stacked.surface->readPixels(composed.pixmap(), 0, 0);
  for (int y = 0; y < 200; ++y)
    ASSERT_EQ(std::memcmp(fromVerb.getAddr32(0, y), composed.getAddr32(0, y),
                          kRowBytes),
              0)
        << "a stacked track dropped the driven axis, at row " << y;
}

// ---------------------------------------------------------------------------
// Shaped bindings — bind(&out).from().map().to().clamp()
