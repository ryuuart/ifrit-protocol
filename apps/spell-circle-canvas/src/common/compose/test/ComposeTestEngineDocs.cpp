#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Rails.h>

#include "support/DocsTestSupport.h"

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
  auto edges = shapes::onEdges(shapes::Edge::Top, stroke(2.0f, ink));

  auto glow =
      kit::brush::presets::filament({0.4f, 0.8f, 1, 1}, {0.9f, 1, 1, 1}, 1.0f);
  auto trace = kit::brush::presets::circuit({0.2f, 0.9f, 0.8f, 1}, 1);
  auto cord = kit::brush::presets::rope(1, 1.0f);
  // OP FIRST, decoration second. The two arguments read equally well in
  // either order in prose, which is why the call is spelled here.
  auto restyled = brush::restyle(kit::brush::shapers::Jitter{8.0f, 2.0f, 7},
                                 stroke(1.0f, ink), 8.0f);

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
  PathFormat inner = stroke(2.0f, ink, PathFormat::Align::Inner);
  PathFormat outer = stroke(2.0f, ink, PathFormat::Align::Outer);
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
  auto shadow = sigil::compose::shadow({0, 0, 0, 0.5f}, {0, 2}, 8.0f);

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
  (void)stroke(2.0f, Fill::color({1, 1, 1, 1}));
  PathFormat stroked = stroke(2.0f, Fill::color({1, 1, 1, 1}));
  stroked.strokeMaterial = radU;  // a stroke takes a Material, not only a Fill
  (void)decorations::wash(glowU, SkBlendMode::kOverlay, 0.5f);

  (void)sweep;
  (void)sksl;
  (void)blended;
  (void)stroked;
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

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>

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

// ---------------------------------------------------------------------------
// Shaped bindings — bind(&out).from().map().to().clamp()
