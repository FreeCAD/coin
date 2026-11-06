#ifndef COIN_SOCPURAYOWNEDCOMPAREP_H
#define COIN_SOCPURAYOWNEDCOMPAREP_H

#include "rendering/SoCpuRayOwnedSnapshotP.h"
#include <Inventor/SbDPViewVolume.h>
#include <Inventor/SbVec2d.h>
#include <Inventor/SbVec3d.h>
#include <tuple>

namespace coin_ray_owned_compare {

inline bool volume(const SbViewVolume & a, const SbViewVolume & b) {
  const auto & x = a.getDPViewVolume();
  const auto & y = b.getDPViewVolume();
  // Width/height/up do not encode asymmetric near-plane offsets. Compare
  // three independent near/far ray pairs before allowing a completed image.
  for (const auto & corner : {SbVec2d(0, 0), SbVec2d(1, 0), SbVec2d(0, 1)}) {
    SbVec3d xn, xf, yn, yf;
    x.projectPointToLine(corner, xn, xf);
    y.projectPointToLine(corner, yn, yf);
    if (xn != yn || xf != yf) return false;
  }
  return x.getProjectionType() == y.getProjectionType() &&
    x.getProjectionPoint() == y.getProjectionPoint() &&
    x.getProjectionDirection() == y.getProjectionDirection() &&
    x.getNearDist() == y.getNearDist() && x.getDepth() == y.getDepth() &&
    x.getWidth() == y.getWidth() && x.getHeight() == y.getHeight() &&
    x.getViewUp() == y.getViewUp();
}

inline auto material(const retained_bridge::Material & m) {
  return std::tie(m.ambient, m.diffuse, m.specular, m.emissive, m.shininess,
    m.opacity, m.shadingModel, m.twoSidedLighting, m.textureAlphaIncludesOpacity,
    m.vertexColorAlphaIncludesOpacity);
}

inline auto raster(const SoRasterState & r) {
  return std::tie(r.visible, r.fillMode, r.cullBackFaces, r.frontFaceCCW,
    r.scissorEnabled, r.viewportOverride, r.viewportEnabled, r.viewportX,
    r.viewportY, r.viewportWidth, r.viewportHeight, r.lineWidth, r.pointSize,
    r.linePattern, r.linePatternScale, r.polygonOffsetFactor,
    r.polygonOffsetUnits, r.polygonOffsetFilled, r.polygonOffsetLines,
    r.polygonOffsetPoints);
}

inline bool state(const SoRenderState & a, const SoRenderState & b) {
  return std::tie(a.depth.enabled, a.depth.writeEnabled, a.depth.func, a.depth.range,
      a.blend.enabled, a.blend.srcRGBFactor, a.blend.dstRGBFactor,
      a.blend.srcAlphaFactor, a.blend.dstAlphaFactor, a.blend.rgbEquation,
      a.blend.alphaEquation, a.alphaTest.policy, a.alphaTest.function,
      a.alphaTest.reference, a.useCommandMatrices) ==
    std::tie(b.depth.enabled, b.depth.writeEnabled, b.depth.func, b.depth.range,
      b.blend.enabled, b.blend.srcRGBFactor, b.blend.dstRGBFactor,
      b.blend.srcAlphaFactor, b.blend.dstAlphaFactor, b.blend.rgbEquation,
      b.blend.alphaEquation, b.alphaTest.policy, b.alphaTest.function,
      b.alphaTest.reference, b.useCommandMatrices) && raster(a.raster) == raster(b.raster);
}

// Both frames must first pass retained_bridge::capture. This is an exact
// comparison of current diagnostic image inputs, not a producer-hash shortcut.
// Provenance revisions/handles and native selection/lighting metadata do not
// alter the fixed diagnostic shader; current-pass metadata is published anew.
inline bool same(const retained_bridge::OwnedFrame & a,
                 const retained_bridge::OwnedFrame & b) {
  const auto & x = a.envelope;
  const auto & y = b.envelope;
  if (a.triangleCount != b.triangleCount || a.resources.size() != b.resources.size() ||
      a.commands.size() != b.commands.size() || x.sceneUnits != y.sceneUnits ||
      x.coordinateFrame != y.coordinateFrame || x.contractVersion != y.contractVersion ||
      x.devicePixelRatio != y.devicePixelRatio || x.viewport != y.viewport ||
      x.viewMatrix != y.viewMatrix || x.projectionMatrix != y.projectionMatrix ||
      x.allowProducerUserDataOmission != y.allowProducerUserDataOmission ||
      !x.diagnosticIgnoreRetainedLighting || !y.diagnosticIgnoreRetainedLighting ||
      !volume(x.cameraVolume, y.cameraVolume)) return false;
  // Shared resource arrays are compared once, even with thousands of instances.
  for (size_t i = 0; i < a.resources.size(); ++i) {
    const auto & l = a.resources[i]; const auto & r = b.resources[i];
    if (l.positions != r.positions || l.normals != r.normals || l.colors != r.colors ||
        l.texcoords != r.texcoords || l.indices != r.indices) return false;
  }
  for (size_t i = 0; i < a.commands.size(); ++i) {
    const auto & l = a.commands[i]; const auto & r = b.commands[i];
    if (l.commandIndex != r.commandIndex || l.resourceId != r.resourceId ||
        l.model != r.model || l.view != r.view || l.projection != r.projection ||
        l.materialIndex != r.materialIndex || l.stage != r.stage ||
        l.usesCommandMatrices != r.usesCommandMatrices ||
        material(l.material) != material(r.material) || !state(l.state, r.state)) return false;
  }
  return true;
}
} // namespace coin_ray_owned_compare
#endif
