// THE KIT'S ENTRANCE PRESETS over a text leaf: a staggered rise revealing in
// order, an underlay beneath the stroke, and a progress driven by a
// transition or by a binding.
//
// The text binary's share of the content suites, one file per subject.

#include "DressedTypeProbes.h"

TEST(ComposeKinetic, StaggeredRiseRevealsInOrder) {
  // The stagger law: at mid-progress the early glyphs are fully revealed
  // while the late ones haven't started — the canonical staggered reveal,
  // rendered through batched RSXform draws. Twelve glyphs of 19.2 px
  // in the instrument face at 32 px, plus the padding, need the width.
  Host host(300, 200);
  auto tree = [](sigil::motion::Animatable<float> progress) {
    return box().padding(10).child(
        text(u8"IIIIIIIIIIII", whiteStyle(32))
            .key("k")
            .fx({.effect = fx::rise(24),
                 .stagger = {.eachMs = 40, .durationMs = 200},
                 .progress = std::move(progress)}));
  };
  host.composer.render(tree(0.0f));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect leftEdge = SkIRect::MakeLTRB(
      (int)b->left(), (int)b->top(), (int)b->left() + 24, (int)b->bottom());
  const SkIRect rightEdge = SkIRect::MakeLTRB(
      (int)b->right() - 24, (int)b->top(), (int)b->right(), (int)b->bottom());
  EXPECT_FALSE(anyWhiteIn(host, leftEdge));  // progress 0: nothing revealed
  host.composer.render(tree(0.45f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, leftEdge));    // head fully in
  EXPECT_FALSE(anyWhiteIn(host, rightEdge));  // tail not started
  host.composer.render(tree(1.0f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, rightEdge));  // everything landed
}

TEST(ComposeKinetic, ATrackKeepsABlurredUnderlayBeneathTheStroke) {
  // A dressed style through the track's batched draw: a dark blurred
  // stroke underlay must stay BENEATH the light stroked foreground — the
  // halo hugs the letterform, the stroke keeps its colour — including
  // mid-cascade, when each glyph's own fade splits the style across
  // several paint buckets and a later letter's halo reaches a landed
  // letter's stroke. The reference is the same cascade with the style cut
  // into two tracked nodes, halo under stroke by stacking order: the one
  // dressed node must composite exactly as that split does.
  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(4.0f);
  stroke.setColor(SK_ColorWHITE);
  SkPaint halo;
  halo.setAntiAlias(true);
  halo.setStyle(SkPaint::kStroke_Style);
  halo.setStrokeWidth(8.0f);
  halo.setColor(0xFF000000);
  const sigil::weave::PaintLayer blurredHalo =
      sigil::weave::PaintLayer::blurred(halo, 4);

  sigil::weave::TextStyle dressed;
  dressed.shaping.fontSize = 64.0f;
  dressed.paint.foreground = stroke;
  dressed.paint.underlays.push_back(blurredHalo);
  sigil::weave::TextStyle haloOnly = dressed;
  haloOnly.paint.foreground = blurredHalo.paint;
  haloOnly.paint.underlays.clear();
  sigil::weave::TextStyle strokeOnly = dressed;
  strokeOnly.paint.underlays.clear();

  constexpr float kMidCascade = 0.45f;
  const auto tracked = [&](const sigil::weave::TextStyle& style,
                           const char* key) {
    return text(u8"OOOOO", style)
        .key(key)
        .absolute()
        .inset(20, 20, 20, 20)
        .fx({.effect = fx::pop(),
             .stagger = {.eachMs = 30, .durationMs = 480},
             .progress = kMidCascade});
  };

  Host actual(420, 140);
  actual.composer.render(box().child(tracked(dressed, "word")));
  actual.frame();
  Host expected(420, 140);
  expected.composer.render(box()
                               .child(tracked(haloOnly, "halos"))
                               .child(tracked(strokeOnly, "strokes")));
  expected.frame();

  // The two trees are not the same tree -- one dressed node against two
  // tracked ones -- so their rasters cannot be compared byte for byte, and
  // a tolerance fitted to the difference would be a number about this
  // machine. The failure this case exists for is one-sided instead: a halo
  // that lands OVER its stroke DARKENS the stroke, so the dressed node can
  // only ever have fewer bright pixels than the split reference, never
  // more. Counting them says exactly that with no tolerance at all.
  const auto brightPixels = [](Host& host) {
    int n = 0;
    for (int y = 0; y < 140; ++y)
      for (int x = 0; x < 420; ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetR(c) > 200 && SkColorGetG(c) > 200 &&
            SkColorGetB(c) > 200)
          ++n;
      }
    return n;
  };
  const int split = brightPixels(expected);
  ASSERT_GT(split, 100) << "the reference drew no strokes, so nothing below "
                           "this line tested anything";
  EXPECT_GE(brightPixels(actual), split)
      << "the dressed node's underlay composited OVER its strokes and dimmed "
         "them: the halo is not beneath the letterform";
}

TEST(ComposeKinetic, TransitionedProgressPaintsLive) {
  // The master progress takes the full Animatable treatment: a with()
  // transition animates the reveal and the node paints live while moving.
  Host host;
  auto tree = [](sigil::motion::Animatable<float> progress) {
    return box().padding(10).child(
        text(u8"POP", whiteStyle(40))
            .key("k")
            .fx({.effect = fx::pop(),
                 .stagger = {.eachMs = 20, .durationMs = 150},
                 .progress = std::move(progress)}));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  host.composer.render(
      tree(animate(sigil::motion::to(1.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2);                                    // mid-ramp
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // live while animating
  host.frame(0.3);                                    // settle
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())));
}

TEST(ComposeKinetic, ABoundProgressRevealsWithoutARedescribe) {
  // Glyph progress must be classified as CONTENT volatility, not paint-only:
  // it rebuilds glyph geometry, so the node's own recording is invalid the
  // moment it ticks. Classified paint-only, computeVolatile leaves
  // `ownContent` and `subtreeVolatile` false, the picture is never reset,
  // and the reveal FREEZES at whatever progress the last describe recorded.
  //
  // Driving the reveal from a BOUND Output is the only way to see that. Any
  // case that moves the reveal by RE-DESCRIBING marks the node paint-dirty
  // and hides the question entirely, and a case that only checks for ink
  // after settling is satisfied by a frozen half-revealed recording. Hence
  // both halves here: the tail must be dark before, and lit after, with no
  // describe in between. Twelve glyphs of 19.2 px in the instrument face at
  // 32 px, plus the padding, need the width.
  Host host(300, 200);
  choreograph::Output<float> progress{0.0f};
  host.composer.render(box().padding(10).child(
      text(u8"IIIIIIIIIIII", whiteStyle(32))
          .key("k")
          .fx({.effect = fx::rise(24),
               .stagger = {.eachMs = 40, .durationMs = 200},
               .progress = &progress})));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect tail = SkIRect::MakeLTRB((int)b->right() - 24, (int)b->top(),
                                         (int)b->right(), (int)b->bottom());
  ASSERT_FALSE(anyWhiteIn(host, tail))
      << "the tail was already revealed at progress 0, so the check below "
         "cannot tell a live reveal from a frozen one";

  progress = 1.0f;  // ONE describe, and the value moves underneath it
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, tail))
      << "the tail never appeared: the node replayed the recording it made at "
         "progress 0, so glyph progress is not invalidating its own picture";
}
