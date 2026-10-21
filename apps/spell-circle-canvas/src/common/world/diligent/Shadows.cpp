/** @file
 * The shadow stage: every casting light's maps laid out in one depth
 * atlas, each drawn from the bodies that cast and that its shadow link
 * keeps — and a map nothing moved in kept from the frame before.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/advanced/Light.h>

#include <algorithm>
#include <glm/vec4.hpp>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "GeometryPass.h"

namespace sigil::world::diligent {

namespace {

/** WHETHER @p body BLOCKS LIGHT: it says it casts, it is lit, and it is
 *  laid over writing depth. A body that adds, multiplies, screens, fades or
 *  writes no depth, one that sees through itself, and one that is its own
 *  light cast nothing. */
bool casts(const Draw& body) {
  if (!body.mesh || !body.castShadow || !body.lit) return false;
  if (seesThrough(body, false)) return false;
  const Blending blending = blendingOf(body, body.baseColor, false);
  if (blended(body, blending, false)) return false;
  return blending.depthWrite;
}

/** WHETHER @p body's SURFACE DECIDES WHERE IT COVERS: it is dressed with a
 *  map, whose alpha may cut it out, or with an opacity map. */
bool coverageOf(const Draw& body) {
  if (body.texture) return true;
  return body.material &&
         material::surface::map(*body.material,
                                material::surface::kOpacitySlot) != nullptr;
}

/** WHETHER @p box CAN REACH THE TILE a map with @p viewProjection draws
 *  into: some part of it stands inside the map's clip volume. A box every
 *  corner of which stands beyond one of the volume's six planes draws no
 *  depth there, since the device clips everything drawn to that volume;
 *  the corners are tested in clip space, where a plane is linear in them,
 *  so a box behind a perspective map's eye is answered as exactly as one
 *  in front of it. */
bool reaches(const glm::mat4& viewProjection, const Box& box) {
  if (box.empty()) return false;
  glm::vec4 corners[8];
  for (int i = 0; i < 8; ++i)
    corners[i] = viewProjection * glm::vec4(box.corner(i), 1.0f);
  const auto allBeyond = [&](auto&& beyond) {
    for (const glm::vec4& corner : corners)
      if (!beyond(corner)) return false;
    return true;
  };
  return !(allBeyond([](const glm::vec4& c) { return c.x < -c.w; }) ||
           allBeyond([](const glm::vec4& c) { return c.x > c.w; }) ||
           allBeyond([](const glm::vec4& c) { return c.y < -c.w; }) ||
           allBeyond([](const glm::vec4& c) { return c.y > c.w; }) ||
           allBeyond([](const glm::vec4& c) { return c.z < 0.0f; }) ||
           allBeyond([](const glm::vec4& c) { return c.z > c.w; }));
}

/** WHAT EACH CASTER WHOSE SURFACE DECIDES WHERE IT COVERS IS WEARING, as
 *  a number per body that stays the same while the surface and its map
 *  are the same values and no texture pass painted a page it wears, and
 *  changes otherwise; zero for every other body. */
std::vector<uint64_t> coverageWorn(Gpu& gpu, const View& view) {
  std::vector<uint64_t> worn(view.draws.size(), 0);
  gpu.shadowWorn.resize(view.draws.size());
  for (size_t i = 0; i < view.draws.size(); ++i) {
    const Draw& body = view.draws[i];
    Gpu::CoverageWorn& was = gpu.shadowWorn[i];
    if (!casts(body) || !coverageOf(body)) {
      was = {};
      continue;
    }
    const bool sameMaterial =
        body.material ? was.material && *was.material == *body.material
                      : !was.material;
    const bool sameTexture = body.texture
                                 ? was.texture && *was.texture == *body.texture
                                 : !was.texture;
    if (was.generation == 0 || body.repainted || !sameMaterial ||
        !sameTexture) {
      was.material = body.material
                         ? std::optional<material::Material>(*body.material)
                         : std::nullopt;
      was.texture = body.texture
                        ? std::optional<material::Texture>(*body.texture)
                        : std::nullopt;
      was.generation = ++gpu.shadowWornGenerations;
    }
    worn[i] = was.generation;
  }
  return worn;
}

/** WHAT ONE MAP'S DEPTH IS A FUNCTION OF: where it stands in the atlas
 *  and how it projects, and every body its light's link keeps that casts
 *  and stands where the map can see it — where it stands, which cooked
 *  triangles it is, every instance of it, the eye's turn where it turns
 *  to face the eye, and, where its surface cuts it out, what that surface
 *  and its map are (@p worn) and the moment a running one is read at.
 *  Two frames folding to the same number draw the same depth into the
 *  tile, so the second keeps the first's. */
uint64_t shadowFold(const ShadowMap& map, const View& view,
                    std::span<const Box> boxes, std::span<const uint64_t> worn,
                    const glm::mat4& cameraView, double seconds) {
  Fold fold;
  fold.add(map.viewProjection);
  fold.add(map.x);
  fold.add(map.y);
  fold.add(map.size);
  const Collection& link = map.light->light.shadowLink;
  for (size_t i = 0; i < view.draws.size(); ++i) {
    const Draw& body = view.draws[i];
    if (!casts(body)) continue;
    if (!link.everything() && !link.contains(memberOf(body))) continue;
    if (!reaches(map.viewProjection, boxes[i])) continue;
    fold.add(i);
    fold.add(body.world);
    fold.add(body.geometry);
    fold.add(body.mesh);
    fold.add(body.mesh->positions.size());
    if (body.instances) {
      fold.all(body.instances->positions);
      fold.all(body.instances->orientations);
      fold.all(body.instances->scales);
    }
    if (body.facing != Facing::None) fold.add(cameraView);
    if (coverageOf(body)) {
      // A caster drawn as a stand-in is drawn again once its program
      // arrives.
      if (view.standIns && body.material)
        fold.add(
            programReady(material::inlined(*body.material), kVariantShadow));
      fold.add(worn[i]);
      fold.add(body.baseColor);
      // Only a surface that moves on its own is read at a new moment.
      if ((body.material && body.material->isRunning()) ||
          (body.texture && body.texture->animated()))
        fold.add(seconds);
    }
  }
  return fold.value;
}

/** A TILE OF CLEARED DEPTH as large as @p side, cleared once when made:
 *  what one map's tile is reset from by a copy, since a clear reaches the
 *  whole atlas and a tile kept from the frame before must survive it. */
dg::ITexture* blankTile(Gpu& gpu, int side) {
  if (gpu.shadowBlank && gpu.shadowBlankSide >= side) return gpu.shadowBlank;
  gpu.shadowBlank.Release();
  gpu.shadowBlankSide = 0;
  dg::TextureDesc desc;
  desc.Name = "world shadow blank tile";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = desc.Height = (dg::Uint32)side;
  desc.Format = kDepthFormat;
  desc.BindFlags = dg::BIND_DEPTH_STENCIL;
  desc.Usage = dg::USAGE_DEFAULT;
  gpu.device->renderDevice()->CreateTexture(desc, nullptr, &gpu.shadowBlank);
  if (!gpu.shadowBlank) return nullptr;
  dg::IDeviceContext* context = gpu.device->context();
  dg::ITextureView* dsv =
      gpu.shadowBlank->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL);
  context->SetRenderTargets(0, nullptr, dsv,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearDepthStencil(dsv, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                             dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  gpu.shadowBlankSide = side;
  return gpu.shadowBlank;
}

}  // namespace

/** THE SHADOW STAGE: every casting light's map drawn into the atlas, each
 *  in its own tile, from the bodies that cast and that its shadow link
 *  keeps. Draws nothing, and binds nothing afterwards, in a pass under no
 *  light that casts.
 *
 *  A MAP NOTHING MOVED IN IS KEPT. The atlas stands from frame to frame,
 *  and a tile is redrawn only when what its depth is a function of
 *  changed — its light, its fit, or a body that casts into it; a body
 *  standing where a map cannot see it neither casts into that map nor
 *  is drawn there. A surface that cuts its caster out is folded as the
 *  value it is, so the same surface kept from frame to frame keeps its
 *  maps, and is read at a new moment only where it runs on its own or a
 *  page it wears was painted again. Answers how many maps it drew. */
int64_t drawShadows(Gpu& gpu, const View& view, const glm::mat4& cameraView) {
  gpu.shadows.clear();
  const std::vector<size_t> chosen =
      shadowCasters(std::span<const PlacedLight>(view.lights));
  if (chosen.empty()) return 0;
  Box scene;
  std::vector<Box> boxes(view.draws.size());
  for (size_t i = 0; i < view.draws.size(); ++i) {
    boxes[i] = boxOf(view.draws[i]);
    scene.add(boxes[i]);
  }
  if (scene.empty()) return 0;

  // EVERY TILE THE SIZE OF THE LARGEST MAP ASKED FOR, in a square grid
  // holding one tile per map — six for a point light, one per cascade for
  // a directional light — and no more than
  // kAtlasSide texels a side: a frame holding more maps than that fits at
  // the size asked for draws every one of them smaller. A map asking for
  // less than its tile draws into the tile's corner.
  constexpr int kAtlasSide = 4096;
  int tile = 64;
  size_t count = 0;
  for (const size_t index : chosen) {
    const PlacedLight& light = view.lights[index];
    tile = std::max(tile, std::clamp(light.light.shadow.mapSize, 64, 2048));
    count += light.light.kind == LightKind::Point ||
                     light.light.kind == LightKind::Sphere ||
                     light.light.kind == LightKind::Rect ||
                     light.light.kind == LightKind::Disk
                 ? 6
             : light.light.kind == LightKind::Directional
                 ? (size_t)std::clamp(light.light.shadow.cascades, 1, 4)
                 : 1;
  }
  int across = 1;
  while ((size_t)(across * across) < count) ++across;
  tile = std::min(tile, kAtlasSide / across);
  dg::ITexture* atlas = gpu.shadowAtlasAt(tile * across);
  if (!atlas) return 0;
  const auto place = [&](ShadowMap map) {
    const int at = (int)gpu.shadows.size();
    map.x = (at % across) * tile;
    map.y = (at / across) * tile;
    gpu.shadows.push_back(map);
  };
  for (const size_t index : chosen) {
    const PlacedLight& light = view.lights[index];
    const int size = std::clamp(light.light.shadow.mapSize, 64, tile);
    // A light with a size casts from its centre through six maps, so a
    // panel's whole front half is covered: a sphere's along the world's
    // axes as a point light's, a rect's or a disk's turned to its face.
    if (light.light.kind == LightKind::Point ||
        light.light.kind == LightKind::Sphere) {
      for (const ShadowMap& face :
           fitPoint(light, scene, size, glm::mat3(1.0f)))
        place(face);
    } else if (light.light.kind == LightKind::Rect ||
               light.light.kind == LightKind::Disk) {
      const glm::vec3 ahead = glm::length(light.light.direction) > 1e-5f
                                  ? glm::normalize(light.light.direction)
                                  : glm::vec3(0, -1, 0);
      glm::vec3 across = light.across - ahead * glm::dot(light.across, ahead);
      if (glm::length(across) < 1e-4f)
        across =
            std::abs(ahead.x) < 0.9f ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1);
      across = glm::normalize(across - ahead * glm::dot(across, ahead));
      const glm::mat3 axes(ahead, across, glm::cross(ahead, across));
      for (const ShadowMap& face : fitPoint(light, scene, size, axes))
        place(face);
    } else if (light.light.kind == LightKind::Spot) {
      place(fitSpot(light, scene, size));
    } else {
      for (const ShadowMap& slice : fitCascades(light, view, scene, size))
        place(slice);
    }
  }

  // WHICH TILES MUST BE DRAWN AGAIN. An atlas made anew, or laid out
  // anew, holds nothing worth keeping.
  const bool laidOut = atlas == gpu.shadowFoldsAtlas &&
                       gpu.shadowFolds.size() == gpu.shadows.size();
  const std::vector<uint64_t> worn = coverageWorn(gpu, view);
  std::vector<uint64_t> folds(gpu.shadows.size());
  std::vector<bool> redraw(gpu.shadows.size(), !laidOut);
  size_t redrawn = 0;
  for (size_t at = 0; at < gpu.shadows.size(); ++at) {
    folds[at] = shadowFold(gpu.shadows[at], view, boxes, worn, cameraView,
                           gpu.seconds);
    if (laidOut && folds[at] != gpu.shadowFolds[at]) redraw[at] = true;
    redrawn += redraw[at] ? 1 : 0;
  }
  gpu.shadowFolds = std::move(folds);
  gpu.shadowFoldsAtlas = atlas;
  if (redrawn == 0) return 0;

  dg::IDeviceContext* context = gpu.device->context();
  dg::ITextureView* dsv = atlas->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL);
  dg::ITexture* blank =
      redrawn == gpu.shadows.size() ? nullptr : blankTile(gpu, tile);
  if (blank) {
    // Each tile drawn again is reset from the blank one; every other tile
    // keeps the depth the frame before drew.
    for (size_t at = 0; at < gpu.shadows.size(); ++at) {
      if (!redraw[at]) continue;
      dg::Box whole;
      whole.MaxX = whole.MaxY = (dg::Uint32)tile;
      dg::CopyTextureAttribs copy(
          blank, dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, atlas,
          dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      copy.pSrcBox = &whole;
      copy.DstX = (dg::Uint32)gpu.shadows[at].x;
      copy.DstY = (dg::Uint32)gpu.shadows[at].y;
      context->CopyTexture(copy);
    }
    context->SetRenderTargets(0, nullptr, dsv,
                              dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  } else {
    std::fill(redraw.begin(), redraw.end(), true);
    redrawn = gpu.shadows.size();
    context->SetRenderTargets(0, nullptr, dsv,
                              dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->ClearDepthStencil(dsv, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                               dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
  const dg::Uint32 side = (dg::Uint32)gpu.shadowAtlasSide;
  for (size_t at = 0; at < gpu.shadows.size(); ++at) {
    if (!redraw[at]) continue;
    const ShadowMap& map = gpu.shadows[at];
    dg::Viewport viewport;
    viewport.TopLeftX = (float)map.x;
    viewport.TopLeftY = (float)map.y;
    viewport.Width = viewport.Height = (float)map.size;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->SetViewports(1, &viewport, side, side);
    const Collection& link = map.light->light.shadowLink;
    const int64_t pendingBefore = gpu.programsPending;
    for (size_t i = 0; i < view.draws.size(); ++i) {
      const Draw& body = view.draws[i];
      if (!casts(body)) continue;
      if (!link.everything() && !link.contains(memberOf(body))) continue;
      // A body the map cannot see draws nothing into it.
      if (!reaches(map.viewProjection, boxes[i])) continue;
      const Caster caster{map.viewProjection, coverageOf(body)};
      // A body turned to the camera keeps the turn the eye sees it at, so
      // its shadow is the shape it shows.
      drawBody(
          gpu, map.viewProjection, cameraView,
          Shape{body.geometry, body.mesh, body.instances, body.facing, -1,
                body.primitiveColor},
          body.world, body.baseColor, caster.coverage ? body.material : nullptr,
          caster.coverage ? body.texture : nullptr, {}, view.environment,
          /*lit=*/false, Blending{}, /*cull=*/false, nullptr, nullptr, &caster);
    }
    // A tile a body stood in for, or was left out of, is drawn again on
    // the next frame, whatever its depth is a function of.
    if (gpu.programsPending != pendingBefore)
      gpu.shadowFolds[at] = ~gpu.shadowFolds[at];
  }
  return (int64_t)redrawn;
}

}  // namespace sigil::world::diligent
