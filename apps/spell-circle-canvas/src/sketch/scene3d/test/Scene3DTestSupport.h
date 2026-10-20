#pragma once

/** @file
 * What the scene3d session's cases share: the fixture every suite that opens
 * a scene3d sketch stands on, the turning box most of them open, and the
 * ways a case draws a frame of a session to read it.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <chrono>
#include <string>
#include <string_view>
#include <thread>

#include "support/Devices.h"

namespace sigil::sketch::test {

/** A CASE THAT OPENS A SCENE3D SKETCH: the device runtime installed for the
 *  process before it, as a host installs it, and taken away after it, or the
 *  case skipped naming what is missing. */
class OnDevice : public sigil::sketch::test::OnDeviceRuntime {};

class Scene3DSession : public OnDevice {};
class Scene3DDoors : public OnDevice {};
class Scene3DWindow : public OnDevice {};
class Scene3DBodies : public OnDevice {};
class Scene3DKinds : public OnDevice {};

/** The body every scene3d sketch here turns: a rounded box, whose flat faces
 *  turn to and from the sun as it spins and whose silhouette a rasteriser
 *  with no antialiasing still draws whole. */
inline sigil::geometry::mesh::Mesh roundedBox() {
  return sigil::geometry::mesh::superellipsoid({40, 40, 40}, 6.0f, 24, 16);
}

/** One box on a turntable, and nothing else. It counts the times its
 *  body was asked to describe, which is the observable behind the
 *  difference between stepping and repainting. */
struct Spun {
  static inline int describes = 0;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0.05f, 0.05f, 0.08f, 1});
    ctx.captureAt(0.5);
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 90, 260};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
  }
  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    ++describes;
    return world::group({world::directionalLight(
                             {-0.4f, -0.8f, -0.3f},
                             {.color = sigil::material::Color{1, 1, 1, 1},
                              .intensity = 1.0f})
                             .key("sun"),
                         world::mesh(roundedBox())
                             .key("body")
                             .rotateY(seconds * 90.0f)
                             .fill(sigil::material::surface::program())})
        .key("set");
  }
};

/** One frame of @p session onto a plate of the declared canvas size. */
inline SkBitmap oneFrame(Session& session) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  session.frame(canvas, 1.0 / 60.0);
  return bitmap;
}

/** The bodies the last frame of @p session stood in for because their
 *  program's pipeline was still building. */
inline double pendingPrograms(const Session& session) {
  for (const LaneCost& lane : session.lanes())
    if (std::string_view(lane.name) == "pending") return lane.ms;
  return 0;
}

/** ONE FRAME AS ITS SURFACES SHADE IT. A live session draws a body as a
 *  stand-in while its program compiles on a worker and while that
 *  program's pipeline builds, so the same moment is drawn again once
 *  every compile is done, and again until a frame stood in for no body:
 *  what is read is the surface and not its stand-in. It is drawn again
 *  whether or not a compile is still running when the first frame
 *  returns: one that finished while that frame was drawing left its
 *  stand-in in the picture all the same. */
inline SkBitmap shadedFrame(Session& session) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(160, 120));
  SkCanvas canvas(bitmap);
  session.frame(canvas, 1.0 / 60.0);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  do {
    while (sigil::material::ProgramCache::shared().compiling() > 0 &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    session.frame(canvas, 0.0);
  } while (pendingPrograms(session) > 0 &&
           std::chrono::steady_clock::now() < deadline);
  return bitmap;
}

}  // namespace sigil::sketch::test
