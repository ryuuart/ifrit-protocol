/** @file
 * daemon console — a log feed that scrolls itself: generated rows fed
 * into a ring of STRUCTURED rows, with severity dressing and a cursor
 * that keeps up.
 */

// A security-operations console — WARDNET, the ward-perimeter watch — built
// entirely out of composition. One panel carries four registers of type and
// three panes of chrome, and the whole surface is priced by the feed idiom:
//
//   scrollback ..... feed::Ring<LogRow> + feed(). A row is a value with
//                    fields, not a line of text, and rows are keyed by
//                    sequence id — so an append reconciles as ONE row mount
//                    and every row already on screen keeps its cached
//                    picture, whatever the ring's capacity.
//   rows ........... each row is a stripe, a chip band and ONE rich() text
//                    leaf: tabular-timestamp, channel tag and payload each in
//                    their own named style, with an optional cipher field.
//                    Severity is encoded in form as well as colour — the
//                    stripe, the tag's ink, and a wash behind a breach line.
//   entrances ...... chosen PER SEVERITY, and every one SETTLES: traces fade,
//                    info types on, warnings rise glyph by glyph, breaches
//                    slam in whole with a screened flash that decays to the
//                    ink's own red. A cipher field is vetoed by fx::hold
//                    until its beat opens, churns hex, and resolves. The
//                    frame a track ends, its row is a cached static leaf
//                    again.
//   fade-out ....... a panel-coloured gradient laid over the top of the
//                    well, so the oldest rows dim without any row node
//                    being re-patched
//   rail ........... channel meters as bars on BOUND outputs (paint-only
//                    volatility, no describes), severity counters and the
//                    uplink lamp patched only when an append re-describes
//   prompt ......... a caret that behaves like one: solid while the console
//                    types a command, blinking while idle, and always sitting
//                    at the end of the typed text because it is the next
//                    sibling in the row
//   chrome ......... an SDF roundBox panel (fill, border, glow in one pass),
//                    hairline rules, and the tube: a scanline tile crept by
//                    a bound pan and a refresh band baked once and slid by
//                    a bound translate — no per-pixel program runs per frame
//
// The scene is re-rendered on every append and every prompt keystroke, and
// reconciliation touches a constant handful of nodes for each: the new row's
// mount plus the few chrome leaves whose text actually changed. The retained
// instance tree is what makes that work; there is no virtualizer.
//
// EDIT THESE FIRST
//   the Ring's capacity  — how much scrollback is retained. Rows past it
//                          leave; nothing on screen is re-patched when
//                          they do.
//   LogGen's roll table  — the mix of severities, and therefore which
//                          entrances the page is a specimen of.
//   kCommands            — what the console types at its own prompt.
//   the palette block    — the whole surface is dressed out of it, and
//                          severity is encoded in ink as well as in form.

#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Features.h>
#include <sigilweave/style/Type.h>
#include <sigilweave/style/Features.h>

#include <include/core/SkPaint.h>

#include <cmath>
#include <format>
#include <random>
#include <string>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace sdf = sigil::material::sdf;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::skia::Paint;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace daemon_console {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

// ---- palette: graphite steel, phosphor accents ----------------------------
constexpr SkColor4f kVoid = hex(0x04060B);
constexpr SkColor4f kGroundTop = hex(0x0A101A);
constexpr SkColor4f kPanel = hex(0x0C121C, 0.97f);
constexpr SkColor4f kRule = hex(0x22344A);
constexpr SkColor4f kAccent = hex(0x59CBE3);
constexpr SkColor4f kBone = hex(0xE8EFF6);
constexpr SkColor4f kChrome = hex(0x8296AE);
constexpr SkColor4f kDim = hex(0x49596D);
constexpr SkColor4f kBody = hex(0xAFC8BB);
constexpr SkColor4f kOk = hex(0x49D6A2);
constexpr SkColor4f kWarn = hex(0xF2B04E);
constexpr SkColor4f kCrit = hex(0xFF5752);
constexpr SkColor4f kCritText = hex(0xFF7A73);
constexpr SkColor4f kMeterBed = hex(0x16202E);

// ---- severities -----------------------------------------------------------
enum Sev : int { kTrace = 0, kInfo, kSeal, kFlux, kBreach, kSevCount };

/** How one severity dresses its row: the stripe's ink, the channel tag's
 *  and the payload's named styles. The tag NAMES the channel; its COLOUR is
 *  the severity — two dimensions on one four-letter word. */
struct SevDress {
  SkColor4f stripe;
  const char* tagStyle;
  const char* bodyStyle;
};
inline const SevDress& dress(int sev) {
  static const SevDress kDress[kSevCount] = {
      {alpha(kDim, 0.55f), "tag-trace", "trace"},
      {alpha(kChrome, 0.6f), "tag-info", ""},
      {kOk, "tag-seal", "seal"},
      {kWarn, "tag-flux", "flux"},
      {kCrit, "tag-breach", "breach"},
  };
  return kDress[sev];
}

/** One log row: mission time, severity, a four-letter channel tag, the
 *  payload, and an optional cipher field that decodes on arrival. */
struct LogRow {
  double t = 0.0;
  int sev = kInfo;
  std::string tag;
  std::string body;
  std::string cipher;
  bool operator==(const LogRow&) const = default;
};

/** THE TUBE, in two pieces whose only per-frame input is a phase — so
 *  neither runs a program per pixel per frame.
 *
 *  The scanline field is a sine over y, and a field that is periodic in y
 *  is a TILE: ten periods baked once into a strip, repeated across the
 *  panel, and crept by the bound pan the tile's material carries. The
 *  strip holds a whole number of pixels, so its period lands a hair off
 *  the sine's own; the eye reads a 3.7 px scanline either way. */
constexpr float kScanPeriods = 10.0f;
constexpr float kScanTileH = 37.0f;  // ten periods of 2pi / 1.7 px, whole
constexpr float kScanCreep = 9.0f / 1.7f;  // px per second, downward
constexpr SkColor4f kTubeInk{0.55f, 0.85f, 0.95f, 1.0f};

inline Pattern scanlineTile() {
  return Pattern::tile({4.0f, kScanTileH}, [](SkCanvas& canvas, SkSize size,
                                                uint32_t) {
    SkPaint row;
    for (int y = 0; y < (int)size.height(); ++y) {
      const float phase =
          ((float)y + 0.5f) / size.height() * kScanPeriods * 6.2831853f;
      const float a = 0.028f * (0.5f + 0.5f * std::sin(phase));
      row.setColor4f({kTubeInk.fR, kTubeInk.fG, kTubeInk.fB, a});
      canvas.drawRect(SkRect::MakeXYWH(0, (float)y, size.width(), 1), row);
    }
  });
}

/** The refresh band: a 128 px tent, brightest at its centre, that sweeps
 *  down the panel — one gradient, baked once, slid by a bound translate. */
constexpr float kRefreshH = 128.0f;
constexpr float kRefreshSpeed = 90.0f;  // px per second
constexpr float kRefreshWrap = 820.0f;  // the sweep's period, in px
inline Paint refreshBand() {
  return Paint::linear({0, 0}, {0, kRefreshH},
                       {{0.0f, {kTubeInk.fR, kTubeInk.fG, kTubeInk.fB, 0.0f}},
                        {0.5f, {kTubeInk.fR, kTubeInk.fG, kTubeInk.fB, 0.045f}},
                        {1.0f, {kTubeInk.fR, kTubeInk.fG, kTubeInk.fB, 0.0f}}});
}

/** Seeded pseudo-log: plausible ward-perimeter chatter with severities and
 *  running counters for the rail. */
struct LogGen {
  // a fixed seed; the scene must render the same on every run
  // NOLINTNEXTLINE(bugprone-random-generator-seed)
  std::mt19937 rng{2077};
  int packet = 41210;
  unsigned seals = 0, warns = 0, breaches = 0;
  uint64_t events = 0;

  uint64_t emitRow(sigil::compose::feed::Ring<LogRow>& ring, double t) {
    ++events;
    packet += (int)(rng() % 97);
    const int roll = (int)(rng() % 100);
    if (roll < 8) {
      ++breaches;
      LogRow row{t, kBreach, "WARD",
                 std::format("BREACH sector {:02} \xc2\xb7 rerouting gate {}",
                             (unsigned)(rng() % 13), (unsigned)(rng() % 7))};
      if (rng() % 2)
        row.cipher =
            std::format("{:04x}\xc2\xb7{:04x}", (unsigned)(rng() % 0xffff),
                        (unsigned)(rng() % 0xffff));
      return ring.append(std::move(row));
    }
    if (roll < 22) {
      ++warns;
      return ring.append({t, kFlux, "FLUX",
                          std::format("sigil flux {:.2f} mS over damping floor",
                                      0.4 + (double)(rng() % 90) / 100.0)});
    }
    if (roll < 32) {
      ++seals;
      return ring.append(
          {t, kSeal, "SEAL",
           std::format("ward seal reforged \xc2\xb7 sector {:02} "
                       "holding",
                       (unsigned)(rng() % 13))});
    }
    if (roll < 62)
      return ring.append({t, kTrace, "LATT",
                          std::format("lattice sweep {:06x} \xc2\xb7 {} pts ok",
                                      packet, (unsigned)(64 + rng() % 900))});
    return ring.append(
        {t, kInfo, "AUTH",
         std::format("daemon[{}] bound :6{:03} \xc2\xb7 handshake",
                     (unsigned)(rng() % 9), (unsigned)(rng() % 1000)),
         std::format("{:04x}\xc2\xb7{:04x}", (unsigned)(rng() % 0xffff),
                     (unsigned)(rng() % 0xffff))});
  }
};

/** The commands the console runs at its own prompt, cycled in order. */
inline const char* kCommands[] = {
    "trace --lattice --deep", "reseal sector 07", "route gate 4 --drain",
    "damp flux 0.60",         "audit auth ring",
};
constexpr int kCommandCount = 5;

}  // namespace daemon_console

struct DaemonConsole final : sketch::Sketch {
  sigil::compose::feed::Ring<daemon_console::LogRow> ring{256};
  daemon_console::LogGen gen;

  // Bound outputs: the only values volatile forever, all paint-only.
  // The caret's clock, in seconds: the square() binding on the caret turns
  // it into the blink, and the prompt machine REBASES it — held at 0 (the
  // pulse's ON phase) while a command types, released to the mission clock
  // while the console waits.
  choreograph::Output<float> caretClock{0.0f};
  choreograph::Output<float> lamp{1.0f};
  choreograph::Output<float> meter[4] = {{0.5f}, {0.5f}, {0.5f}, {0.5f}};
  // The tube's two phases: where the scanline tile has crept to, and
  // where the refresh band's top stands.
  choreograph::Output<float> scanCreep{0.0f};
  choreograph::Output<float> refreshSweep{0.0f};
  // The scanline strip: held here because its bake is its identity.
  Pattern scanlines;

  // The prompt's typing machine.
  enum class Prompt { Idle, Typing, Hold };
  Prompt prompt = Prompt::Idle;
  int commandIndex = 0;
  size_t shown = 0;
  double promptAt = 2.6;
  int burst = 0;

  double nextAppend = 0.0;
  double clockNow = 0.0;

  sk_sp<SkTypeface> faceMono, faceMonoMed, faceChrome, faceChromeMed;

  // The window full: seals, warnings and a breach on screen, a cipher still
  // churning, the refresh band mid-panel and the prompt mid-command.

  /** Mission clock: the timestamp voice every row and the header speak. */
  static double mission(double t) { return 412.0 + t; }

  /** Session health for the rail's hero stat, derived from the counters so
   *  it moves only when an append re-describes anyway. */
  double integrity() const {
    return std::max(92.0, 99.8 - 0.22 * gen.breaches + 0.04 * gen.seals);
  }

  /** The row voices, named once and resolved by name from every rich()
   *  span. Timestamps and tags are monospaced so the columns align by
   *  construction; the chrome (header, rail, counters) is proportional with
   *  tabular numerals asked of it where digits must sit in columns. */
  sigil::weave::StyleSet rowStyles() const {
    namespace dc = daemon_console;
    sigil::weave::StyleSet s(
        weave::textStyle({.face = faceMono, .size = 12.5f, .color = dc::kBody}));
    s.set("ts", weave::textStyle({.face = faceMono, .size = 11, .color = dc::kDim}));
    s.set("trace", weave::textStyle({.face = faceMono, .size = 12.5f, .color = dc::kDim}));
    s.set("seal",
          weave::textStyle({.face = faceMono, .size = 12.5f, .color = hex(0x8FE5C4)}));
    s.set("flux", weave::textStyle({.face = faceMono, .size = 12.5f, .color = dc::kWarn}));
    s.set("breach",
          weave::textStyle({.face = faceMono, .size = 12.5f, .color = dc::kCritText}));
    s.set("cipher",
          weave::textStyle({.face = faceMono, .size = 12.5f, .color = dc::kAccent}));
    auto tag = [&](SkColor4f color) {
      return weave::textStyle({.face = faceMonoMed, .size = 11, .color = color});
    };
    s.set("tag-trace", tag(alpha(dc::kDim, 0.8f)));
    s.set("tag-info", tag(dc::kChrome));
    s.set("tag-seal", tag(dc::kOk));
    s.set("tag-flux", tag(dc::kWarn));
    s.set("tag-breach", tag(dc::kCrit));
    return s;
  }

  /** A chrome style — the proportional voice of the enclosure. Tabular
   *  numerals so a ticking clock or a counter never jitters sideways. */
  sigil::weave::TextStyle chrome(float size, SkColor4f color, float track = 0,
                                 bool medium = false, bool tabular = false) {
    sigil::weave::TextStyle s =
        weave::textStyle({.face = medium ? faceChromeMed : faceChrome,
              .size = size,
              .color = color,
              .track = track});
    if (tabular)
      s.shaping.fontFeatures = {weave::features::tabularNumbers};
    return s;
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(9.0);
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    namespace dc = daemon_console;
    caretClock = 0.0f;
    lamp = 1.0f;
    prompt = Prompt::Idle;
    commandIndex = 0;
    shown = 0;
    promptAt = 2.6;
    burst = 0;
    nextAppend = 0.0;
    clockNow = 0.0;
    ring.clear();  // scenes re-activate; seq ids stay monotonic
    gen = dc::LogGen{};
    scanCreep = 0.0f;
    refreshSweep = 0.0f;
    scanlines = dc::scanlineTile();

    faceMono = weave::ports::pickTypeface({"SF Mono", "Menlo", "Monaco"}, 400);
    faceMonoMed = weave::ports::pickTypeface({"SF Mono", "Menlo", "Monaco"}, 700);
    faceChrome = weave::ports::pickTypeface({"Helvetica Neue", "Arial"}, 400);
    faceChromeMed = weave::ports::pickTypeface({"Helvetica Neue", "Arial"}, 600);

    for (int i = 0; i < 9; ++i)  // history at boot, timestamped in the past
      gen.emitRow(ring, mission(-4.5 + 0.5 * i));

    // The data-side drive. Appends and keystrokes re-render; reconciliation
    // prices each at the new row's mount plus the chrome leaves whose text
    // changed. Meters, lamp and caret ride bound outputs and never
    // re-describe anything.
    ticker.add([this, &composer, t = 0.0](double dt) mutable {
      t += dt;
      clockNow = t;
      meter[0] = 0.62f + 0.26f * (float)std::sin(t * 0.83 + 0.4);
      meter[1] = 0.48f + 0.30f * (float)std::sin(t * 1.31 + 2.1);
      meter[2] = 0.55f + 0.34f * (float)std::sin(t * 0.57 + 4.4) *
                             (float)std::sin(t * 1.9);
      meter[3] = 0.70f + 0.22f * (float)std::sin(t * 1.07 + 1.2);
      lamp = 0.55f + 0.45f * (float)std::sin(t * 2.4);
      // The tube: the scanlines creep, and the refresh band sweeps from
      // above the panel's top edge to below its foot and wraps.
      scanCreep = (float)(t * dc::kScanCreep);
      refreshSweep = (float)std::fmod(t * dc::kRefreshSpeed, dc::kRefreshWrap);
      // A caret blinks while the console waits and holds solid while it
      // types. The waveform lives on the caret's square() binding; what
      // the machine owns is the PHASE — parked at 0, the pulse's ON
      // instant, for as long as a command is typing.
      caretClock = prompt == Prompt::Typing ? 0.0f : (float)t;
      bool dirty = false;
      const char* command = daemon_console::kCommands[commandIndex];
      switch (prompt) {
        case Prompt::Idle:
          if (t >= promptAt) {
            prompt = Prompt::Typing;
            shown = 0;
            promptAt = t;
          }
          break;
        case Prompt::Typing:
          while (t >= promptAt && shown < std::string(command).size()) {
            ++shown;
            promptAt += 0.055;
            dirty = true;
          }
          if (shown >= std::string(command).size()) {
            prompt = Prompt::Hold;
            promptAt = t + 0.5;
          }
          break;
        case Prompt::Hold:
          if (t >= promptAt) {
            // The command lands in the log and answers arrive as a burst.
            ring.append({mission(t), daemon_console::kInfo, "EXEC",
                         std::string("$ ") + command});
            ++gen.events;
            burst = 2 + (int)(gen.rng() % 3);
            nextAppend = t + 0.10;
            prompt = Prompt::Idle;
            shown = 0;
            commandIndex = (commandIndex + 1) % daemon_console::kCommandCount;
            promptAt = t + 3.2 + (double)(gen.rng() % 200) / 100.0;
            dirty = true;
          }
          break;
      }
      if (t >= nextAppend) {
        nextAppend =
            t + (burst > 0 ? 0.09 : 0.14 + (double)(gen.rng() % 200) / 1000.0);
        if (burst > 0) --burst;
        gen.emitRow(ring, mission(t));
        dirty = true;
      }
      if (dirty) composer.render(describe());
      return true;
    });

    composer.render(describe());
  }

  /** One log row: the severity's stripe, then a single rich() leaf whose
   *  runs speak in the named voices — timestamp, channel tag, payload,
   *  cipher. Every entrance SETTLES: while a track runs the row paints
   *  live, and the frame it ends the row goes back to being a cached
   *  static leaf like every row above it. */
  Element logRow(const daemon_console::LogRow& r,
                 const sigil::weave::StyleSet& styles) const {
    namespace dc = daemon_console;
    const dc::SevDress& d = dc::dress(r.sev);

    auto line = rich(styles.base())
                    .styles(styles)
                    .add(toU8(std::format("{:07.2f}  ", r.t)), "ts")
                    .add(toU8(std::format("{:<6}", r.tag)), d.tagStyle)
                    .add(toU8(r.body), d.bodyStyle);
    if (!r.cipher.empty()) line.add(toU8("  " + r.cipher), "cipher");

    Element leaf = text(std::move(line));
    switch (r.sev) {
      case dc::kTrace:
        // A trace merely surfaces: one quiet fade, no cascade.
        leaf.fx({.effect = fx::keys({{0.0f, {.alpha = 0}}, {1.0f, {}}}),
                 .progress = animate(motion::from(0.0f).to(1.0f),
                                     {180ms, &choreograph::easeNone})});
        break;
      case dc::kFlux:
        // A warning rises glyph by glyph — more insistent than type-on,
        // still a sweep the eye can follow.
        leaf.fx({.effect = fx::rise(6),
                 .stagger = {.eachMs = 4, .durationMs = 120},
                 .progress = animate(motion::from(0.0f).to(1.0f),
                                     {300ms, &choreograph::easeNone})});
        break;
      case dc::kBreach:
        // A breach does not type: the whole line slams in at once, wide and
        // flat, FLASHES hot at impact, and settles to its own red as it
        // snaps to rest. The flash is a SCREEN term, not an add: the ink is
        // already at the red primary, so an added flash could only clip
        // that channel and shove the hue — where a screen lifts each
        // channel by its headroom, so the line blooms toward white and
        // decays back through its own colour, which is this console's
        // phosphor idiom (the glow underlays, the screen-blended scanline
        // pass) spoken per glyph.
        leaf.fx({.effect = fx::keys({{0.0f,
                                      {.alpha = 0,
                                       .colorScreen = {0.9f, 0.85f, 0.8f, 0},
                                       .scaleX = 1.45f,
                                       .scaleY = 0.62f}},
                                     {0.35f,
                                      {.colorScreen = {0.4f, 0.28f, 0.22f, 0},
                                       .scaleX = 0.97f}},
                                     {1.0f, {}}}),
                 .progress = animate(motion::from(0.0f).to(1.0f),
                                     {240ms, &choreograph::easeOutQuad})});
        break;
      default:
        // Info and seals type on — the terminal's own voice.
        leaf.fx({.effect = fx::typeOn(),
                 .stagger = {.eachMs = 6, .durationMs = 40},
                 .progress = animate(motion::from(0.0f).to(1.0f),
                                     {320ms, &choreograph::easeNone})});
        break;
    }
    if (!r.cipher.empty())
      // The cipher decodes on its own clock: held to NOTHING until each
      // glyph's beat opens (an unheld scramble would show wrong letters out
      // of turn), then hex churn, resolved by the end of the beat.
      leaf.fx(
          {.where = sel::style("cipher"),
           .effect = fx::hold(fx::scramble(U"0123456789abcdef", 10)),
           .stagger = {.eachMs = 30, .durationMs = 340},
           .over = unit::Cluster,
           .progress =
               animate(motion::from(0.0f).to(1.0f), {750ms, &choreograph::easeNone})});

    Element row = box()
                      .row()
                      .gap(8)
                      .padding(6, 1)
                      .corners({2})
                      .alignItems(Align::Center)
                      .child(box().width(3).height(12).corners({1.5f}).fill(
                          Fill::color(d.stripe)))
                      .child(std::move(leaf));
    // Severity in form as well as ink: a breach line carries its own wash.
    if (r.sev == dc::kBreach) row.fill(Fill::color(alpha(dc::kCrit, 0.09f)));
    return row;
  }

  /** A rail meter: a named channel and a bar riding a bound output —
   *  paint-only volatility over a cached bed. */
  Element meterRow(const char* label, choreograph::Output<float>* level) {
    namespace dc = daemon_console;
    return box()
        .column()
        .gap(4)
        .child(text(toU8(label), weave::textStyle({.face = faceMono,
                                       .size = 10,
                                       .color = dc::kChrome,
                                       .track = 0.6f})))
        .child(box()
                   .height(4)
                   .corners({2})
                   .clip()
                   .fill(Fill::color(dc::kMeterBed))
                   .child(box()
                              .inset(0)
                              .corners({2})
                              .fill(Fill::color(dc::kAccent))
                              .transformOrigin(0.0f, 0.5f)
                              .scaleX(level)));
  }

  Element counterRow(const char* label, SkColor4f chip, unsigned n) {
    namespace dc = daemon_console;
    return box()
        .row()
        .gap(8)
        .alignItems(Align::Center)
        .child(box().width(6).height(6).corners({1.5f}).fill(Fill::color(chip)))
        .child(text(toU8(label), chrome(10, dc::kChrome, 1.6f)))
        .child(box().grow(1))
        .child(text(toU8(std::format("{}", n)),
                    chrome(12, dc::kBone, 0, true, true)));
  }

  Element rule(float marginTop, float marginBottom) {
    return box()
        .height(1)
        .margin(0, marginTop, 0, marginBottom)
        .fill(Fill::color(daemon_console::kRule));
  }

  Element describe() {
    namespace dc = daemon_console;
    namespace feed = sigil::compose::feed;

    // Panel chrome: one-pass SDF (fill + border + glow), cached between
    // layouts. The style reserves its glow's reach INSIDE the box, so the
    // drawn border sits sdf::pad() in from the node edge — the content
    // padding is that reserve plus the designed inset, read off the style
    // rather than restated as a number that drifts.
    const sdf::Style panelStyle{.fill = mskia::toColor(dc::kPanel),
                                .borderWidth = 1.0f,
                                .borderColor = mskia::toColor(hex(0x3B5474, 0.95f)),
                                .glowRadius = 6,
                                .glowColor = mskia::toColor(hex(0x3EC2DC, 0.22f))};
    Paint panel = Paint::recipe(sdf::material(sdf::roundBox(12), panelStyle));
    const float padX = sdf::pad(panelStyle) + 17.0f;
    const float padY = sdf::pad(panelStyle) + 12.0f;

    // Fade the OLDEST rows: a panel-coloured gradient over the top of the
    // well — zero row nodes touched, fully cached.
    Paint fade = Paint::linear(
        {0, 0}, {0, 64},
        {{0.0f, {dc::kPanel.fR, dc::kPanel.fG, dc::kPanel.fB, 1.0f}},
         {1.0f, {dc::kPanel.fR, dc::kPanel.fG, dc::kPanel.fB, 0.0f}}});

    feed::Options window;
    window.visible = 22;
    window.gap = 3;
    // The boot history cascades in; each live append is the only new mount
    // in its patch, so it enters the instant it arrives.
    window.entrance = {.eachMs = 26};

    // Built once per describe; the rows compare it by value, so identical
    // styles prune and only genuinely new rows mount.
    const sigil::weave::StyleSet styles = rowStyles();
    Element well =
        box()
            .grow(1)
            .clip()
            .child(feed::feed(
                       ring, window,
                       [&](const dc::LogRow& r) { return logRow(r, styles); })
                       .zIndex(1))
            .child(box().inset(0).fill(fade).zIndex(2).hitTestable(false));

    // ---- header band ------------------------------------------------------
    Element header =
        box()
            .row()
            .gap(10)
            .alignItems(Align::Center)
            .child(box().width(9).height(9).corners({2}).rotate(45.0f).fill(
                Fill::color(dc::kAccent)))
            .child(text(toU8("WARDNET"), chrome(15, dc::kBone, 3.5f, true)))
            .child(
                text(toU8("PERIMETER WATCH"), chrome(10.5f, dc::kChrome, 3.5f)))
            .child(box().grow(1))
            .child(text(toU8("NODE 07 \xc2\xb7 flooded-causeway"),
                        chrome(10.5f, dc::kDim, 0.8f)))
            .child(box()
                       .width(6)
                       .height(6)
                       .corners({3})
                       .fill(Fill::color(dc::kOk))
                       .opacity(&lamp))
            .child(text(toU8(std::format("T+{:07.2f}", mission(clockNow))),
                        chrome(11.5f, dc::kAccent, 0.6f, true, true)));

    // ---- rail -------------------------------------------------------------
    const sigil::weave::TextStyle label = chrome(9.5f, dc::kDim, 2.4f, true);
    Element rail =
        box()
            .column()
            .width(172)
            .gap(9)
            .child(text(toU8("CHANNELS"), label))
            .child(meterRow("LATT", &meter[0]))
            .child(meterRow("GATE", &meter[1]))
            .child(meterRow("FLUX", &meter[2]))
            .child(meterRow("AUTH", &meter[3]))
            .child(rule(6, 2))
            .child(text(toU8("SEVERITY \xc2\xb7 SESSION"), label))
            .child(counterRow("SEALS", dc::kOk, gen.seals))
            .child(counterRow("FLUX WARNS", dc::kWarn, gen.warns))
            .child(counterRow("BREACHES", dc::kCrit, gen.breaches))
            .child(rule(6, 2))
            .child(text(toU8("UPLINK"), label))
            .child(
                box()
                    .row()
                    .gap(8)
                    .alignItems(Align::Center)
                    .child(text(toU8("latency"), chrome(10, dc::kChrome, 0.8f)))
                    .child(box().grow(1))
                    .child(text(toU8(std::format(
                                    "{:2.0f} mS",
                                    11.0 + 3.0 * std::sin(clockNow * 0.7))),
                                chrome(11, dc::kBone, 0, true, true))))
            // The hero stat anchors the rail's foot: session health as one
            // number, amber the moment the breach count says it should be.
            .child(box().grow(1))
            .child(rule(6, 2))
            .child(text(toU8("WARD INTEGRITY"), label))
            .child(
                box()
                    .row()
                    .gap(4)
                    .alignItems(Align::Baseline)
                    .child(text(
                        toU8(std::format("{:.1f}", integrity())),
                        chrome(24, integrity() >= 96.0 ? dc::kBone : dc::kWarn,
                               0, true, true)))
                    .child(text(toU8("%"), chrome(12, dc::kChrome))))
            .child(
                text(toU8(std::format("{} breach{} this session", gen.breaches,
                                      gen.breaches == 1 ? "" : "es")),
                     chrome(9.5f, dc::kDim, 0.8f, false, true)));

    // ---- prompt -----------------------------------------------------------
    const char* command = dc::kCommands[commandIndex];
    Element promptLine =
        box()
            .row()
            .gap(2)
            .alignItems(Align::Center)
            .child(text(
                rich(weave::textStyle({.face = faceMono, .size = 12, .color = dc::kDim}))
                    .add(toU8("wardnet"))
                    .add(toU8(" $ "), weave::textStyle({.face = faceMonoMed,
                                            .size = 12,
                                            .color = dc::kAccent}))))
            .child(text(
                toU8(std::string(command).substr(0, shown)),
                weave::textStyle({.face = faceMono, .size = 12.5f, .color = dc::kBone})))
            .child(box()
                       .width(7)
                       .height(13)
                       .margin(3, 0, 0, 0)
                       .fill(Fill::color(dc::kAccent))
                       // The blink is the pulse waveform itself: on for
                       // 0.62 s of every 1.06 s cycle, resting dim rather
                       // than vanishing. Phase 0 is ON, so the caret the
                       // typing machine parks at 0 sits solid.
                       .opacity(motion::bind(&caretClock)
                                    .source(0.0f, 1.06f)
                                    .square(0.62f / 1.06f)
                                    .target(0.10f, 1.0f))
                       .key("caret"))
            .child(box().grow(1))
            .child(text(toU8(std::format("ring 256 \xc2\xb7 {} events",
                                         (unsigned long long)gen.events)),
                        chrome(9.5f, dc::kDim, 0.8f, false, true)));

    return stack()
        .fill(Paint::linear({0, 0}, {0, dc::kH},
                               {{0.0f, dc::kGroundTop}, {1.0f, dc::kVoid}}))
        .child(
            box()
                .column()
                .inset(26, 22, 26, 22)
                .fill(panel)
                .clip()
                .padding(padX, padY)
                .child(header)
                .child(rule(9, 8))
                .child(box()
                           .row()
                           .grow(1)
                           .gap(16)
                           .clip()
                           .child(std::move(well))
                           .child(box().width(1).fill(Fill::color(dc::kRule)))
                           .child(std::move(rail)))
                .child(rule(8, 7))
                .child(promptLine))
        // the living surface: the scanline tile, crept by its bound pan
        .child(box()
                   .inset(0)
                   .zIndex(3)
                   .hitTestable(false)
                   .fill(Pattern(scanlines)
                             .offset(std::nullopt, &scanCreep)
                             .material())
                   .blend(SkBlendMode::kScreen))
        // …and the refresh band, baked once and slid down the panel. Its
        // rest position puts the tent's centre 90 px above the top edge,
        // so the sweep enters from above and leaves below the foot.
        .child(box()
                   .left(0)
                   .top(-90.0f - dc::kRefreshH * 0.5f)
                   .width(dc::kW)
                   .height(dc::kRefreshH)
                   .zIndex(4)
                   .hitTestable(false)
                   .fill(dc::refreshBand())
                   .translateY(&refreshSweep)
                   .cache(Cache::Texture)
                   .blend(SkBlendMode::kScreen));
  }
};

}  // namespace

SIGIL_SKETCH_AS(DaemonConsole, "daemon console", "Catalog \xc2\xb7 Game UI",
                "feed::Ring<LogRow>, with an entrance per severity")
