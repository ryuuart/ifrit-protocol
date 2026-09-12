#include "ThunderFulu.h"

auto ThunderFulu::runChecks() -> void {
  verdict = {};
  // --- panel A: the stroke data -----------------------------------------
  logA.append(
      {toUtf8("makemeahanzi graphics.txt \xe2\x80\x94 MEDIANS, 1024 em, "
              "DRAWING ORDER"),
       "heading"});
  int totalPts = 0, totalStrokes = 0;
  for (const Glyph& g : font.glyphs) {
    const int16_t* p = g.data.data();
    for (int s = 0; s < g.strokes; ++s) {
      totalPts += *p;
      p += 1 + 2 * (*p);
    }
    totalStrokes += g.strokes;
  }
  logA.append(
      {toUtf8(kit::formatted("  11 characters, %d strokes, %d median points, "
                             "verbatim",
                             totalStrokes, totalPts)),
       "dim"});
  const char* stems = claim("GANG U+7F61 strokes, the ten Heavenly Stems", 10,
                            font.glyphs[GANG].strokes);
  logA.append({toUtf8(kit::formatted("  GANG U+7F61 has %d strokes",
                                     font.glyphs[GANG].strokes)),
               stems});
  logA.append(
      {toUtf8("  doctrine: 10 strokes = the ten Heavenly Stems"), "dim"});
  logA.append({toUtf8("  JIA YI BING DING WU JI GENG XIN REN GUI \xe2\x80\x94 "
                      "the count MATCHES"),
               stems});
  logA.append({toUtf8(kit::formatted(
                   "  foot JI+JI+RU+LU+LING = %d+%d+%d+%d+%d = %d strokes",
                   font.glyphs[JI].strokes, font.glyphs[JI].strokes,
                   font.glyphs[RU].strokes, font.glyphs[LV].strokes,
                   font.glyphs[LING].strokes, nFootStrokes)),
               claim("foot JI+JI+RU+LU+LING strokes", 38, nFootStrokes)});
  logA.append({toUtf8(kit::formatted(
                   "    drawn as ONE contour: %d spans = %d strokes + "
                   "%d ligatures",
                   2 * nFootStrokes - 1, nFootStrokes, nFootStrokes - 1)),
               "pass"});
  logA.append({toUtf8(kit::formatted(
                   "  body YU+WU / YUN / GUI = %d cloud-seal strokes", nBody)),
               claim("body YU+WU / YUN / GUI cloud-seal strokes", 33, nBody)});
  logA.append({toUtf8(kit::formatted("  ink strokes on the plate: %d nodes",
                                     (int)strokes.size())),
               "number"});

  // --- panel B: the taxonomy --------------------------------------------
  logB.append({toUtf8("KanjiVG kvg:type \xe2\x80\x94 THE STROKE CLASS, FOR w0"),
               "heading"});
  // A statement about the SOURCE, not about this reconstruction: its
  // verdict is printed as any claim's is and never counted against the run.
  verdict.add(sigil::measure::finding(
      sigil::measure::check("KanjiVG carries GANG U+7F61", false)));
  logB.append(
      {toUtf8("  kanji/07f61.svg (GANG) is a 14-byte 404. NOT A KANJI."),
       "fail"});
  logB.append(
      {toUtf8("  recovered instead from the median geometry: length,"), "dim"});
  logB.append(
      {toUtf8("  body chord angle over 14-86%, peak turn from entry"), "dim"});
  logB.append({toUtf8(kit::formatted(
                   "  vs KanjiVG over the %d strokes it does carry: %d/%d",
                   kvgTotal, kvgAgree, kvgTotal)),
               [&] {
                 verdict.add(sigil::measure::check(
                     kit::formatted("classifier agrees with KanjiVG on %d "
                                    "of %d strokes, at least 80%%",
                                    kvgAgree, kvgTotal),
                     kvgAgree * 10 >= kvgTotal * 8));
                 return verdict.rows.back().pass ? "pass" : "fail";
               }()});
  for (size_t i = 0; i < kvgMiss.size() && i < 3; ++i)
    logB.append({toUtf8("    " + kvgMiss[i]), "dim"});
  logB.append(
      {toUtf8("  GUI U+9B3C: mmah 9, KanjiVG 10 \xe2\x80\x94 the Japanese"),
       "dim"});
  logB.append(
      {toUtf8("    form carries an extra PIE; and the two sources order"),
       "dim"});
  logB.append(
      {toUtf8("    TIAN's interior in LEI U+96F7 opposite ways"), "number"});
  {
    std::string row = "  GANG recovered: ";
    for (int i = 0; i < 10; ++i) {
      std::string n = kClsName[gangCls[(size_t)i]];
      while (!n.empty() && n.back() == ' ') n.pop_back();
      row += n.substr(0, 2) + (i < 9 ? " " : "");
    }
    logB.append({toUtf8(row), "number"});
  }

  // --- panel C: the brush ------------------------------------------------
  logC.append({toUtf8("THE BRUSH \xc2\xb7 QI / XING / SHOU, one law over arc "
                      "length"),
               "heading"});
  logC.append(
      {toUtf8("  w(s)/w0 = 1.15e^-12s + 0.62 + 0.28s + 0.55e^-90(s-.88)^2"),
       "dim"});
  logC.append(
      {toUtf8(kit::formatted("    ni feng (reverse entry) w(0)    = %.2f w0",
                             (double)widthLaw(0.0f))),
       "number"});
  {
    float minS = 0, minW = 1e9f;
    for (int i = 0; i <= 100; ++i) {
      const float s = (float)i / 100.0f, w = widthLaw(s);
      if (w < minW) {
        minW = w;
        minS = s;
      }
    }
    logC.append(
        {toUtf8(kit::formatted("    zhong feng belly  min %.2f w0 at s = %.2f",
                               (double)minW, (double)minS)),
         "number"});
  }
  logC.append({toUtf8(kit::formatted("    dun (the press)   w(0.88) = %.2f w0",
                                     (double)widthLaw(0.88f))),
               "number"});
  logC.append({toUtf8(kit::formatted("    shou (the cut)    w(1)    = %.2f w0",
                                     (double)widthLaw(1.0f))),
               "number"});
  logC.append(
      {toUtf8("  brush::Ribbon::width takes exactly this law, as a"), "dim"});
  logC.append({toUtf8("  comparable Profile. BUT under trim() a decoration is"),
               "dim"});
  logC.append(
      {toUtf8("  handed the REVEALED contour: a fraction is a fraction"),
       "dim"});
  logC.append(
      {toUtf8("  of what is drawn SO FAR, so the profile is keyed in PX"),
       "dim"});
  logC.append(
      {toUtf8("  or the dun press SLIDES down the stroke as it writes."),
       "pass"});
  logC.append(
      {toUtf8(kit::formatted("  tempo: foot %.3f s/stroke vs body %.3f = %.1fx",
                             (double)tFootEach, (double)tBodyEach,
                             (double)(tBodyEach / tFootEach))),
       "number"});
}

auto ThunderFulu::validateClassifier() -> void {
  kvgAgree = kvgTotal = 0;
  kvgMiss.clear();
  for (const KvgRow& row : font.kvg) {
    const std::vector<Poly> ms = medians(font, row.glyph);
    if ((int)ms.size() != row.n)  // GUI: 9 vs 10 — reported, not compared
      continue;
    for (size_t i = 0; i < ms.size(); ++i) {
      const int got = classify(ms[i]);
      ++kvgTotal;
      if (got == row.cls[i])
        ++kvgAgree;
      else
        kvgMiss.push_back(kit::formatted(
            "%s #%d  kvg %s  geo %s", font.glyphs[(size_t)row.glyph].id.c_str(),
            (int)i + 1, kClsName[row.cls[i]], kClsName[got]));
    }
  }
}
