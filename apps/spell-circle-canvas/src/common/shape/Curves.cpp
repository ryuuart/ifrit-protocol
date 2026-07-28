#include "sigilshape/Curves.h"

#include "sigilshape/detail/VecMath.h"

#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>

namespace sigil::shape {

using detail::normalized;
using glm::cross;
using glm::dot;

namespace {

/** Rotate @p v around unit axis by angle (Rodrigues). */
glm::vec3 rotate(glm::vec3 v, glm::vec3 axis, float angle) {
  const float c = std::cos(angle), s = std::sin(angle);
  return v * c + cross(axis, v) * s + axis * (dot(axis, v) * (1 - c));
}

glm::vec3 catmullRom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2,
                     glm::vec3 p3, float t) {
  const float t2 = t * t, t3 = t2 * t;
  return (p1 * 2.0f + (p2 - p0) * t +
          (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
          (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) *
         0.5f;
}

glm::vec3 bezier(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3,
                 float t) {
  const float u = 1 - t;
  return p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) +
         p3 * (t * t * t);
}

} // namespace

glm::vec3 Spline3::position(float t) const {
  const size_t n = points.size();
  if (n == 0)
    return {0, 0, 0};
  if (n == 1)
    return points[0];
  t = std::clamp(t, 0.0f, 1.0f);

  switch (type) {
  case Type::Linear: {
    const size_t segments = closed ? n : n - 1;
    const float f = t * (float)segments;
    const size_t i = std::min((size_t)f, segments - 1);
    const float local = f - (float)i;
    const glm::vec3 a = points[i % n];
    const glm::vec3 b = points[(i + 1) % n];
    return a + (b - a) * local;
  }
  case Type::Bezier: {
    // 3n+1 layout: anchor, out-handle, in-handle, anchor, ...
    const size_t segments = std::max<size_t>(1, (n - 1) / 3);
    const float f = t * (float)segments;
    const size_t i = std::min((size_t)f, segments - 1);
    const float local = f - (float)i;
    const size_t base = i * 3;
    auto at = [&](size_t k) { return points[std::min(k, n - 1)]; };
    return bezier(at(base), at(base + 1), at(base + 2), at(base + 3),
                  local);
  }
  case Type::CatmullRom:
  default: {
    const size_t segments = closed ? n : n - 1;
    const float f = t * (float)segments;
    const size_t i = std::min((size_t)f, segments - 1);
    const float local = f - (float)i;
    auto at = [&](long k) -> glm::vec3 {
      if (closed)
        return points[(size_t)(((k % (long)n) + (long)n) % (long)n)];
      return points[(size_t)std::clamp(k, 0L, (long)n - 1)];
    };
    return catmullRom(at((long)i - 1), at((long)i), at((long)i + 1),
                      at((long)i + 2), local);
  }
  }
}

glm::vec3 Spline3::tangent(float t) const {
  const float eps = 1e-3f;
  const glm::vec3 a = position(std::max(t - eps, 0.0f));
  const glm::vec3 b = position(std::min(t + eps, 1.0f));
  return normalized(b - a, {0, 0, 1});
}

float Spline3::length(int samples) const {
  samples = std::max(samples, 2);
  float total = 0;
  glm::vec3 prev = position(0);
  for (int i = 1; i <= samples; ++i) {
    const glm::vec3 p = position((float)i / (float)samples);
    total += glm::length(p - prev);
    prev = p;
  }
  return total;
}

std::vector<glm::vec3> Spline3::sample(int count) const {
  std::vector<glm::vec3> out;
  count = std::max(count, 2);
  out.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    out.push_back(position((float)i / (float)(count - 1)));
  return out;
}

std::vector<glm::vec3> Spline3::sampleArcLength(int count) const {
  count = std::max(count, 2);
  // Arc-length table over a dense parameter sweep, then invert.
  const int dense = std::max(count * 8, 256);
  std::vector<float> arc((size_t)dense + 1, 0.0f);
  glm::vec3 prev = position(0);
  for (int i = 1; i <= dense; ++i) {
    const glm::vec3 p = position((float)i / (float)dense);
    arc[(size_t)i] = arc[(size_t)i - 1] + glm::length(p - prev);
    prev = p;
  }
  const float total = arc.back();
  std::vector<glm::vec3> out;
  out.reserve((size_t)count);
  size_t cursor = 0;
  for (int i = 0; i < count; ++i) {
    const float target =
        total * (float)i / (float)(count - 1);
    while (cursor + 1 < arc.size() && arc[cursor + 1] < target)
      ++cursor;
    const float span = arc[cursor + 1] - arc[cursor];
    const float local = span < 1e-9f ? 0 : (target - arc[cursor]) / span;
    const float t = ((float)cursor + local) / (float)dense;
    out.push_back(position(t));
  }
  return out;
}

namespace curves {

std::vector<Frame3> frames(const Spline3 &spline, int count,
                           glm::vec3 up) {
  std::vector<Frame3> out;
  count = std::max(count, 2);
  out.reserve((size_t)count);

  // Arc-length parameter stops.
  const int dense = std::max(count * 8, 256);
  std::vector<float> arc((size_t)dense + 1, 0.0f);
  glm::vec3 prev = spline.position(0);
  for (int i = 1; i <= dense; ++i) {
    const glm::vec3 p = spline.position((float)i / (float)dense);
    arc[(size_t)i] = arc[(size_t)i - 1] + glm::length(p - prev);
    prev = p;
  }
  const float total = arc.back();
  size_t cursor = 0;
  std::vector<float> stops;
  stops.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const float target = total * (float)i / (float)(count - 1);
    while (cursor + 1 < arc.size() && arc[cursor + 1] < target)
      ++cursor;
    const float span = arc[cursor + 1] - arc[cursor];
    const float local = span < 1e-9f ? 0 : (target - arc[cursor]) / span;
    stops.push_back(((float)cursor + local) / (float)dense);
  }

  // Initial frame: normal = up projected off the tangent.
  Frame3 frame;
  frame.t = stops[0];
  frame.position = spline.position(stops[0]);
  frame.tangent = spline.tangent(stops[0]);
  glm::vec3 normal =
      up - frame.tangent * dot(up, frame.tangent);
  normal = normalized(normal,
                      std::abs(frame.tangent.y) < 0.9f
                          ? cross(cross(frame.tangent, glm::vec3{0, 1, 0}),
                                  frame.tangent)
                          : glm::vec3{1, 0, 0});
  frame.normal = normal;
  frame.binormal = cross(frame.tangent, frame.normal);
  out.push_back(frame);

  // Parallel transport: rotate the previous normal by the minimal
  // rotation between consecutive tangents.
  for (int i = 1; i < count; ++i) {
    Frame3 next;
    next.t = stops[(size_t)i];
    next.position = spline.position(next.t);
    next.tangent = spline.tangent(next.t);
    const Frame3 &last = out.back();
    glm::vec3 axis = cross(last.tangent, next.tangent);
    const float axisLen = glm::length(axis);
    glm::vec3 transported = last.normal;
    if (axisLen > 1e-9f) {
      axis = axis * (1.0f / axisLen);
      const float angle = std::atan2(
          axisLen,
          std::clamp(dot(last.tangent, next.tangent), -1.0f, 1.0f));
      transported = rotate(last.normal, axis, angle);
    }
    // Re-orthogonalize against drift.
    transported =
        transported - next.tangent * dot(transported, next.tangent);
    next.normal = normalized(transported, last.normal);
    next.binormal = cross(next.tangent, next.normal);
    out.push_back(next);
  }

  // Closed loops: distribute the end-to-start twist so ring 0 == ring N.
  if (spline.closed && count > 2) {
    const Frame3 &first = out.front();
    const Frame3 &last = out.back();
    // Angle between last.normal and first.normal about last.tangent.
    const glm::vec3 ref =
        first.normal - last.tangent * dot(first.normal, last.tangent);
    const float refLen = glm::length(ref);
    if (refLen > 1e-6f) {
      const glm::vec3 refN = ref * (1.0f / refLen);
      const float cosA = std::clamp(dot(last.normal, refN), -1.0f, 1.0f);
      const float sinA = dot(cross(last.normal, refN), last.tangent);
      const float twist = std::atan2(sinA, cosA);
      for (int i = 1; i < count; ++i) {
        Frame3 &f = out[(size_t)i];
        const float share = twist * (float)i / (float)(count - 1);
        f.normal = normalized(rotate(f.normal, f.tangent, share),
                              f.normal);
        f.binormal = cross(f.tangent, f.normal);
      }
    }
  }
  return out;
}

Mesh tube(const Spline3 &spline, const TubeOptions &options) {
  Mesh out;
  const int segments = std::max(options.segments, 2);
  const int sides = std::max(options.sides, 3);
  const std::vector<Frame3> rail = frames(spline, segments, options.up);

  for (int i = 0; i < segments; ++i) {
    const Frame3 &f = rail[(size_t)i];
    const float r =
        options.radius *
        (options.profile ? std::max(options.profile(f.t), 0.0f) : 1.0f);
    for (int s = 0; s <= sides; ++s) { // seam duplicated for clean UVs
      const float a = (float)s / (float)sides * 2.0f * (float)M_PI;
      const glm::vec3 dir =
          f.normal * std::cos(a) + f.binormal * std::sin(a);
      out.positions.push_back(f.position + dir * r);
      out.normals.push_back(dir);
      out.uvs.push_back({(float)s / (float)sides, f.t});
    }
  }
  const int ring = sides + 1;
  for (int i = 0; i + 1 < segments; ++i)
    for (int s = 0; s < sides; ++s) {
      const uint32_t a = (uint32_t)(i * ring + s);
      const uint32_t b = a + 1;
      const uint32_t c = a + (uint32_t)ring;
      const uint32_t d = c + 1;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }

  if (options.caps && !spline.closed) {
    for (int end = 0; end < 2; ++end) {
      const Frame3 &f = rail[end == 0 ? 0 : rail.size() - 1];
      const glm::vec3 n = end == 0 ? f.tangent * -1.0f : f.tangent;
      const uint32_t center = (uint32_t)out.positions.size();
      out.positions.push_back(f.position);
      out.normals.push_back(n);
      out.uvs.push_back({0.5f, end == 0 ? 0.0f : 1.0f});
      const uint32_t ringStart =
          (uint32_t)((end == 0 ? 0 : segments - 1) * ring);
      for (int s = 0; s < sides; ++s) {
        const uint32_t a = ringStart + (uint32_t)s;
        const uint32_t b = ringStart + (uint32_t)s + 1;
        if (end == 0)
          out.indices.insert(out.indices.end(), {center, b, a});
        else
          out.indices.insert(out.indices.end(), {center, a, b});
      }
    }
  }
  return out;
}

Mesh ribbon(const Spline3 &spline, const RibbonOptions &options) {
  Mesh out;
  const int segments = std::max(options.segments, 2);
  const std::vector<Frame3> rail = frames(spline, segments, options.up);
  for (int i = 0; i < segments; ++i) {
    const Frame3 &f = rail[(size_t)i];
    const float half =
        options.width * 0.5f *
        (options.profile ? std::max(options.profile(f.t), 0.0f) : 1.0f);
    out.positions.push_back(f.position - f.binormal * half);
    out.positions.push_back(f.position + f.binormal * half);
    out.normals.push_back(f.normal);
    out.normals.push_back(f.normal);
    out.uvs.push_back({0, f.t});
    out.uvs.push_back({1, f.t});
  }
  for (int i = 0; i + 1 < segments; ++i) {
    const uint32_t a = (uint32_t)(i * 2);
    out.indices.insert(out.indices.end(),
                       {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  return out;
}

Mesh banner(const Spline3 &spline, const BannerOptions &options) {
  Mesh out;
  const int sections = std::max(options.sections, 2);
  const float half = options.width * 0.5f;
  const auto wrap01 = [](float t) { return t - std::floor(t); };
  glm::vec3 hang = {0, -1, 0}; // carried through vertical stretches
  for (int i = 0; i < sections; ++i) {
    const float f = (float)i / (float)(sections - 1);
    const float t =
        wrap01(options.head - options.span + options.span * f);
    const glm::vec3 p = spline.position(t);
    glm::vec3 tangent = spline.position(wrap01(t + 0.002f)) -
                        spline.position(wrap01(t - 0.002f));
    const float len = glm::length(tangent);
    if (len > 1e-6f)
      tangent = tangent * (1.0f / len);
    glm::vec3 down = glm::vec3{0, -1, 0} -
                     tangent * dot(tangent, glm::vec3{0, -1, 0});
    const float downLen = glm::length(down);
    if (downLen > 0.15f)
      hang = down * (1.0f / downLen);
    const glm::vec3 normal = cross(hang, tangent);
    out.positions.push_back(p - hang * half); // u = 0: top edge
    out.positions.push_back(p + hang * half);
    out.normals.push_back(normal);
    out.normals.push_back(normal);
    out.uvs.push_back({0, f});
    out.uvs.push_back({1, f});
  }
  for (int i = 0; i + 1 < sections; ++i) {
    const uint32_t a = (uint32_t)(i * 2);
    out.indices.insert(out.indices.end(),
                       {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  return out;
}

SkPath project(const Spline3 &spline, const space::Camera &camera,
               SkSize viewport, int samples) {
  SkPathBuilder out;
  samples = std::max(samples, 2);
  const glm::mat4 vp = camera.viewProjection(viewport);
  bool penDown = false;
  for (int i = 0; i < samples; ++i) {
    float t = (float)i / (float)(samples - 1);
    if (spline.closed)
      t = (float)i / (float)samples; // wrap-friendly spacing
    const glm::vec3 p = spline.position(t);
    const glm::vec4 clip = vp * glm::vec4{p, 1};
    if (clip.w <= 1e-4f) {
      penDown = false;
      continue;
    }
    const SkPoint screen = {clip.x / clip.w, clip.y / clip.w};
    if (!penDown) {
      out.moveTo(screen);
      penDown = true;
    } else {
      out.lineTo(screen);
    }
  }
  if (spline.closed && penDown)
    out.close();
  return out.detach();
}

} // namespace curves

} // namespace sigil::shape
