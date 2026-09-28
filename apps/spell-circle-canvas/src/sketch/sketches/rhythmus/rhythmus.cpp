/** A concert poster built from an asymmetric typographic grid. The red
 *  syllable drifts within its register while the programme remains fixed. */
// TAGS: Studies/Graphic Design, Typography/Posters, Layout/Grid,
// Motion/Bindings

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cmath>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
const auto paper = material::hexColor(0xEAE5D8);
const auto black = material::hexColor(0x171E1B);
const auto red = material::hexColor(0xDC3528);

Text type(std::string words, float size, bool heavy = false,
          material::Color color = black) {
  return text(std::move(words))
      .font({.face = weave::ports::face(
                 {"Helvetica Neue", "Helvetica", "Arial"}, heavy ? 700 : 400),
             .size = size,
             .track = heavy ? -size * 0.048f : 0.0f})
      .ink(color);
}
}  // namespace

struct Rhythmus {
  motion::Animatable<float> displacement = motion::animatable(0.0f);

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1000, 1400);
    ctx.background(paper);
    ctx.captureAt(3.2);
    std::vector<Element> poster{
        kit::at(0, 0, 1000, 1400)
            .fill(material::from(paper).layer(
                material::noise(0.55f, {.seed = 21, .grain = true}),
                {.blend = material::BlendMode::Multiply, .opacity = 0.04f}))
            .cache(Cache::Texture),
        kit::at(type("tonhalle\nzürich", 26, true), 57, 44, 340, 72),
        kit::at(type("neue musik\nkonzertreihe 04", 17), 723, 50, 226, 51),
        kit::at(58, 150, 884, 3).fill(black),
        kit::at(type("rhyth", 282, true), 41, 200, 930, 345),
        kit::at(type("mus", 328, true, red), 319, 438, 650, 398)
            .translateX(displacement),
        kit::at(type("zeit wird form", 54, true), 57, 848, 880, 79),
        kit::at(58, 960, 884, 13).fill(red),
        kit::at(type("01", 115, true, red), 51, 989, 240, 141),
        kit::at(type("02", 115, true), 347, 989, 240, 141),
        kit::at(type("03", 115, true), 643, 989, 240, 141),
        kit::at(type("puls / impuls", 24, true), 59, 1138, 274, 40),
        kit::at(type("raum / struktur", 24, true), 354, 1138, 274, 40),
        kit::at(type("zeit / bewegung", 24, true), 651, 1138, 290, 40),
        kit::at(
            type("percussion ensemble\nfreitag  18. oktober\n19.30 uhr", 17),
            59, 1191, 274, 79),
        kit::at(type("kammerorchester\nsamstag  19. oktober\n20.00 uhr", 17),
                354, 1191, 274, 79),
        kit::at(
            type("elektronisches studio\nsonntag  20. oktober\n18.00 uhr", 17),
            651, 1191, 290, 79),
        kit::at(58, 1314, 884, 1).fill(black),
        kit::at(type("ein typografisches konzert", 14), 59, 1341, 590, 26),
        kit::at(type("RH / 04", 14, true), 830, 1341, 118, 26)};
    for (int beat = 0; beat < 12; ++beat) {
      poster.push_back(
          kit::at(61 + beat * 25.0f, 774, beat % 3 ? 2 : 6, beat % 3 ? 19 : 43)
              .fill(black));
    }
    ctx.composer.render(
        positioned().width(1000).height(1400).children(std::move(poster)));
  }

  void update(double elapsed) {
    displacement = 12.0f * std::sin(float(elapsed) * 0.785398f);
  }
};

SIGIL_SKETCH(Rhythmus, "Study · Type",
             "Swiss concert poster: asymmetric registers, dense grotesk and a "
             "moving red syllable")
