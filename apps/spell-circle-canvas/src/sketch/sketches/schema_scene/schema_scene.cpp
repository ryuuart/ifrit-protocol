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
 * AND A DOOR BESIDE THE FILE. A window also opens a feed on `kLive`,
 * and a message that arrives there and verifies as an Envelope carrying
 * a Sky replaces the file's sky from then on — the same schema, off the
 * wire instead of off the disk, verified before a byte of it is read
 * and never decoded twice. The door is a window's alone: a capture
 * opens nothing, so its still is the file's sky and stays what it was.
 * Nothing is sent back through it; this sketch is a reader.
 *
 * EDIT THESE FIRST
 *   data/sky.json  the sky: its bands, its wind and its palette
 *   kLive          the URI a window listens for a sky on
 *   kSpacing       how far apart the bands stand
 *   kSegment       the length of a ribbon's segments, and their gap
 */

// TAGS: Data/Schema, Data/Sources

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "schema_scene_generated.h"

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;

using sigil::draw::Pen;

namespace {

const char* kLive = "udp://:27022";  // where a window listens for a sky

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
  /** A WINDOW'S DOOR onto the same schema. Null in a capture, which
   *  opens none. */
  std::shared_ptr<io::Feed> live;
  /** The newest message that verified as an Envelope carrying a Sky,
   *  which stands in front of the file's sky once there is one. */
  std::shared_ptr<const Envelope> arrived;
  /** The generation `arrived` was read at, so one message is verified
   *  once rather than on every frame. */
  uint64_t taken = 0;

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
    // THE DOOR A WINDOW OPENS. A capture opens none: its still is a
    // function of the files beside it, and a port is not one of those.
    live.reset();
    arrived.reset();
    taken = 0;
    if (!ctx.deterministic) live = ctx.assets.hub().feed(kLive);
    ctx.composer.render(
        compose::pen("schema_scene.sky", [this](Pen& pen) { draw(pen); }));
  }

  /** The newest message on the door, taken once each time one arrives:
   *  bytes that verify as this schema's Envelope and carry a Sky become
   *  the sky the scene is drawn from. Bytes that are something else are
   *  left where they are — a sender at the wrong port cannot blank the
   *  scene. Nothing is described again, because the pen reads the sky
   *  it draws on every frame. */
  void update() {
    if (!live) return;
    const uint64_t generation = live->generation();
    if (generation == taken) return;
    taken = generation;
    const std::shared_ptr<const io::Bytes> newest = live->latest();
    if (!newest) return;
    std::optional<Envelope> verified =
        data::flatBufferFromBytes<schema_scene::Envelope>(
            {reinterpret_cast<const uint8_t*>(newest->bytes.data()),
             newest->bytes.size()});
    if (verified && (*verified)->message_as_Sky())
      arrived = std::make_shared<const Envelope>(std::move(*verified));
  }

  /** The bands: each a ribbon of segments the width of the canvas,
   *  drifting on the wind plus its own speed and breathing by its
   *  wobble, tinted from the palette in turn. */
  void draw(Pen& pen) {
    pen.noStroke();
    // What arrived stands in front of what was read.
    const std::shared_ptr<const Envelope>& scene = arrived ? arrived : sky;
    const schema_scene::Sky* state =
        scene ? (*scene)->message_as_Sky() : nullptr;
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
