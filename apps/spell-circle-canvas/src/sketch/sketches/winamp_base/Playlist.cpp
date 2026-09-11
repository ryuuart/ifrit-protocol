#include "WinampBase.h"

auto WinampBase::playlistWindow() -> Element {
  using namespace wa;
  const float W = 400, H = 377;
  Element w = box().width(Dim(n(W))).height(Dim(n(H)));
  w.child(box().inset(0).fill(steel).cache(Cache::Texture));
  raised(w, kWellHi, kWellLo);
  w.child(titleBar(W, "WINAMP PLAYLIST", true, false, 20.0f));

  // The list well: left rail 12, right rail 20. At this window height it
  // is 319 native px tall, which is 24 whole 13 px rows and half of a
  // twenty-fifth — the list draws all 25 and the clip takes the rest,
  // exactly as the real window does at a size that is not a multiple of
  // the row height.
  Element list = at(box(), 12, 20, W - 32, 319).fill(kPlBg);
  sunken(list, mskia::withAlpha(hexColor(0x4A4A70), 0.6f), hexColor(0x06060A));
  // row backgrounds: one atlas stamp, three tint states.
  list.child(box().inset(0).child(
      instancing::instances(rowAtlas, rowPool, instancing::Mode::Live)));
  list.child(box().inset(0).clip().child(slot("tracks")));
  w.child(list);

  // The scrollbar rail and its two arrow buttons. NOT a stepper at each
  // end of a track: the rail runs the whole height of the well and the
  // two arrows stack INSIDE its bottom 28 px, over it. The grip is a
  // skin's sprite at the one size the skin cut it, so it is not a
  // reading of how much list is showing either.
  Element rail = at(box(), W - 20, 20, 20, 319).fill(hexColor(0x1A1A2A));
  sunken(rail, mskia::withAlpha(hexColor(0x4A4A70), 0.5f), hexColor(0x0A0A12));
  w.child(rail);
  Element grip = at(box(), W - 19, 24, 18, 36).fill(kBtnFace);
  raised(grip);
  w.child(grip);
  auto arrow = [&](float y, bool up) {
    Element e = key(W - 19, y, 18, 14, box());
    e.child(part(6, up ? 5 : 4, 6, 5, upDown(up)));
    return e;
  };
  w.child(arrow(310, true));
  w.child(arrow(324, false));

  // ---- the bottom control strip (native y 339..377) -------------------
  Element bottom = at(box(), 0, 339, W, 38);
  bottom.child(box().inset(0).fill(steel).cache(Cache::Texture));
  bottom.child(
      at(box(), 0, 0, W, 1).fill(mskia::withAlpha(hexColor(0x585880), 0.6f)));

  // ADD / REM / SEL / MISC. These sit in the strip's own coordinates, 6
  // native px clear of the sill — in the real window the row is pinned to
  // the bottom of the playlist, not to a fixed y in the window, so it
  // stays put whatever height the list is resized to.
  static const char* menus[4] = {"ADD", "REM", "SEL", "MISC"};
  for (int i = 0; i < 4; ++i)
    bottom.child(textKey(14 + 29 * (float)i, 14, 22, 18, menus[i], 4.0f));
  Element opts = key(W - 44, 14, 22, 18, box());
  opts.column().justify(Justify::Center).alignItems(Align::Center);
  opts.child(t("LIST", pix(3.8f, hexColor(0x121A24))));
  opts.child(t("OPTS", pix(3.8f, hexColor(0x121A24))));
  bottom.child(opts);

  // running-time readout
  bottom.child(at(box(), 132, 13, 62, 7)
                   .alignItems(Align::Center)
                   .child(t(runningTime(), pix(4.0f, kPlText))));

  // the mini transport dock
  Element dock = at(box(), 132, 22, 62, 12).fill(hexColor(0x12121E));
  sunken(dock, mskia::withAlpha(hexColor(0x4A4A70), 0.55f), hexColor(0x08080E));
  for (int i = 0; i < 5; ++i) {
    Element g = box();
    if (i == 0) {
      g.child(part(2, 3, 1, 5));
      g.child(part(4, 3, 4, 5, tri(1)));
    } else if (i == 1) {
      g.child(part(3, 2, 5, 6, tri(0)));
    } else if (i == 2) {
      g.child(part(3, 3, 2, 5));
      g.child(part(6, 3, 2, 5));
    } else if (i == 3) {
      g.child(part(3, 3, 5, 5));
    } else {
      g.child(part(3, 3, 4, 5, tri(0)));
      g.child(part(8, 3, 1, 5));
    }
    Element b = at(box(), 2 + 12 * (float)i, 1, 11, 10)
                    .fill(mskia::withAlpha(kBtnFace, 0.9f));
    raised(b, mskia::withAlpha(kBtnHi, 0.8f), kBtnLo);
    b.child(g);
    dock.child(b);
  }
  bottom.child(dock);

  // the preview-visualiser swatch (default checkerboard art)
  Element sw = at(box(), W - 88, 20, 38, 14).fill(hexColor(0x000000));
  sunken(sw, mskia::withAlpha(hexColor(0x4A4A70), 0.5f), hexColor(0x08080E));
  sw.child(box().inset(0).fill(previewCheck.material()));
  bottom.child(sw);
  w.child(bottom);
  return w;
}

auto WinampBase::runningTime() -> std::string {
  int total = 0;
  for (const Track& tr : tracks()) total += tr.seconds;
  return mmss((int)(playPos.value() * tracks()[(size_t)nowPlaying].seconds)) +
         "/" + mmss(total);
}

auto WinampBase::trackList() -> Element {
  using namespace wa;
  Element col = box().column();
  const float rowH = 13, listW = 368;
  for (int i = 0; i < 25; ++i) {
    const Track& tr = tracks()[(size_t)i];
    const SkColor4f ink = i == nowPlaying ? kPlNow : kPlText;
    Element r = box()
                    .height(Dim(n(rowH)))
                    .row()
                    .alignItems(Align::Center)
                    .padding(n(3), 0, n(3), 0);
    // PLEDIT.TXT Font=Arial, CSS font-size 9px + 0.5px tracking.
    auto st = type(arial(), n(9) * 0.78f, ink, n(0.5f));
    r.child(ellipsized(i, std::to_string(i + 1) + ". " + tr.title, st,
                       n(listW - 40)));
    r.child(box().grow(1));
    r.child(t(tr.time, type(arial(), n(9) * 0.78f, ink, n(0.5f))));
    // Rows reveal in bands of four: 25 rows on an even stagger reads as 25
    // separate animations, where batching reads as a list populating.
    r.opacity(&rowIn[(size_t)i]);
    r.translateY(motion::bind(&rowIn[(size_t)i]).invert().scale(n(2)));
    col.child(r);
  }
  return col;
}

auto WinampBase::ellipsized(int idx, const std::string& s,
                            const sigil::weave::TextStyle& st, float w)
    -> Element {
  sigil::weave::ParagraphBuilder b(st);
  b.addText(toU8(s));
  auto p = std::make_shared<sigil::weave::Paragraph>(b.build());
  if ((int)rowPara.size() <= idx) rowPara.resize((size_t)idx + 1);
  rowPara[(size_t)idx] = p;
  sigil::weave::ParagraphLayoutOptions o;
  o.overflow.ellipsis = u"…";
  o.overflow.maxLines = 1;
  return text(p, o).width(Dim(w)).shrink(0);
}
