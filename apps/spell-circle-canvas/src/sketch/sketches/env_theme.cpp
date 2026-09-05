/** @file
 * env_theme — `env::Provide<T>` / `env::inherited<T>()`.
 *
 * The point is a value read where a component is COMPOSED, not where it
 * is written. `chip()` below takes NO arguments and is four calls down
 * from the nearest `Provide`; `feed::feed(ring)` — a library component
 * nobody here wrote — is themed by the same channel, keyed on its own
 * props type.
 *
 * Three columns, one component tree, drawn three times:
 *   NO BINDING   inherited<Palette>() is null, so chip() uses its own
 *                default — exactly a React context's default value.
 *   OUTER        one Provide<Palette> + one Provide<feed::TextOptions> at
 *                the top of the column. Nothing below is handed either.
 *   SHADOWED     the same column, with an INNER Provide<Palette> around
 *                only the middle band. LIFO: the inner one wins there and
 *                the outer one is back in scope after it.
 *
 * EDIT THESE FIRST
 *   kOuter / kInner — the two Palettes. Change a colour and watch which
 *                chips move: only the ones under that scope, because a
 *                read lands in the reading node's OWN props and
 *                propsEqual is already the dependency tracker.
 *   kLevels     — how deep the handed-nothing chain runs.
 *
 * The three ways things move: none of them. env:: is a DESCRIBE-path
 * channel — a theme change re-describes and the reconciler patches the
 * nodes whose props moved. Bind the one property that scrubs at 60 Hz,
 * never the theme.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/reconcile/Env.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace env = sigil::core::env;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {
/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.ash = {0.55f, 0.60f, 0.70f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.title = {.size = 15, .track = 2};
  look.type.subtitle = {.size = 11, .track = 0.6f};
  look.type.footer = {.size = 10.5f, .track = 0.2f};
  look.type.captionLabel = {.size = 14, .track = 0.5f};
  look.type.captionNote = {.size = 11, .track = 0.2f};
  look.captionWhere = kit::Caption::Where::Above;
  look.spacing.marginX = 30;
  look.spacing.marginTop = 22;
  look.spacing.captionGap = 10;
  return look;
}

/** An inherited type is a comparable VALUE. Structural and exact — that is
 *  what makes `propsEqual` the dependency tracker. No std::function lives
 *  in here: a member that cannot compare equal to itself would make every
 *  memo below a permanent miss, because the props of every reading node
 *  would look changed on every describe. Derivations are RUN and STORED. */
struct Palette {
  std::string name = "chip()'s own default";
  SkColor4f surface{0.16f, 0.17f, 0.21f, 1};
  SkColor4f ink{0.62f, 0.66f, 0.74f, 1};
  SkColor4f accent{0.45f, 0.48f, 0.56f, 1};
  bool operator==(const Palette&) const = default;
};

// NOLINTBEGIN(bugprone-throwing-static-initialization): literal tables; only
// allocation could throw
const Palette kOuter{"outer",
                     {0.09f, 0.20f, 0.26f, 1},
                     {0.80f, 0.94f, 0.98f, 1},
                     {0.30f, 0.85f, 0.82f, 1}};
const Palette kInner{"inner (shadowing)",
                     {0.26f, 0.16f, 0.06f, 1},
                     {0.99f, 0.92f, 0.78f, 1},
                     {1.00f, 0.70f, 0.24f, 1}};
// NOLINTEND(bugprone-throwing-static-initialization)

constexpr int kLevels = 4;  // containers between Provide and the read

constexpr SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
constexpr SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};
constexpr float kColumn = 340.0f;

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

// -------------------------------------------------------------- the consumer
// THE WHOLE POINT: handed nothing, reads the ambient binding, states its own
// default when there is none.

Element chip(const char* caption) {
  const Palette c = env::inheritedOr(Palette{});
  return box()
      .width(64)
      .height(34)
      .corners({6})
      .fill(Fill::color(c.surface))
      .stroke(stroke(1.2f, Fill::color(c.accent)))
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(text(toU8(caption), label(11, c.ink)));
}

/** `kLevels` plain containers, none of which knows a Palette exists. */
Element handedNothing(int depth) {
  if (depth == 0)
    return box()
        .row()
        .gap(8)
        .child(chip("read"))
        .child(chip("here"))
        .child(chip("only"));
  return box().padding(3).child(handedNothing(depth - 1));
}

/** A named-binding question a component can ask instead of falling back. */
Element boundLine() {
  const bool have = env::bound<Palette>();
  const Palette c = env::inheritedOr(Palette{});
  return text(
      toU8(have ? std::string("env::bound<Palette>() true \xc2\xb7 ") + c.name
                : std::string("env::bound<Palette>() FALSE")),
      label(11, have ? c.accent : kDim));
}

// -------------------------------------------------------------- the library
// feed::TextOptions is the worked consumer: it is already a comparable
// props type, so it is already an env key. `feed::feed(ring)` with no
// options argument reads whatever is in scope.

feed::TextOptions feedOptions(const Palette& c) {
  feed::TextOptions options;
  options.styles.base(label(11, c.ink))
      .set("accent", label(11, c.accent))
      .set("dim", label(11, kDim));
  options.window.gap = 3;
  options.window.visible = 5;
  return options;
}

Element panelColumn(const char* heading, const char* note, Element body) {
  return sketch::kit::caption(kColumn, toU8(heading), toU8(note),
                              std::move(body));
}

/** The body every column shares — same code, three environments. */
Element themedBody(const feed::TextRing& ring) {
  return box()
      .width(kColumn)
      .column()
      .gap(10)
      .padding(12)
      .stroke(stroke(1.0f, Fill::color(kFrame)))
      .child(handedNothing(kLevels))
      .child(boundLine())
      .child(text(toU8("feed::feed(ring) \xc2\xb7 no options argument"),
                  label(10, kDim)))
      .child(feed::feed(ring));
}

}  // namespace

struct EnvTheme : sketch::Sketch {
  feed::TextRing ring{16};

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1140, 520}});
    // Nothing moves: env is a describe-path channel.
    ctx.captureAt(0.05);

    ring.clear();
    ring.append({u8"describe: env stack pushed", "accent"});
    ring.append({u8"chip() read the nearest binding"});
    ring.append({u8"patchedNodes == 1 on a colour change", "dim"});
    ring.append({u8"nothing was threaded through"});

    // NO BINDING — no Provide anywhere. Every read returns its default.
    Element plain = themedBody(ring);

    // OUTER — one Provide, at the top. `plain`'s tree was already built
    // above and is unaffected: reads happen during DESCRIBE.
    Element outer = [&] {
      env::Provide<Palette> palette(kOuter);
      env::Provide<feed::TextOptions> style(feedOptions(kOuter));
      return themedBody(ring);
    }();

    // SHADOWED — the same column, with an inner Provide around only the
    // second half. LIFO and per-TYPE: the inner Palette wins below it, the
    // feed::TextOptions binding is untouched by it, and after the inner scope
    // ends the outer Palette is back.
    Element shadowed = [&] {
      env::Provide<Palette> palette(kOuter);
      env::Provide<feed::TextOptions> style(feedOptions(kOuter));
      Element top = handedNothing(kLevels);
      Element inner = [&] {
        env::Provide<Palette> nested(kInner);
        return box()
            .column()
            .gap(8)
            .child(handedNothing(kLevels))
            .child(boundLine());
      }();
      return box()
          .width(kColumn)
          .column()
          .gap(10)
          .padding(12)
          .stroke(stroke(1.0f, Fill::color(kFrame)))
          .child(std::move(top))
          .child(std::move(inner))
          .child(text(toU8("\xe2\x80\xa6"
                           "and back OUT of the inner scope:"),
                      label(10, kDim)))
          .child(handedNothing(kLevels))
          .child(feed::feed(ring));
    }();

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("ENV \xc2\xb7 env::Provide<T> / "
                       "env::inherited<T>()"),
         .subtitle = toU8("one component tree, three environments "
                          "\xe2\x80\x94 read where a component is "
                          "COMPOSED, not where it is written"),
         .footer = toU8("bindings are keyed by C++ TYPE \xc2\xb7 there "
                        "is no library-wide Theme \xc2\xb7 a callable "
                        "the KERNEL invokes sees no scope")},
        kit::cells(
            {.cells = {panelColumn("NO BINDING",
                                   "inheritedOr() default \xe2\x80\x94 the "
                                   "feed's own, at its own size",
                                   std::move(plain)),
                       panelColumn("OUTER SCOPE", "one Provide, four levels up",
                                   std::move(outer)),
                       panelColumn("SHADOWED",
                                   "an inner Provide over the middle band",
                                   std::move(shadowed))},
             .gap = 24})));
  }
};

SIGIL_SKETCH(
    EnvTheme, "Kit \xc2\xb7 API",
    "env::Provide / inherited \xe2\x80\x94 one tree, three environments; "
    "feed::TextOptions is the worked library consumer")
