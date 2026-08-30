#pragma once

/** @file
 * A primvar of any interpolation flattened to one value per
 * face-vertex, which is the order an unwelded mesh's vertices take.
 */

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <vector>

namespace sigil::usd {

/** One value per face-vertex from @p values under @p interp: constant
 *  repeats the first, uniform indexes by face, faceVarying by
 *  face-vertex, vertex and varying by point; @p primvarIndices, when
 *  present, indirect the lookup. Out-of-range lookups yield T(). */
template <class T>
std::vector<T> perFaceVertex(const pxr::VtArray<T>& values,
                             const pxr::TfToken& interp,
                             const pxr::VtIntArray& counts,
                             const pxr::VtIntArray& indices,
                             const pxr::VtIntArray& primvarIndices) {
  std::vector<T> out;
  out.reserve(indices.size());
  int face = 0, inFace = 0;
  for (size_t fv = 0; fv < indices.size(); ++fv) {
    size_t at;
    if (interp == pxr::UsdGeomTokens->constant)
      at = 0;
    else if (interp == pxr::UsdGeomTokens->uniform)
      at = (size_t)face;
    else if (interp == pxr::UsdGeomTokens->faceVarying)
      at = fv;
    else  // vertex / varying: by point
      at = (size_t)indices[fv];
    if (!primvarIndices.empty() && at < primvarIndices.size())
      at = (size_t)primvarIndices[at];
    out.push_back(at < values.size() ? values[at] : T());
    if (++inFace >= counts[(size_t)face]) {
      inFace = 0;
      ++face;
    }
  }
  return out;
}

}  // namespace sigil::usd
