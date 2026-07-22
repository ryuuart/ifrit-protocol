// frame_asset.cpp — an ASSET sketch: authors a 96px carved nine-slice
// frame at exact pixel size, for headless export:
//
//   ComposeSketch sketches/frame_asset.cpp \
//       --frame frame.png --at 0
//
// The canvas IS the asset: ctx.canvas(96, 96) with --scale 1 (the
// default) captures exactly 96×96 pixels, ready for
// SigilLoader + Slice{xDivs {32,64}, yDivs {32,64}} in any demo.
// Edit-and-save still live-previews in the windowed host; Cmd+S drops
// numbered captures next to the sketch.

#include <sigilsketch/Sketch.h>

#include <include/core/SkPathBuilder.h>

using namespace sigil::compose;
using namespace sigil::compose::util;

struct FrameAsset : sigil::compose::sketch::Sketch {
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(96, 96);
    ctx.background({0, 0, 0, 0}); // transparent: it's a texture

    ctx.composer.render(
        box().inset(0)
            .child(custom([](SkCanvas &c, const PaintContext &paint) {
                     const float s = paint.size.width();
                     SkPaint brush;
                     brush.setAntiAlias(true);

                     // Parchment center (stretches under content).
                     brush.setColor4f({0.88f, 0.80f, 0.62f, 0.96f},
                                      nullptr);
                     c.drawRoundRect(
                         SkRect::MakeLTRB(5, 5, s - 5, s - 5),
                         s * 0.14f, s * 0.14f, brush);

                     // Wood band + gilded trims.
                     brush.setStyle(SkPaint::kStroke_Style);
                     brush.setStrokeWidth(s * 0.10f);
                     brush.setColor4f({0.36f, 0.22f, 0.11f, 1}, nullptr);
                     c.drawRoundRect(
                         SkRect::MakeLTRB(s * 0.07f, s * 0.07f,
                                          s * 0.93f, s * 0.93f),
                         s * 0.16f, s * 0.16f, brush);
                     brush.setStrokeWidth(1.6f);
                     brush.setColor4f({0.85f, 0.64f, 0.22f, 1}, nullptr);
                     c.drawRoundRect(
                         SkRect::MakeLTRB(s * 0.135f, s * 0.135f,
                                          s * 0.865f, s * 0.865f),
                         s * 0.10f, s * 0.10f, brush);

                     // Corner bosses with gilded studs.
                     brush.setStyle(SkPaint::kFill_Style);
                     const float at[4][2] = {{0.13f, 0.13f},
                                             {0.87f, 0.13f},
                                             {0.87f, 0.87f},
                                             {0.13f, 0.87f}};
                     for (auto &corner : at) {
                       brush.setColor4f({0.22f, 0.13f, 0.07f, 1},
                                        nullptr);
                       c.drawCircle(s * corner[0], s * corner[1],
                                    s * 0.085f, brush);
                       brush.setColor4f({0.85f, 0.64f, 0.22f, 1},
                                        nullptr);
                       SkPathBuilder d;
                       const float r = s * 0.042f;
                       d.moveTo(s * corner[0], s * corner[1] - r);
                       d.lineTo(s * corner[0] + r, s * corner[1]);
                       d.lineTo(s * corner[0], s * corner[1] + r);
                       d.lineTo(s * corner[0] - r, s * corner[1]);
                       d.close();
                       c.drawPath(d.detach(), brush);
                     }
                   })
                       .inset(0)
                       .cache(Cache::None)));
  }
};

SIGIL_SKETCH(FrameAsset)
