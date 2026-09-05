#pragma once

/** @file
 * The prose a specimen is set in, read from beside the sketch rather than
 * typed into it.
 */

#include <sigilsketch/canvas/Sketch.h>

#include <string>
#include <string_view>

namespace sigil::sketch::kit {

/** THE PASSAGE AT `res://passages/<name>`, as one string.
 *
 *      body = sketch::kit::passage(ctx, "paragraph_paints.txt");
 *
 *  A page of running text is the SUBJECT of a sketch about setting one,
 *  and it is also the longest thing in the file that is about the
 *  setting — so it stands beside the sketch as a file and reaches it
 *  through the sketch's own resource hub, which caches it and notices an
 *  edit to it.
 *
 *  THE TEXT IS THE FILE'S, MINUS THE NEWLINES IT ENDS WITH. A file ends
 *  in one by convention rather than because the prose breaks there, and
 *  a paragraph engine handed it would set an empty line after the
 *  passage. Every newline before the last word is the author's and is
 *  kept, so a passage of several paragraphs is several lines.
 *
 *  A MISSING RESOURCE IS THE EMPTY STRING, and says so on stderr. It is
 *  a broken checkout rather than a condition to render around: a page
 *  set in nothing is unmistakable, where a page set in a stand-in is a
 *  plate that quietly stopped being the picture its header describes. */
[[nodiscard]] std::u8string passage(SketchContext& ctx,
                                    std::string_view name);

}  // namespace sigil::sketch::kit
