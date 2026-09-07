#pragma once

/** @file
 * TWO DIRECTORIES OF PLATES, PICTURE BY PICTURE.
 *
 * A byte-identity sweep needs no decoder — two plates are the same file
 * or they are not. Comparing two RENDERERS does: the same sketch drawn
 * on the CPU and on a device is the same picture within a tolerance and
 * never the same bytes, so the question is how far apart the two are.
 * That is a decode and an arithmetic over pixels, which is what this
 * binary already carries, so it is answered here rather than in whatever
 * script asks — and the script keeps the tolerances, because deciding
 * what is close enough is a judgement about a machine and not a fact
 * about two files.
 */

#include <string>

namespace sigil::sketch {

/** Two plate directories, and what to print about them. */
struct CompareOptions {
  std::string first;
  std::string second;
};

/** Compares every plate that stands in either directory and prints one
 *  line per plate, each opening with the word that says what it is:
 *
 *      compared <name> mean <mean> p99 <p99> max <max>
 *               clear <max over transparent> content <max over the rest>
 *      size <name> <W>x<H> <W>x<H>
 *      missing <name> first|second
 *      unreadable <name> first|second
 *
 *  Every distance is an absolute difference of one 8-bit channel, in
 *  0..255, over every channel of every pixel. `clear` and `content` are
 *  the same worst difference split by what the FIRST plate holds where the
 *  difference is — transparent black, or anything at all — because a
 *  caller's tolerance can depend on it: a picture drawn over nothing and
 *  the same picture drawn over something are not composited the same
 *  number of times. Which pixels are which is a fact about two files;
 *  how much each is allowed is a judgement and stays with the caller.
 *
 *  Returns 0 when every plate was compared, 1 when any was missing,
 *  unreadable or a different size, and 2 when a directory cannot be read
 *  at all. */
int compare(const CompareOptions& options);

}  // namespace sigil::sketch
