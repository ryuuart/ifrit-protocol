/** @file
 * Receiver: the other side of the publish door. A publication is a name
 * other applications on this machine can subscribe to, and this is the
 * subscriber — so what a publisher offers can be looked at, and written
 * down, without a third application in the middle.
 *
 *   Receiver --list                          every publication on this
 *                                            machine, one per line
 *   Receiver <name> [--app <application>]    a window on that publication
 *   Receiver <name> --grab <png> [--frames <n>]
 *            [--timeout <seconds>]           its newest frame, written as
 *                                            a PNG, and nothing on screen
 *
 * `<name>` is what the publication announced itself as, which is the
 * first column `--list` prints; the application drawing it is the second.
 * `--app` picks between two applications publishing the same name, and
 * without it whichever answers first is taken.
 *
 * THE WINDOW WAITS FOR ITS PUBLICATION. A name nothing is publishing yet
 * is not an error: the window says it is waiting and shows the first frame
 * that arrives, and it does the same again if the publisher stops and
 * starts. What it shows is the frame at the size it was drawn, one pixel
 * each where the screen has room, and the title says what is arriving and
 * how fast it arrives.
 *
 * A GRAB IS A MEASUREMENT, so it refuses rather than waits: it exits 2
 * when nothing is publishing under that name, 3 when the frames it was
 * told to wait for did not arrive in time, and 4 when the frame could not
 * be written. It waits for NEW frames — one, unless told otherwise — so
 * what it writes was drawn after it subscribed rather than whatever the
 * last subscriber left behind.
 */

#include <cstdio>
#include <optional>

#include "Arguments.h"
#include "Grab.h"
#include "Servers.h"
#include "Window.h"

int main(int argc, char *argv[]) {
  const std::optional<receiver::Arguments> parsed = receiver::parseArguments(argc, argv);
  if (!parsed) return 2;
  const receiver::Arguments &arguments = *parsed;

  if (arguments.list) {
    // ONE LINE PER PUBLICATION, the name first because it is the argument
    // every other verb here takes, and the application after a tab so a
    // script can split the line and a reader can see whose it is.
    NSArray<NSDictionary<NSString *, id> *> *offered =
        receiver::publications(receiver::kAnnounceSeconds);
    for (NSDictionary<NSString *, id> *publication in offered)
      std::printf("%s\t%s\n", receiver::publicationName(publication).UTF8String,
                  receiver::publicationApp(publication).UTF8String);
    // A machine with nothing to subscribe to is an answer, not a failure —
    // and it is said where a script reading the lines will not read it.
    if (offered.count == 0) std::fprintf(stderr, "nothing is publishing on this machine\n");
    return 0;
  }

  if (!arguments.grabPath.empty()) return receiver::runGrab(arguments);
  return receiver::runWindow(arguments);
}
