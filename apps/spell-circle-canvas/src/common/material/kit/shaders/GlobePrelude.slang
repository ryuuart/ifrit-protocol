// THE GLOBE, written once and crossed into SkSL by the core: the disc
// inscribed in the node, inverted back onto the near hemisphere, carried
// into the sphere's own frame, and read for its two hemispheres, its
// graticule and its horizon. One text, so a globe on a device and a
// globe on a raster surface are the same ball.
//
// The colour comes back premultiplied by the limb's feather, because the
// alpha the entry returns is that same feather; each target's entry is
// the one line that spells the return in its own types.
float4 globeAt(float2 p) {
  float2 middle = uResolution * 0.5;
  float R = min(middle.x, middle.y) * fill;
  float2 q = (p - middle) / R;
  float rr = dot(q, q);
  if (rr > 1.0) { return float4(0.0, 0.0, 0.0, 0.0); }
  // z = +sqrt(1 - r^2) picks the FRONT of the sphere; the back is never
  // sampled, so no depth test is needed to hide it.
  float z = sqrt(max(0.0, 1.0 - rr));
  float3 v = float3(q.x, -q.y, z);

  // The eye ray carried back into the sphere's frame: roll, then pitch,
  // then heading, each undone.
  float cr = cos(-roll), sr = sin(-roll);
  v = float3(v.x * cr - v.y * sr, v.x * sr + v.y * cr, v.z);
  float cp = cos(-pitch), sp = sin(-pitch);
  v = float3(v.x, v.y * cp - v.z * sp, v.y * sp + v.z * cp);
  float cy = cos(yaw), sy = sin(yaw);
  v = float3(v.x * cy + v.z * sy, v.y, -v.x * sy + v.z * cy);

  float lat = asin(clamp(v.y, -1.0, 1.0));
  float lon = atan2(v.x, v.z);
  float px = 1.0 / R;

  float hemi = smoothstep(-horizonBlend, horizonBlend, v.y);
  float3 above = lerp(sky.rgb, skyPole.rgb, clamp(lat / 1.5707963, 0.0, 1.0));
  float3 below =
      lerp(ground.rgb, groundPole.rgb, clamp(-lat / 1.5707963, 0.0, 1.0));
  float3 col = lerp(below, above, hemi);

  // A rule is the distance to a PLANE: a meridian's plane stands through
  // the poles at its own longitude, a parallel's lies at its own sine.
  // The crowding toward the limb and toward the poles is then exact.
  float fine = minorDeg * 0.017453292;
  float fineLon = floor(lon / fine + 0.5) * fine;
  float dFineMeridian = abs(v.x * cos(fineLon) - v.z * sin(fineLon));
  float fineLat = floor(lat / fine + 0.5) * fine;
  float dFineParallel = abs(v.y - sin(fineLat));
  float meridianStep = meridianDeg * 0.017453292;
  float heavyLon = floor(lon / meridianStep + 0.5) * meridianStep;
  float dMeridian = abs(v.x * cos(heavyLon) - v.z * sin(heavyLon));
  float parallelStep = parallelDeg * 0.017453292;
  float heavyLat = floor(lat / parallelStep + 0.5) * parallelStep;
  float dParallel = abs(v.y - sin(heavyLat));

  float w = lineWidth * px;
  float minor = max(1.0 - smoothstep(w, w * 2.4, dFineMeridian),
                    1.0 - smoothstep(w, w * 2.4, dFineParallel));
  float major = max(1.0 - smoothstep(w * 1.5, w * 3.4, dMeridian),
                    1.0 - smoothstep(w * 1.5, w * 3.4, dParallel));
  float horizon = 1.0 - smoothstep(w * 1.7, w * 3.6, abs(v.y));

  col = lerp(col, grid.rgb,
             clamp(minor * minorWeight + major * majorWeight, 0.0, 1.0));
  col = lerp(col, grid.rgb, horizon * horizonWeight);

  // The ball: what a point keeps at the limb plus what it gains facing
  // the eye, and one specular from the named direction.
  col *= (ambient + diffuse * z);
  float3 L = normalize(light.xyz);
  float lit = pow(max(0.0, dot(L, float3(q.x, -q.y, z))), shininess);
  col += lit * specular;

  float edge = 1.0 - smoothstep(1.0 - edgeFeather * px, 1.0, sqrt(rr));
  return float4(col * edge, edge);
}
