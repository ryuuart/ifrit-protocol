/** @file
 * schema_scene — a sky drawn from the sketch's own FlatBuffers schema.
 *
 * The schema beside this file states what the sketch evaluates and
 * shows: a Sky of bands is the state the scene is drawn from, and the
 * commands beside it are what moves a sky that is arriving live. The
 * build compiles the schema TWICE — `schema_scene_generated.h`, the
 * accessors that read a buffer in place, and `schema_scene_values.h`,
 * the same message as plain C++ — and this file reads the second. So
 * the scene is a value in a field: `sky.bands` is a vector, `sky.wind`
 * is a struct of two floats, and the envelope's message is a variant
 * whose alternative IS its type. Nothing here walks a pointer into
 * bytes it has to keep alive.
 *
 * The scene itself is `data/sky.json`, the schema's own JSON form. The
 * hub reads it as a resource: the flat decoder converts the file
 * through the schema the generated root carries and verifies what it
 * made, and the value is read off those bytes once. A saved edit to the
 * file is a new sky in the window, because a file a sketch read is
 * watched and a change re-declares it.
 *
 * AND A DOOR BESIDE THE FILE. A window also opens a connection on
 * `kLive` through that same schema, and asks it for the newest message
 * as an Envelope: one that carries a Sky replaces the file's sky from
 * then on — the same schema and the same value, off the wire instead of
 * off the disk, verified before a byte of it is read, taken on the
 * frame's own dispatch and read once per message. Either form arrives, since
 * the door holds the schema: a buffer, or the schema's own JSON form. The door
 * is a window's alone: a capture opens nothing, so its still is the file's sky
 * and stays what it was. Nothing is sent back through it; this sketch is a
 * reader.
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
#include <sigilcompose/kit/Document.h>
#include <sigildata/connection/Connection.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildraw/Pen.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/source/Source.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Instrument.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "schema_scene_generated.h"
#include "schema_scene_values.h"

namespace sketch = sigil::sketch;
namespace compose = sigil::compose;
namespace data = sigil::data;
namespace io = sigil::io;
namespace scene = schema_scene::values;

using sigil::draw::Pen;

namespace {

const char* kLive = "udp://:27022";  // where a window listens for a sky

constexpr float kSpacing = 104.0f;  // how far apart the bands stand
constexpr float kSegment = 260.0f;  // a ribbon's segment, and its gap after
constexpr float kGap = 70.0f;

struct SchemaScene {
  /** The sky the file states, read off the hub's bytes as a value and
   *  held. Nothing when the file is missing or does not fit. */
  std::optional<scene::Envelope> sky;
  std::string why;
  /** A WINDOW'S DOOR onto the same schema, which hands out the same
   *  value the file reads as. A connection onto nothing in a capture,
   *  which opens none. */
  data::Connection live;
  /** The newest message that read as an Envelope carrying a Sky, which
   *  stands in front of the file's sky once there is one. */
  std::optional<scene::Envelope> arrived;
  /** The bytes `arrived` was read out of, so one message is read once
   *  rather than on every frame. They are the door's own and stand as
   *  long as it holds them, which is until the next message it takes. */
  std::shared_ptr<const io::Bytes> taken;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1280, 720},
                             .captureAt = 2.0,
                             .background = SkColor4f{0.04f, 0.04f, 0.09f, 1}});
    // THE SKETCH'S OWN SCHEMA DECODES THE SKETCH'S OWN SCENE. The
    // generated root carries the schema, so the hub converts the JSON
    // form through it and verifies what it made; registering the root
    // again on a later declaration replaces the decoder, harmlessly.
    data::registerFlatBuffer<schema_scene::Envelope>(ctx.assets.hub());
    const std::shared_ptr<const data::FlatBuffer<schema_scene::Envelope>> read =
        ctx.assets.hub().load<data::FlatBuffer<schema_scene::Envelope>>(
            ctx.local("data/sky.json"));
    sky.reset();
    why.clear();
    if (read) {
      sky = scene::readEnvelope(std::as_bytes(read->bytes()));
      if (!sky) why = "the scene does not read as an Envelope";
    } else {
      // The same conversion, asked for the value and the reason at
      // once, so the caption says what the file did wrong.
      const std::optional<std::string> text =
          ctx.assets.hub().text(ctx.local("data/sky.json"));
      if (!text)
        why = "data/sky.json is missing";
      else
        sky = scene::fromJson(*text, &why);
    }
    // THE DOOR A WINDOW OPENS. A capture opens none: its still is a
    // function of the files beside it, and a port is not one of those.
    live = data::Connection();
    arrived.reset();
    taken.reset();
    if (!ctx.deterministic)
      live = data::Connection(ctx.assets.hub(), kLive,
                              data::schema<schema_scene::Envelope>());
    ctx.composer.render(sketch::kit::instrument(
        {.page = {.title = "A scene, described as data.",
                  .subtitle = "FLATBUFFERS / SCHEMA  /  A file and a live "
                              "message share one description.",
                  .footer = "Edit data/sky.json and save  ·  Send a matching "
                            "Envelope to udp://:27022"},
         .pictureWidth = 816,
         .readingsLabel = "SOURCE",
         .note = "The schema owns the bands, wind and palette. The renderer "
                 "draws the verified value."},
        compose::pen("schema_scene.sky", [this](Pen& pen) { draw(pen); })
            .fill(SkColor4f{0.04f, 0.04f, 0.09f, 1}),
        compose::box().column().gap(18).children(
            {compose::document::eyebrow("DOCUMENT"),
             compose::text("data/sky.json"),
             compose::document::eyebrow("SCHEMA"),
             compose::text("Envelope → Sky"),
             compose::document::eyebrow("LIVE INPUT"), compose::text(kLive),
             compose::document::caption(
                 "A valid live message replaces the file's sky. An "
                 "invalid message leaves the last picture in place.")})));
  }

  /** The newest message on the door, read once each time one arrives:
   *  a message that reads as this schema's Envelope and carries a Sky
   *  becomes the sky the scene is drawn from. One that is something
   *  else leaves the sky where it is — a sender at the wrong port
   *  cannot blank the scene. What says a message is new is the bytes
   *  the door's own dispatch took, so a door nothing has arrived at,
   *  and a capture's door onto nothing, read nothing at all. Nothing is
   *  described again, because the pen reads the sky it draws on every
   *  frame. */
  void update() {
    std::shared_ptr<const io::Bytes> newest = live.latestBytes();
    if (!newest || newest == taken) return;
    taken = std::move(newest);
    std::optional<scene::Envelope> read = live.latest<scene::Envelope>();
    if (read && std::holds_alternative<scene::Sky>(read->message))
      arrived = std::move(read);
  }

  /** The bands: each a ribbon of segments the width of the canvas,
   *  drifting on the wind plus its own speed and breathing by its
   *  wobble, tinted from the palette in turn. */
  void draw(Pen& pen) {
    pen.noStroke();
    // What arrived stands in front of what was read.
    const std::optional<scene::Envelope>& held = arrived ? arrived : sky;
    const scene::Sky* state =
        held ? std::get_if<scene::Sky>(&held->message) : nullptr;
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
    const float wind = state->wind.x;
    const float period = kSegment + kGap;
    int i = 0;
    for (const scene::Band& band : state->bands) {
      if (!state->palette.empty()) {
        const scene::Color& tint =
            state->palette[(size_t)i % state->palette.size()];
        pen.fill(tint.r * 255, tint.g * 255, tint.b * 255, tint.a * 255);
      } else {
        pen.fill(255, 255, 255, 120);
      }
      const float drift = (wind + band.speed) * seconds;
      const float phase = std::fmod(std::fmod(drift, period) + period, period);
      const float y = 80 + kSpacing * (float)i +
                      band.wobble * std::sin(seconds * 0.7f + (float)i);
      for (float x = phase - period; x < pen.width; x += period)
        pen.rect(x, y, kSegment, band.height, band.height * 0.5f);
      ++i;
    }
  }
};

}  // namespace

SIGIL_SKETCH(SchemaScene, "Data",
             "A sky read from the sketch's own FlatBuffers schema: "
             "data/sky.json in the schema's form, converted through it, "
             "verified and drawn.")
