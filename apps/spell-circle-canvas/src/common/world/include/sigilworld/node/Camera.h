#pragma once

/** @file
 * @ingroup world-node
 * A CAMERA: Geometry's lens, standing where its node is placed, and how
 * the picture it takes is read — the exposure and the tone curve.
 */

#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Verbs.h>

namespace sigil::world {

/** ONE VIEWPOINT. Its lens's eye and target are carried by the node's
 *  placement, so a camera riding a rail looks from wherever it has
 *  reached. A frame looks through one camera: the only one in the tree,
 *  or the one the frame names by key. */
class Camera : public Node,
               public PlacementVerbs<Camera>,
               public IdentityVerbs<Camera> {
 public:
  Camera() = default;

  /** What this node describes. */
  [[nodiscard]] const CameraDescription& description() const;
};

/** How the picture a camera takes is read. */
struct CameraOptions {
  /** What every radiance is multiplied by before the tone curve
   *  compresses it; doubling it is one stop. */
  motion::Animatable<float> exposure{1.0f};
  /** The curve that compresses a lit sum onto what a display holds. */
  ToneCurve toneCurve = ToneCurve::Reinhard;
};

/** A camera looking through @p lens. */
Camera camera(geometry::mesh::camera::Camera lens, CameraOptions options = {});

}  // namespace sigil::world
