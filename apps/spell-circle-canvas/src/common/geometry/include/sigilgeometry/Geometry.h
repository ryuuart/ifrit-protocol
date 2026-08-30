#pragma once

/** @file
 * Every public header of SigilGeometry in one include, for a consumer
 * that takes the whole library rather than a tier of it. Narrowing to
 * the feature headers actually used is always available.
 */

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/codec/Encode.h"
#include "sigilgeometry/mesh/codec/Model.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "sigilgeometry/mesh/render/Painter.h"
#include "sigilgeometry/mesh/render/Runtime.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Ops.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"
#include "sigilgeometry/path/blend/Blend.h"
