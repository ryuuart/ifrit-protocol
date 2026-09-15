/** @file
 * schema_scene — a sky drawn from the sketch's own FlatBuffers schema.
 *
 * The schema beside this file states what the sketch evaluates and
 * shows: a Sky of bands is the state the scene is drawn from, and the
 * commands beside it are what moves a sky that is arriving live. The
 * build compiles the schema into `schema_scene_generated.h`, which is
 * what this file includes, and the scene itself is `data/sky.json`, the
 * schema's own JSON form. The hub reads it as a resource: the flat
 * decoder converts the file through the schema the generated root
 * carries, verifies what it made, and the sketch reads the root in
 * place. A saved edit to the file is a new sky in the window, because a
 * file a sketch read is watched and a change re-declares it.
 *
 * EDIT THESE FIRST
 *   data/sky.json  the sky: its bands, its wind and its palette
 *   kSpacing       how far apart the bands stand
 *   kSegment       the length of a ribbon's segments, and their gap
 */

// TAGS: Data/Schema, Data/Sources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <cmath>
#include <memory>
#include <optional>
#include <string>

#include "schema_scene_generated.h"

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;

using sigil::draw::Pen;

namespace {

constexpr float kSpacing = 104.0f;  // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;

/** The scene as the schema states it: an Envelope, read in place. */
using Envelope = data::FlatBuffer<schema_scene::Envelope>;

struct SchemaScene {
  /** The hub's view of data/sky.json: cached, and read again when the
   *  file changes. Null when the file is missing or does not fit. */
  std::shared_ptr<const Envelope> sky;
  std::string why;

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = {1280, 720},
                             .captureAt = 2.0,
                             .background = SkColor4f{0.04f, 0.04f, 0.09f, 1}});
    // THE SKETCH'S OWN SCHEMA DECODES THE SKETCH'S OWN SCENE. The
    // generated root carries the schema, so the hub converts the JSON
    // form through it and verifies what it made; registering the root
    // again on a later declaration replaces the decoder, harmlessly.
    data::registerFlatBuffer<schema_scene::Envelope>(ctx.assets.hub());
    sky = ctx.assets.hub().load<Envelope>(ctx.local("data/sky.json"));
    why.clear();
    if (!sky) {
      // The reason, for the caption: the same conversion, asked for it.
      const std::optional<std::string> text =
          ctx.assets.hub().text(ctx.local("data/sky.json"));
      if (!text)
        why = "data/sky.json is missing";
      else
        data::flatBufferFromJson<schema_scene::Envelope>(*text, &why);
    }
    ctx.composer.render(
        compose::pen("schema_scene.sky", [this](Pen& pen) { draw(pen); }));
  }

  /** The bands: each a ribbon of segments the width of the canvas,
   *  drifting on the wind plus its own speed and breathing by its
   *  wobble, tinted from the palette in turn. */
  void draw(Pen& pen) {
    pen.noStroke();
    const schema_scene::Sky* state = sky ? (*sky)->message_as_Sky() : nullptr;
    if (!state) {
      pen.fill(255, 90, 120);
      pen.textSize(18);
      pen.text(
          "no sky: " +
              (sky ? std::string("the scene's message is not a Sky") : why),
          40, 60);
      return;
    }
    const float seconds = (float)(pen.millis() * 0.001);
    const auto* palette = state->palette();
    const float wind = state->wind() ? state->wind()->x() : 0.0f;
    const float period = kSegment + kGap;
    int i = 0;
    for (const schema_scene::Band* band : *state->bands()) {
      if (palette && palette->size() > 0) {
        const schema_scene::Color* tint = palette->Get(i % palette->size());
        pen.fill(tint->r() * 255, tint->g() * 255, tint->b() * 255,
                 tint->a() * 255);
      } else {
        pen.fill(255, 255, 255, 120);
      }
      const float drift = (wind + band->speed()) * seconds;
      const float phase = std::fmod(std::fmod(drift, period) + period, period);
      const float y = 80 + kSpacing * (float)i +
                      band->wobble() * std::sin(seconds * 0.7f + (float)i);
      for (float x = phase - period; x < pen.width; x += period)
        pen.rect(x, y, kSegment, band->height(), band->height() * 0.5f);
      ++i;
    }
  }
};

}  // namespace

SIGIL_SKETCH(SchemaScene, "Data",
             "A sky read from the sketch's own FlatBuffers schema: "
             "data/sky.json in the schema's form, converted through it, "
             "verified and drawn.")
