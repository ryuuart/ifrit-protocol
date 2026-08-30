#pragma once

/** @file
 * Every public header of SigilGeometry in one include. Transitional: a
 * consumer that spelled the headers by bare name before they moved under
 * their features includes this one and keeps compiling, then narrows to
 * the feature headers it actually uses.
 */

#include "sigilgeometry/blend/Blend.h"
#include "sigilgeometry/codec/Decode.h"
#include "sigilgeometry/codec/Encode.h"
#include "sigilgeometry/codec/Model.h"
#include "sigilgeometry/curves/Curves.h"
#include "sigilgeometry/easel/Easel.h"
#include "sigilgeometry/material/Materials.h"
#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Ops.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"
#include "sigilgeometry/points/Points.h"
#include "sigilgeometry/pop/Pop.h"
#include "sigilgeometry/space/Space.h"
