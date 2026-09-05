#pragma once
/** @file
 * The corpus every shaping, layout and paint benchmark measures over: two
 * fixed word lists and the texts drawn from them, so a run is comparable
 * with the last. The style the corpus is set in and the process-wide font
 * context it shapes through are the ones the tests use, so a benchmark and
 * a test measure the same engine over the same inputs.
 */

#include <random>
#include <string>
#include <vector>

#include "../../test/support/Faces.h"
#include "../../test/support/Paragraphs.h"

namespace sigil::weave::bench {

using test::basicStyle;
using test::makePooledText;

inline const std::vector<std::u8string>& latinWords() {
  static const std::vector<std::u8string> words = {
      u8"the",     u8"quick",   u8"brown",  u8"fox",     u8"jumps",
      u8"over",    u8"lazy",    u8"dogs",   u8"while",   u8"seventy",
      u8"wizards", u8"conjure", u8"spell",  u8"circles", u8"of",
      u8"light",   u8"and",     u8"shadow", u8"beneath", u8"ancient",
      u8"stars",   u8"text",    u8"layout", u8"engines", u8"measure",
      u8"shape",   u8"place",   u8"glyphs", u8"with",    u8"care"};
  return words;
}

inline const std::vector<std::u8string>& cjkWords() {
  static const std::vector<std::u8string> words = {
      u8"文字",   u8"レイアウト", u8"エンジン", u8"高速",   u8"描画", u8"字形",
      u8"配置",   u8"計算",       u8"한글",     u8"텍스트", u8"배치", u8"엔진",
      u8"빠르게", u8"그리기",     u8"漢字",     u8"排版",   u8"引擎", u8"快速",
      u8"绘制",   u8"字体",       u8"測定",     u8"測量",   u8"캐시"};
  return words;
}

/** `wordCount` space-separated words, every third one CJK when `mixed`. */
inline std::u8string makeText(int wordCount, bool mixed, uint32_t seed = 7) {
  if (!mixed) return makePooledText(latinWords(), wordCount, seed);
  // The mixed corpus draws twice per word — the pool and then the word —
  // so its sequence is its own and not the single-pool generator's.
  std::mt19937 randomEngine(seed);
  const auto& latin = latinWords();
  const auto& cjk = cjkWords();
  std::u8string text;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    if (randomEngine() % 3 == 0)
      text += cjk[randomEngine() % cjk.size()];
    else
      text += latin[randomEngine() % latin.size()];
    text += ' ';
  }
  return text;
}

/** `wordCount` CJK words run together with no separators — the corpus a
 *  COLUMN is set in, where the break opportunities are between characters
 *  rather than at spaces, and every character stands upright. */
inline std::u8string makeColumnText(int wordCount, uint32_t seed = 11) {
  return makePooledText(cjkWords(), wordCount, seed, u8"");
}

}  // namespace sigil::weave::bench
