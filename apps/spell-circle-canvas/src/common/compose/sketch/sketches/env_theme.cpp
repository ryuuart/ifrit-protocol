// env_theme.cpp — ONE API: env::Provide<T> / env::inherited<T>().
// =============================================================================
// The point is a value read where a component is COMPOSED, not where it is
// written. `chip()` below takes NO arguments and is four calls down from the
// nearest `Provide`; `feed::feed(ring)` — a library component nobody here
// wrote — is themed by the same channel, keyed on its own props type.
//
// Three columns, one component tree, drawn three times:
//   NO BINDING   inherited<Palette>() is null, so chip() uses its own
//                default — exactly a React context's default value.
//   OUTER        one Provide<Palette> + one Provide<feed::TextOptions> at the
//                top of the column. Nothing below is handed either.
//   SHADOWED     the same column, with an INNER Provide<Palette> around
//                only the bottom half. LIFO: the inner one wins there and
//                the outer one is back in scope after it.
//
// EDIT THESE FIRST
//   kOuter / kInner (below) — the two Palettes. Change a colour and watch
//                            which chips move: only the ones under that
//                            scope, because a read lands in the reading
//                            node's OWN props and propsEqual is already
//                            the dependency tracker.
//   kLevels                 — how deep the handed-nothing chain runs.
//
// The three ways things move (hello.cpp): none of them. env:: is a
// DESCRIBE-path channel — a theme change re-describes and the reconciler
// patches the nodes whose props moved. Bind the one property that scrubs
// at 60 Hz, never the theme.

#include <sigilcompose/Feed.h>
#include <sigilsketch/Sketch.h>

#include <string>

using namespace sigil::compose;

namespace {

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

const Palette kOuter{"outer",
                     {0.09f, 0.20f, 0.26f, 1},
                     {0.80f, 0.94f, 0.98f, 1},
                     {0.30f, 0.85f, 0.82f, 1}};
const Palette kInner{"inner (shadowing)",
                     {0.26f, 0.16f, 0.06f, 1},
                     {0.99f, 0.92f, 0.78f, 1},
                     {1.00f, 0.70f, 0.24f, 1}};

constexpr int kLevels = 4;  // containers between Provide and the read

sigil::weave::TextStyle type(float size, SkColor4f color) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = size;
  style.paint.foreground.setColor4f(color, nullptr);
  style.paint.foreground.setAntiAlias(true);
  return style;
}

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};

// -------------------------------------------------------------- the consumer
// THE WHOLE POINT: handed nothing, reads the ambient binding, states its own
// default when there is none.

Element chip(const char* label) {
  const Palette c = env::inheritedOr(Palette{});
  return box()
      .width(64)
      .height(34)
      .corners({6})
      .fill(Fill::color(c.surface))
      .stroke(stroke(1.2f, Fill::color(c.accent)))
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(text(toU8(label), type(11, c.ink)));
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
      type(11, have ? c.accent : kDim));
}

// -------------------------------------------------------------- the library
// feed::TextOptions is the worked consumer: it is already a comparable
// props type, so it is already an env key. `feed::feed(ring)` with no
// options argument reads whatever is in scope.

feed::TextOptions feedOptions(const Palette& c) {
  feed::TextOptions options;
  options.styles.base(type(11, c.ink))
      .set("accent", type(11, c.accent))
      .set("dim", type(11, kDim));
  options.window.gap = 3;
  options.window.visible = 5;
  return options;
}

Element panelColumn(const char* heading, const char* note, Element body) {
  return box()
      .width(340)
      .column()
      .gap(10)
      .child(text(toU8(heading), type(14, kInk)))
      .child(text(toU8(note), type(11, kDim)))
      .child(std::move(body));
}

/** The body every column shares — same code, three environments. */
Element themedBody(const feed::TextRing& ring) {
  return box()
      .width(340)
      .column()
      .gap(10)
      .padding(12)
      .stroke(stroke(1.0f, Fill::color(kFrame)))
      .child(handedNothing(kLevels))
      .child(boundLine())
      .child(text(toU8("feed::feed(ring) \xc2\xb7 no options argument"),
                  type(10, kDim)))
      .child(feed::feed(ring));
}

}  // namespace

struct EnvTheme : sigil::compose::sketch::Sketch {
  feed::TextRing ring{16};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1140, 470);
    ctx.background({0.055f, 0.06f, 0.085f, 1});

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
          .width(340)
          .column()
          .gap(10)
          .padding(12)
          .stroke(stroke(1.0f, Fill::color(kFrame)))
          .child(std::move(top))
          .child(std::move(inner))
          .child(
              text(toU8("…and back OUT of the inner scope:"), type(10, kDim)))
          .child(handedNothing(kLevels))
          .child(feed::feed(ring));
    }();

    ctx.composer.render(
        stack()
            .child(text(toU8("env::Provide<T> / env::inherited<T>() \xc2\xb7 "
                             "read where a component is COMPOSED"),
                        type(15, kInk))
                       .left(30)
                       .top(16))
            .child(
                box()
                    .row()
                    .left(30)
                    .top(52)
                    .gap(24)
                    .child(panelColumn("NO BINDING",
                                       "inheritedOr() default \xe2\x80\x94 the "
                                       "feed's own, at its own size",
                                       std::move(plain)))
                    .child(panelColumn("OUTER SCOPE",
                                       "one Provide, four levels up",
                                       std::move(outer)))
                    .child(panelColumn("SHADOWED",
                                       "an inner Provide over the middle band",
                                       std::move(shadowed))))
            .child(text(toU8("bindings are keyed by C++ TYPE \xc2\xb7 there "
                             "is no library-wide Theme \xc2\xb7 a callable "
                             "the KERNEL invokes sees no scope"),
                        type(11, kDim))
                       .left(30)
                       .bottom(14)));
  }
};

SIGIL_SKETCH(EnvTheme)
