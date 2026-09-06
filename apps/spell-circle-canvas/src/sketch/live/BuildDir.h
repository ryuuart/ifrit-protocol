#pragma once

/** @file
 * Where the hosts of one process build: one directory, named for the
 * process, made with the first host and removed with the last.
 */

#include <filesystem>

namespace sigil::sketch {

/** Makes the directory for the first host in this process and hands
 *  every host the same path.
 *
 *  ONE PER PROCESS, shared by every host in it, and named for the process
 *  so that two runs side by side never write into one. It holds the
 *  cached objects and one dylib per build, all of it useless the moment
 *  the process ends: the freshness table that decides a rebuild is in
 *  memory, so nothing here is ever read by a later run.
 *
 *  REMOVING IT DOES NOT DISTURB WHAT IS RUNNING. Every dylib the host
 *  loaded stays mapped for the life of the process — none is ever
 *  dlclosed, because a live session may hold a vtable or a string literal
 *  inside one — and an unlinked file that is mapped stays readable until
 *  the last mapping goes. */
[[nodiscard]] std::filesystem::path acquireBuildDir();

/** The last host to let go removes the directory. */
void releaseBuildDir();

/** WHICH HOST IN THIS PROCESS, counted from one and never reused.
 *
 *  Every host in a process links into one directory, and the window
 *  keeps three sketches resident, each with a host of its own. Naming a
 *  build by its generation alone would have all of them writing
 *  `sketch_1.dylib`: two hosts building at once race for the path, and
 *  the file standing there when one of them dlopens is whichever link
 *  finished last — so a host adopts a sketch it did not build.
 *
 *  The image already loaded is not the exposure. The linker REPLACES its
 *  output rather than rewriting it, so the inode a mapped dylib is
 *  reading stays alive under it however many times the path is relinked,
 *  and dlopen of a replaced path loads the new file rather than handing
 *  back the old image. The exposure is the window between a link and the
 *  dlopen that follows it, and an id per host closes it by giving no two
 *  hosts a path in common. */
[[nodiscard]] int nextHostId();

}  // namespace sigil::sketch
