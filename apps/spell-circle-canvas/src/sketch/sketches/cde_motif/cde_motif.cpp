// A CDE desktop whose bevels and control colours are derived from its palette.

// TAGS: Interfaces/Desktop

#include "Motif.h"

struct CdeMotifSketch : sketch::Sketch {
  using Set = cde::ColorSet;

  /** The one 2x2 tile every insensitive label on the desktop is painted
   *  through, cut once and bound over the whole description. */
  cde::Theme theme;
  int paletteIndex = 0;
  double nextSwitch = 0.0;

  // The one thing on this canvas allowed to move smoothly, because it is
  // showing a FUNCTION rather than a desktop.
  ch::Output<float> sweep{0.0f};     // 0..1, the grey ramp under the strip
  ch::Output<float> clockT{0.42f};   // fraction of an hour; the minute hand
  ch::Output<float> busy{0.0f};      // the TYPE busy blinker, ~2 Hz
  ch::Output<float> caret{0.0f};     // the text field's I-beam, ~1 Hz
  ch::Output<float> subpanel{0.0f};  // the Text Editor subpanel's wipe

  std::array<Pattern, 4> backdrops;
  bool backdropsBuilt = false;

  // -------------------------------------------------------------------------

  /** THE VERIFICATION, as claims rather than as printed lines.
   *
   *  Four bytes sampled off a photograph of a running CDE desktop, plus
   *  the one branch that is easy to get backwards. The verdict is never
   *  written by hand — `check()` computes it from the two values — and
   *  the table is DRAWN, on the derivation strip, because a study whose
   *  subject is an algorithm should carry its own proof in the picture
   *  rather than in a console nobody captures. */
  static measure::Table derivation() {
    struct Case {
      const char* what;
      uint32_t bg, ts, bs;
    };
    const Case cases[] = {
        {"Front Panel", 0xAEB2C3, 0xDCDEE5, 0x5D6069},
        {"switch btn", 0x63639C, 0xB7B7D1, 0x2F2F4A},
    };
    auto hexOf = [](cde::Rgb c) {
      return kit::formatted("#%06X", cde::toHex(c));
    };
    measure::Table t;
    for (const Case& c : cases) {
      const cde::Derived d = cde::calculate(cde::from8(c.bg));
      t.add(measure::check(std::string(c.what) + " ts",
                           kit::formatted("#%06X", c.ts), hexOf(d.ts)));
      t.add(measure::check(std::string(c.what) + " bs",
                           kit::formatted("#%06X", c.bs), hexOf(d.bs)));
    }
    // The five LITE colours CDE ships are all colour-set 4, and the top
    // shadow comes out DARKER than the background on every one — which is
    // the branch a reimplementation gets backwards.
    const cde::Derived lite = cde::calculate(cde::from8(0xFFF7E9));
    t.add(
        measure::check("#FFF7E9 takes LITE", lite.branch == cde::Branch::Lite));
    t.add(measure::check("LITE ts darker than bg", lite.ts.r < lite.bg.r));
    return t;
  }

  // -------------------------------------------------------------------------
  // Window chrome.

  /** The 6 px Motif window frame [MEAS]: a raised outer bevel of 2, a 3 px
   *  band, and a 1 px INNER bevel that is inverted — you are looking at
   *  the inside face of a ridge. The inner one goes through
   *  inset(5, …), which is that helper's exact use case ("the
   *  same bevel again, five pixels in"). */
  Element windowFrame(Element content) {
    const Set s = cde::ambient();
    return box()
        .fill(s.bg)
        .overlay(cde::bevel(2, false, false))
        .overlay(inset(5, cde::bevel(1, true, false)))
        .padding(6)
        .column()
        .children({std::move(content)});
  }

  /** The title bar: 23 px, shadowThickness 1, with the window-menu button
   *  at the left and minimise/maximise at the right [MEAS]. The label is
   *  centred in the derived foreground — white on both #B24D7A and
   *  #EDA870, because both sets fall under the 70% threshold. */
  Element titleBar(std::string_view t, bool active) {
    const Set s = cde::ambient();
    auto furniture = [&](Element glyph) {
      return box()
          .width(20)
          .height(19)
          .fill(s.bg)
          .overlay(cde::bevel(1, false, false))
          .alignItems(Align::Center)
          .justify(Justify::Center)
          .children({std::move(glyph)});
    };
    Element menuGlyph =
        box().width(12).height(4).fill(s.fg);
    Element minGlyph =
        box().width(5).height(5).fill(s.fg);
    Element maxGlyph = box()
                           .width(11)
                           .height(11)
                           .overlay(cde::bevelFg(1));
    return cde::surface(s)
        .height(23)
        .overlay(cde::bevel(1, false, false))
        .row()
        .alignItems(Align::Center)
        .padding(2, 1)
        .children({furniture(std::move(menuGlyph)),
                   box()
                       .grow(1)
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .children({cde::label(t)}),
                   furniture(std::move(minGlyph)), box().width(2),
                   furniture(std::move(maxGlyph))});
  }

  /** The 31 px menu bar [MEAS], in colour set 6, with Motif's mnemonic
   *  underline on exactly one character of every label. */
  Element menuBar(const std::vector<std::string>& items, int rightFrom) {
    const Set s = cde::ambient();
    Element bar = cde::surface(s)
                      .height(31)
                      .overlay(cde::bevel(1, false, false))
                      .row()
                      .alignItems(Align::Center)
                      .padding(10, 1);
    for (int i = 0; i < (int)items.size(); ++i) {
      if (i == rightFrom) bar.children({box().grow(1)});
      bar.children({box().padding(8, 4).children(
          {cde::mnemonicLabel(items[(size_t)i], s.fg, 0)})});
    }
    return bar;
  }

  // -------------------------------------------------------------------------
  // The File Manager. Frame and title in set 1, menu bar and client area
  // in set 5, the path field in set 4 and the scrollbar in set 3 — four
  // scopes opened around four subtrees, and no widget below them told
  // which it is in.

  Element fileManager() {
    // THE WINDOW'S COLOUR SETS, and the scope each one covers. The frame
    // and the title bar are set 1; the menu bar and the client area are
    // ONE set together, which is what the reference shows — a CDE window
    // is chrome plus a body, not three bands of three colours. Only the
    // path field is its own (set 4 is the near-white text set, and the
    // only one that ever takes the LITE branch), and the scrollbar is
    // set 3 because a scrollbar is dtwm's, not the application's.
    environment::Provide<cde::ColorSet> chrome(theme[1]);
    Element title = titleBar("File Manager - user", true);

    Element window;
    {
      environment::Provide<cde::ColorSet> body(theme[5]);
      const Set& c5 = cde::ambient();

      // The reference holds 31 folders in a pane this size. They flow
      // and wrap, the way an XmContainer lays icons out, rather than
      // sitting in a hand-counted grid.
      static const char* kNames[31] = {
          "bin",    "boot",     "cdrom",  "dev",   "devices",    "etc",
          "export", "home",     "kernel", "lib",   "mnt",        "net",
          "opt",    "platform", "proc",   "sbin",  "system",     "tmp",
          "usr",    "var",      "vol",    "xfn",   "lost+found", "core",
          "mail",   "news",     "share",  "spool", "local",      "adm",
          "log"};

      Element grid = box().row().wrapLines().gap(4).padding(8, 8);
      for (const char* name : kNames)
        grid.children({box()
                           .width(70)
                           .column()
                           .alignItems(Align::Center)
                           .gap(2)
                           .children({cde::art(cde::icoFolder(), 2.0f)})
                           .children({cde::label(name)})});

      // The scrollbar: a sunken trough with a raised slider, and a
      // STEPPER at each end — CDE puts an arrow box top and bottom, and
      // without them the trough reads as a plain groove. The slider's
      // length is the ONE number here measured off the screenshot rather
      // than read off what the pane holds: the reference shows a file
      // view whose icon count nothing in this sketch stands for.
      Element scrollbar;
      {
        environment::Provide<cde::ColorSet> bar(theme[3]);
        const Set& c3 = cde::ambient();
        auto stepper = [&](bool up) {
          return box()
              .width(15)
              .height(15)
              .shrink(0)
              .fill(c3.bg)
              .overlay(cde::bevel(2, false, false))
              .alignItems(Align::Center)
              .justify(Justify::Center)
              .children({box()
                             .width(9)
                             .height(7)
                             .shape(shapes::polygon(3, up ? 0.0f : 180.0f))
                             .fill(c3.fg)});
        };
        scrollbar = sketch::kit::scrollbar({.leading = stepper(true),
                                            .trailing = stepper(false),
                                            .thumb = box().fill(c3.bg).overlay(
                                                cde::bevel(2, false, false)),
                                            .thumbLength = Dimension(150),
                                            .track = Fill::none()})
                        .width(19)
                        .padding(2)
                        .gap(2)
                        .fill(c3.bg)
                        .overlay(cde::bevel(2, true, false));
      }

      Element pathRow = box()
                            .row()
                            .alignItems(Align::Center)
                            .padding(8, 6)
                            .gap(8)
                            .children({cde::label("Path:")});
      {
        environment::Provide<cde::ColorSet> field(theme[4]);
        pathRow.children(
            {cde::textField("/export/home/user", 420, true, &caret)});
      }

      Element client =
          cde::surface(c5)
              .grow(1)
              .column()
              .children({std::move(pathRow)})
              // The icon pane is an XmScrolledWindow: XmSHADOW_IN at
              // T = 2, which is why a CDE file view reads as a well and
              // not as a sheet of colour.
              .children({box()
                             .grow(1)
                             .margin(6, 0, 6, 0)
                             .row()
                             .overlay(cde::bevel(2, true, false))
                             .padding(2)
                             .children({box().grow(1).clip().children(
                                 {std::move(grid)})})
                             .children({std::move(scrollbar)}),
                         box()
                             .height(2)
                             .margin(2, 3)
                             .overlay(cde::bevel(2, true, true)),
                         box()
                             .height(22)
                             .row()
                             .alignItems(Align::Center)
                             .padding(8, 2)
                             .children({cde::label("31 Items  11 Hidden")})});

      window = box().grow(1).column().children(
          {std::move(title), menuBar({"File", "Selected", "View", "Help"}, 3),
           std::move(client)});
    }
    return windowFrame(std::move(window));
  }

  // -------------------------------------------------------------------------
  // The Style Manager's Color dialog. The self-documenting piece: the
  // eight numbers of the theme, on screen, in the theme.

  Element colorDialog() {
    const Set& c1 = theme[1];
    const Set& c2 = theme[2];  // dtsession's primary set — unstyled widgets
    const Set& c6 = theme[6];  // list panes

    Element list = cde::surface(c6)
                       .overlay(cde::bevel(2, true, false))
                       .padding(2)
                       .grow(1)
                       .column();
    for (int i = 0; i < (int)cde::kPalettes.size(); ++i) {
      const bool current = i == paletteIndex;
      Element rowBox =
          box()
              .row()
              .alignItems(Align::Center)
              .height(20)
              .padding(6, 0)
              .children({cde::label(cde::kPalettes[(size_t)i]->name)});
      if (current) rowBox.fill(c6.sel);
      list.children({std::move(rowBox)});
    }
    for (const char* n : {"Cabernet", "Charcoal", "Delphinium", "Desert",
                          "GrayScale", "Lilac", "Neptune", "Olive"})
      list.children({box()
                         .row()
                         .alignItems(Align::Center)
                         .height(20)
                         .padding(6, 0)
                         .children({cde::label(n)})});

    // XmScrollBar: a sunken trough in the workspace set with a raised
    // slider, 15 px of trough plus 2 px of shadow either side [MEAS].
    auto scrollBar = [&](const Set& t, float sliderFrac) {
      return sketch::kit::scrollbar({.thumb = box().fill(t.bg).overlay(
                                         cde::bevel(2, false, false)),
                                     .thumbLength = pct(sliderFrac),
                                     .track = Fill::none()})
          .width(19)
          .fill(t.bg)
          .overlay(cde::bevel(2, true, false))
          .padding(2);
    };

    Element listPane =
        box()
            .row()
            .width(170)
            .children({std::move(list), scrollBar(theme[3], 34)});

    // The eight colour-set swatches, 4 x 2. This IS the palette file.
    Element swatches = box().column().gap(6);
    for (int r = 0; r < 2; ++r) {
      Element rr = box().row().gap(6);
      for (int c = 0; c < 4; ++c) {
        const int idx = r * 4 + c + 1;
        rr.children({box()
                         .width(50)
                         .height(42)
                         .fill(theme[idx].bg)
                         .overlay(cde::bevel(2, false, false))});
      }
      swatches.children({std::move(rr)});
    }

    Element buttons =
        box()
            .row()
            .gap(10)
            .justify(Justify::SpaceBetween)
            .children({cde::pushButton("OK", false, true),
                       cde::pushButton("Add..."),
                       cde::pushButton("Delete", false, false, true),
                       cde::pushButton("Help")});

    Element body =
        cde::surface(c2).grow(1).column().padding(10).gap(10).children(
            {box()
                 .row()
                 .gap(12)
                 .grow(1)
                 .children({box()
                                .column()
                                .gap(4)
                                .children({cde::label("Palettes")})
                                .children({std::move(listPane)})})
                 .children(
                     {box()
                          .column()
                          .gap(4)
                          .children({cde::label("Color Sets")})
                          .children({std::move(swatches)})
                          .children({box().height(6)})
                          .children({cde::label("Number of Colors:")})
                          .children({cde::label("  High Color  (8 sets)")})}),
             box().height(2).overlay(cde::bevel(2, false, true)),
             std::move(buttons)});

    return windowFrame(box().grow(1).column().children(
        {titleBar("Style Manager - Color", false), std::move(body)}));
  }

  // -------------------------------------------------------------------------
  // A torn-off menu, posted over the desktop. Set 6, shadowThickness 2.

  Element postedMenu() {
    const Set& s = theme[6];
    auto item = [&](std::string_view t, bool cascade, bool insensitive) {
      Element row = box()
                        .row()
                        .alignItems(Align::Center)
                        .height(24)
                        .padding(14, 0)
                        .children({cde::label(t), box().grow(1)});
      if (cascade)
        row.children({box()
                          .width(9)
                          .height(9)
                          .fill(s.bg)
                          .overlay(cde::bevel(2, false, false))});
      if (insensitive) row.foreground(cde::stipple());
      return row;
    };
    // XmSHADOW_ETCHED_IN at T = 2 — a two-pass etched shadow, and the only
    // place in a CDE session where that branch of XmeDrawShadows shows.
    auto separator = [&] {
      return box()
          .height(2)
          .margin(3)
          .overlay(cde::bevel(2, true, true));
    };
    // The tear-off "perforation" Motif puts at the top of a posted menu.
    Element tearOff =
        box().height(9).margin(3).overlay(cde::bevel(2, true, true));

    return cde::surface(s)
        .overlay(cde::bevel(2, false, false))
        .padding(2)
        .width(214)
        .column()
        .children({std::move(tearOff), item("New...", false, false),
                   item("Open", true, false), separator(),
                   item("Print", false, true),
                   item("Properties...", false, false), separator(),
                   item("Close", false, false)});
  }

  // -------------------------------------------------------------------------
  // The derivation strip. Not a CDE widget and it does not pretend to be:
  // a plain Motif frame with a label, showing the function running over a
  // grey ramp that crosses both thresholds.

  Element derivationStrip() {
    const Set& s = theme[2];
    const int v = std::clamp((int)std::lround(sweep.value() * 255.0f), 0, 255);
    const cde::Rgb bgv =
        cde::from8((uint32_t)v << 16u | (uint32_t)v << 8u | (uint32_t)v);
    const cde::Derived d = cde::calculate(bgv);

    auto swatch = [&](const char* name, cde::Rgb c) {
      const std::string hex = kit::formatted("#%06X", cde::toHex(c));
      return box()
          .column()
          .gap(3)
          .alignItems(Align::Center)
          .children({box()
                         .width(70)
                         .height(36)
                         .fill(cde::toSk(c))
                         .overlay(cde::bevel(2, false, false)),
                     cde::label(name), cde::label(hex)});
    };

    const char* branch = d.branch == cde::Branch::Dark   ? "DARK"
                         : d.branch == cde::Branch::Lite ? "LITE"
                                                         : "MEDIUM";
    const std::string line =
        kit::formatted("B = %5d      branch %-6s      f = (%d, %d, %d)",
                       d.brightness, branch, d.fSel, d.fBs, d.fTs);

    Element proof = box().column().gap(1).justify(Justify::Center);
    for (const measure::Check& c : derivation().rows)
      proof.children({cde::label(c.line(22, 9), 10.0f, c.pass ? s.fg : s.bs)});

    return cde::surface(s)
        .overlay(cde::bevel(2, false, false))
        .padding(2)
        .column()
        .children({box().padding(8, 6).children({cde::mnemonicLabel(
                       "XmGetColors( bg ) - live", s.fg, 0)}),
                   box()
                       .row()
                       .gap(18)
                       .padding(8, 0)
                       .children({box()
                                      .row()
                                      .gap(6)
                                      .children({swatch("bg", d.bg)})
                                      .children({swatch("topShadow", d.ts)})
                                      .children({swatch("botShadow", d.bs)})
                                      .children({swatch("select", d.sel)})
                                      .children({swatch("foreground", d.fg)})})
                       // THE PROOF, on the plate: the sampled bytes this
                       // algorithm has to reproduce, each with its verdict
                       // computed from the two values rather than written.
                       .children({std::move(proof)}),
                   box().padding(8, 8).children({cde::label(line)})});
  }

  // -------------------------------------------------------------------------
  // The Front Panel. dtwm.fp.src's control list, in POSITION_HINTS order,
  // with DISPLAY_CONTROL_LABELS False — which is why it is only 86 px tall.

  /** The end handles: a 1-px alternating bottomShadow/topShadow texture,
   *  period 2 in y [MEAS at x = 60, y = 700..761, perfect alternation].
   *  The only texture on the panel.
   *
   *  A BAND EVERY OTHER ROW IS `styles::Scanlines`, which is one fill over
   *  the bottom-shadow ground rather than thirty-one boxes a flexbox has
   *  to lay out — the phase puts the light rows on the odd ones. */
  Element handle() {
    const Set s = cde::ambient();
    return box()
        .width(18)
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .children({box()
                       .width(18)
                       .height(31)
                       .fill(s.bs)
                       .overlay(styles::Scanlines{s.ts, 2, 1, 1})});
  }

  Element panelSeparator() {
    return box()
        .width(2)
        .column()
        .children({box().grow(1).overlay(cde::bevel(2, true, true))});
  }

  /** A Front Panel control: a 48 x 48 icon, 4 px either side, with the
   *  small chevron above it that marks a subpanel [SRC: Text Editor,
   *  Printer, Applications and Help have subpanels].
   *
   *  THE CHEVRON IS NOT `shapes::polygon(3)`, which the scrollbar stepper
   *  below is. A stepper arrow is one smoothed triangle inscribed in its
   *  box; this is four stacked rows of 1, 3, 5 and 7 px, and the steps
   *  are what a 1993 pixmap arrow is made of. An inscribed triangle would
   *  draw a smaller shape with a smoothed hypotenuse. */
  Element control(Element icon, float w, bool subpanelArrow) {
    const Set s = cde::ambient();
    Element chev = box()
                       .height(10)
                       .alignItems(Align::Center)
                       .justify(Justify::Center);
    if (subpanelArrow) {
      Element up = box().column().alignItems(Align::Center);
      for (int i = 0; i < 4; ++i)
        up.children({box()
                         .width((float)(1 + i * 2))
                         .height(1)
                         .fill(s.fg)});
      chev.children({std::move(up)});
    }
    return box()
        .width(w)
        .column()
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .children({std::move(chev), std::move(icon)});
  }

  /** Fpclock.l.pm plus dtwm's live hands. No second hand, and it STEPS
   *  ONCE A MINUTE — there is no smooth motion anywhere in CDE, so the
   *  step is declared with bind().quantize(61) rather than left to
   *  emerge from the frame rate. */
  Element clockIcon() {
    const Set s = cde::ambient();
    Element face = stack().width(48).height(48);
    face.children({box().inset(0).corners({24}).fill(s.bg).foreground(
        PathFormat{.width = 2,
                   .strokeFill = Fill::color(cde::C(cde::kIconGray[6])),
                   .align = PathFormat::Align::Inner})});
    face.children(
        {box().inset(4).corners({20}).fill(cde::C(cde::kIconGray[0]))});
    // Twelve ticks, entered at twelve o'clock and swept the whole way
    // round. `arrange::onRing` is the ring arithmetic's origin; a sketch
    // that respells it with its own sin and cos rounds differently.
    for (int i = 0; i < 12; ++i) {
      const SkPoint c = arrange::onRing(
          (size_t)i, 12, {24.0f, 24.0f}, {18.0f, 18.0f}, -(float)M_PI * 0.5f,
          2.0f * (float)M_PI, arrange::Turn::Closed);
      const float cx = c.fX, cy = c.fY;
      const float sz = (i % 3 == 0) ? 4.0f : 2.0f;
      face.children({box()
                         .left(cx - sz * 0.5f)
                         .top(cy - sz * 0.5f)
                         .width(sz)
                         .height(sz)
                         .fill(cde::C(cde::kIconColor[0]))});
    }
    // Hour hand, ~60% radius; minute hand, full radius. Both quantised to
    // the minute: 61 levels across [0,1] is a 6-degree step.
    face.children(
        {box()
             .left(23)
             .top(13)
             .width(3)
             .height(11)
             .fill(cde::C(cde::kIconColor[0]))
             .transformOrigin(0.5f, 1.0f)
             .rotate(
                 motion::bind(&clockT).quantize(61).scale(30).offset(300))});
    face.children({box()
                       .left(23)
                       .top(6)
                       .width(2)
                       .height(18)
                       .fill(cde::C(cde::kIconColor[0]))
                       .transformOrigin(0.5f, 1.0f)
                       .rotate(motion::bind(&clockT).quantize(61).scale(360))});
    face.children({box()
                       .left(22)
                       .top(22)
                       .width(4)
                       .height(4)
                       .corners({2})
                       .fill(cde::C(cde::kIconColor[0]))});
    return face;
  }

  /** FpCM, the date control — a calendar page. */
  Element dateIcon() {
    const Set s = cde::ambient();
    return stack()
        .width(48)
        .height(48)
        .ink(s.fg)
        .children({box()
                       .inset(3, 2, 3, 2)
                       .fill(cde::C(cde::kIconColor[1]))
                       .overlay(cde::bevel(2, false, false)),
                   box()
                       .left(5)
                       .top(4)
                       .width(38)
                       .height(13)
                       .fill(s.sel)
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .children({cde::label("Jul", 11.0f)}),
                   box()
                       .left(5)
                       .top(18)
                       .width(38)
                       .height(24)
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .children({cde::label("22", 19.0f,
                                             cde::C(cde::kIconColor[0]))})});
  }

  /** The SWITCH: dtwm.fp.src gives it NUMBER_OF_ROWS 2, plus Lock, the
   *  `TYPE busy` blinker, a blank and Exit. Workspace n's button is filled
   *  with colour set 3, 8, 6 and 7 respectively [MEAS] — the single most
   *  visible use of the palette on the screen. */
  Element workspaceSwitch() {
    static const char* kNames[4] = {"One", "Two", "Three", "Four"};
    static const int kSets[4] = {3, 8, 6, 7};
    Element gridEl = box().column().gap(3);
    for (int r = 0; r < 2; ++r) {
      Element rr = box().row().gap(3);
      for (int c = 0; c < 2; ++c) {
        const int i = r * 2 + c;
        const Set& ws = theme[kSets[i]];
        rr.children({cde::surface(ws)
                         .width(129)
                         .height(22)
                         .overlay(cde::bevel(2, false, false))
                         .row()
                         .alignItems(Align::Center)
                         .padding(7, 0)
                         .children({cde::label(kNames[i])})});
      }
      gridEl.children({std::move(rr)});
    }
    Element left =
        box()
            .column()
            .gap(4)
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .width(26)
            .children({cde::art(cde::icoLock(), 2.0f),
                       box()
                           .width(10)
                           .height(10)
                           .fill(cde::C(0x00C000))
                           .overlay(cde::bevel(1, true, false))
                           .opacity(motion::bind(&busy).quantize(2))});
    Element right = box()
                        .width(26)
                        .alignItems(Align::Center)
                        .justify(Justify::Center)
                        .children({cde::art(cde::icoExit(), 2.0f)});
    return box()
        .width(324)
        .row()
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .gap(3)
        .children({std::move(left), std::move(gridEl), std::move(right)});
  }

  Element frontPanel() {
    const Set& s = theme[2];  // dtsession: the primary colour set

    Element rowEl = box().row().alignItems(Align::Center).height(74);
    rowEl.children({handle()});
    rowEl.children({panelSeparator()});
    rowEl.children({control(clockIcon(), 58, false)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(dateIcon(), 56, false)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoHome(), 2.0f), 56, false)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoEditor(), 2.0f), 56, true)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoMail(), 2.0f), 54, false)});
    rowEl.children({panelSeparator()});
    rowEl.children({workspaceSwitch()});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoPrinter(), 2.0f), 56, true)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoStyle(), 2.0f), 54, false)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoApps(), 2.0f), 58, true)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoHelp(), 2.0f), 56, true)});
    rowEl.children({panelSeparator()});
    rowEl.children({control(cde::art(cde::icoTrash(), 2.0f), 56, false)});
    rowEl.children({panelSeparator()});
    rowEl.children({handle()});

    // A raised T = 2 shell containing a raised T = 2 box, 2 px apart
    // [MEAS: ts/ts/bg/bg/ts/ts going in from the top edge].
    return box()
        .fill(s.bg)
        .overlay(cde::bevel(2, false, false))
        .padding(4)
        .children({box()
                       .grow(1)
                       .fill(s.bg)
                       .overlay(cde::bevel(2, false, false))
                       .padding(2)
                       .children({std::move(rowEl)})});
  }

  /** A subpanel, sliding up out of the Front Panel. dtwm.fp.src gives
   *  subpanels to TextEditor (PersApps), Printer (PersPrinters),
   *  Applications and Help (HelpSubpanel) [SRC]; this is Help's.
   *  Element::wipe at 90 degrees is the reveal — and it covers the
   *  node's decorations, which is exactly what a bevelled panel needs
   *  and what trim()/scaleY could not have given. */
  Element helpSubpanel() {
    const Set& s = theme[2];
    Element col =
        cde::surface(s)
            .overlay(cde::bevel(2, false, false))
            .padding(4)
            .column()
            .alignItems(Align::Center)
            .gap(4)
            .children({cde::label("Help"),
                       box()
                           .row()
                           .gap(6)
                           .children({cde::art(cde::icoHelp(), 1.4f)})
                           .children({cde::art(cde::icoApps(), 1.4f)}),
                       box()
                           .height(6)
                           .width(60)
                           .overlay(cde::bevel(2, true, true))});
    return col.mask(by::edge(270.0f, &subpanel));  // 270 = from the BOTTOM
  }

  // -------------------------------------------------------------------------

  /** dtwm iconifies a window ONTO THE ROOT WINDOW as a bevelled square
   *  with the window's title beneath it — an XmSHADOW_OUT box in the
   *  primary colour set, which is why iconified windows change colour
   *  with the theme along with everything else. */
  Element iconifiedWindow(std::string_view title,
                          const std::vector<std::string>& pixmap) {
    const Set& s = theme[2];
    return box()
        .column()
        .alignItems(Align::Center)
        .gap(2)
        .children({box()
                       .width(64)
                       .height(64)
                       .fill(s.bg)
                       .overlay(cde::bevel(2, false, false))
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .children({cde::art(pixmap, 2.0f)}),
                   cde::surface(s)
                       .overlay(cde::bevel(1, false, false))
                       .padding(4, 1)
                       .children({cde::label(title)})});
  }

  Element describe(sketch::SketchContext& ctx) {
    // ONE REGISTER FOR THE WHOLE DESKTOP, declared where the desktop is:
    // CDE has no type hierarchy, so every label under here is set in this
    // and says only its words. The colour is not part of it — a colour
    // set's foreground is the ink of whatever wears the set.
    Element root = stack()
                       .width(1152)
                       .height(900)
                       .font(cde::uiType());

    // 1. The root window: PinStripe, tiled, in colour set 3's shadows.
    //    ONE 28 x 52 pixmap that dtwm tiles — not instanced, not
    //    randomised per repeat; the lattice is part of how it looks.
    root.children({box()
                       .inset(0)
                       .fill(backdrops[(size_t)paletteIndex].material())
                       .cache(Cache::Texture)});

    // 2. The File Manager.
    root.children({fileManager()
                       .left(40)
                       .top(48)
                       .width(600)
                       .height(452)});

    // 3. The Style Manager's Color dialog.
    root.children({colorDialog()
                       .left(664)
                       .top(96)
                       .width(452)
                       .height(356)});

    // 4. A posted (torn-off) menu.
    root.children({postedMenu().left(700).top(506)});

    // 5. The derivation strip — the only smooth motion on the canvas.
    root.children({slot("derivation").left(40).top(536)});

    // 6. The Help subpanel, wiping up out of the panel. The Help control
    //    sits at x = 912..968, so the subpanel is centred on it and its
    //    bottom edge meets the panel's top.
    root.children({helpSubpanel()
                       .left(865)
                       .top(700)
                       .width(150)
                       .height(106)});

    // 6b. Iconified windows on the root, where dtwm parks them.
    root.children(
        {box()
             .left(48)
             .top(690)
             .row()
             .gap(34)
             .children({iconifiedWindow("Terminal", cde::icoEditor())})
             .children({iconifiedWindow("Mailer", cde::icoMail())})
             .children({iconifiedWindow("Print Manager", cde::icoPrinter())})});

    // 7. The Front Panel, bottom-centred. 960 wide => 948 of content,
    //    which is exactly the measured control list.
    root.children({frontPanel()
                       .left(96)
                       .top(806)
                       .width(960)
                       .height(86)});

    (void)ctx;
    return root;
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override {
    // The still has to name its moment: palettes snap every 3 s over
    // {Default, Crimson, Black, Summer}, and an undeclared capture can land
    // on a snap or on Black, the all-black degenerate palette. Default —
    // the shipped canonical, loaded at setup — holds [12, 15); 13.5 s is
    // dead centre, with the derivation strip visibly mid-sweep.
    sketch::kit::stage(ctx, {.size = {1152, 900},
                             .captureAt = 13.5,
                             .background = cde::C(0x000000)});

    theme.load(*cde::kPalettes[0]);

    if (!backdropsBuilt) {
      for (size_t i = 0; i < cde::kPalettes.size(); ++i) {
        const cde::Derived d = cde::calculate(cde::kPalettes[i]->set[2]);
        // NEAREST, and it is the whole reason the backdrop reads. This
        // is a 1993 PIXMAP: two colours, one pixel each, at full contrast
        // between the set's top and bottom shadows — the widest pair the
        // derivation produces. Sampled linearly the 1 px dither smears
        // into a flat mid-tone and the lattice disappears, which is the
        // opposite of what a dither is for.
        backdrops[i] =
            Pattern::tile({28, 52},
                          cde::pinStripeTile(cde::toSk(d.ts), cde::toSk(d.bs)))
                .sampling(SkSamplingOptions(SkFilterMode::kNearest));
      }
      backdropsBuilt = true;
    }

    // The clock: 60x, so a minute passes every second and the hand
    // visibly steps.
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      clockT = (float)std::fmod(t / 60.0 + 0.42, 1.0);
      busy = (std::fmod(t, 0.5) < 0.25) ? 1.0f : 0.0f;
      caret = (std::fmod(t, 1.0) < 0.5) ? 1.0f : 0.0f;
      // The subpanel posts and unposts on a 12 s cycle, in step with the
      // palette loop.
      const double c = std::fmod(t, 12.0);
      const double up =
          c < 1.0 ? c
                  : (c < 10.5 ? 1.0 : std::max(0.0, 1.0 - (c - 10.5) / 0.7));
      subpanel = (float)std::clamp(up, 0.0, 1.0);
      // The sweep: 0 -> 255 over 8 s, held 1 s at each end.
      const double u = std::fmod(t, 10.0);
      sweep = (float)(u < 1.0 ? 0.0 : (u < 9.0 ? (u - 1.0) / 8.0 : 1.0));
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("derivation", derivationStrip());
  }

  int lastSweepByte = -1;

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // Beat one: the palette cycles every 3 s. SNAP, do not crossfade —
    // CDE's colour server re-allocates the cells and every window
    // repaints on the next expose; a 300 ms lerp across a desktop would
    // be the single most anachronistic thing you could add.
    if (elapsed >= nextSwitch) {
      nextSwitch = std::floor(elapsed / 3.0) * 3.0 + 3.0;
      paletteIndex =
          ((int)std::floor(elapsed / 3.0)) % (int)cde::kPalettes.size();
      theme.load(*cde::kPalettes[(size_t)paletteIndex]);
      // A palette change is a re-describe, which is what the inherited
      // channel makes it: the reconciler patches the nodes whose properties
      // moved and leaves the rest, where a bound colour would have made
      // every node on the desktop volatile for ever to save this.
      ctx.composer.render(describe(ctx));
    }

    const int b = std::clamp((int)std::lround(sweep.value() * 255.0f), 0, 255);
    if (b != lastSweepByte) {
      lastSweepByte = b;
      ctx.composer.renderSlot("derivation", derivationStrip());
    }
  }
};

SIGIL_SKETCH(CdeMotifSketch, "Study · Screens",
             "CDE 1.0 on OSF/Motif 2.1 (1995) — XmGetColors reproduced "
             "byte-exact")
