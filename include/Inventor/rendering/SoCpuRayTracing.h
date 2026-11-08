#ifndef COIN_SOCPURAYTRACING_H
#define COIN_SOCPURAYTRACING_H

#include <Inventor/SbBasic.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbString.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec4f.h>
#include <cstdint>
#include <vector>

// Value-only diagnostics for the experimental synchronous CPU main-surface path.
// Spatial data uses scene units. Source light positions/directions are view-space;
// model maps object-local coordinates into Coin/FreeCAD world coordinates.
struct SoCpuRayTracingLight {
  int type = 0;
  SbVec3f color, direction, position, attenuation;
  float spotCutoffCos = -1.0f, spotExponent = 0.0f;
  SoCpuRayTracingLight() { color.setValue(0,0,0); direction.setValue(0,0,0); position.setValue(0,0,0); attenuation.setValue(0,0,0); }
};

struct SoCpuRayTracingElementRange {
  int type = 0, elementIndex = -1;
  uint32_t drawStart = 0, drawCount = 0;
};

struct SoCpuRayTracingSurfaceInfo {
  uint32_t originalCommandIndex = 0, sourceLightingHandle = 0;
  uint64_t nodeId = 0, instanceId = 0, objectId = 0;
  SbMatrix model;
  SbVec4f ambient, diffuse, specular, emissive;
  float shininess = 0.0f, opacity = 1.0f;
  int sourceShadingModel = 0, materialIndex = 0;
  bool hadProducerUserData = false;
  bool hasLightingSetup = false;
  SbVec3f ambientLighting;
  std::vector<SoCpuRayTracingLight> lights;
  std::vector<SoCpuRayTracingElementRange> elementRanges;
  SoCpuRayTracingSurfaceInfo() {
    model.makeIdentity();
    ambient.setValue(0, 0, 0, 1); diffuse.setValue(0, 0, 0, 1);
    specular.setValue(0, 0, 0, 1); emissive.setValue(0, 0, 0, 1);
    ambientLighting.setValue(0, 0, 0);
  }
};

struct SoCpuRayTracingResult {
  enum Status { DISABLED = 0, RENDERED = 1, FALLBACK = 2, FAILED = 3 };
  Status status = DISABLED;
  SbString detail;
  SbString lightingPolicy;
  uint64_t frameSerial = 0, cameraNodeId = 0, contentRevision = 0;
  uint64_t triangleCount = 0, hitCount = 0, rayTests = 0;
  uint32_t generation = 0, contextId = 0;
  int framebuffer = 0, samples = 0, viewportX = 0, viewportY = 0;
  int width = 0, height = 0;
  float devicePixelRatio = 1.0f;
  bool readbackValid = false;
  bool framebufferSrgbEnabled = false;
  SbMatrix viewMatrix, projectionMatrix;
  // Bottom-up, tightly packed, linear RGB8, window-depth float32 and coverage.
  std::vector<unsigned char> rgb, coverage, readbackRgb;
  std::vector<float> depth, readbackDepth;
  std::vector<SoCpuRayTracingSurfaceInfo> sources;
  SoCpuRayTracingResult() {
    viewMatrix.makeIdentity(); projectionMatrix.makeIdentity();
    lightingPolicy = "DIAGNOSTIC_IGNORE_RETAINED_LIGHTING";
  }
};

// Owned, application-independent summary of one retained command. This is a
// diagnostic value type; it does not transfer geometry or DrawList lifetime.
struct SoRetainedCommandDiagnostic {
  uint32_t commandIndex = 0, sourceLightingHandle = 0;
  uint64_t nodeId = 0, instanceId = 0, objectId = 0;
  uint32_t topology = 0, vertexCount = 0, normalCount = 0, indexCount = 0;
  uint64_t positionHash = 0, normalHash = 0, vertexColorHash = 0;
  SbMatrix model;
  SbVec4f ambient, diffuse, specular, emissive, firstVertexColor;
  SbVec3f normalMin, normalMax, normalSum, ambientLighting;
  float shininess = 0.0f, opacity = 1.0f;
  int sourceShadingModel = 0, materialIndex = 0, stage = 0;
  bool hasPositions = false, hasNormals = false, hasVertexColors = false;
  bool hasLightingSetup = false, rasterVisible = false;
  bool twoSidedLighting = false;
  bool cullBackFaces = false, frontFaceCCW = true;
  std::vector<SoCpuRayTracingLight> lights;
  std::vector<SoCpuRayTracingElementRange> elementRanges;
  SoRetainedCommandDiagnostic() {
    model.makeIdentity();
    ambient.setValue(0, 0, 0, 1); diffuse.setValue(0, 0, 0, 1);
    specular.setValue(0, 0, 0, 1); emissive.setValue(0, 0, 0, 1);
    firstVertexColor.setValue(0, 0, 0, 0);
    normalMin.setValue(0, 0, 0); normalMax.setValue(0, 0, 0);
    normalSum.setValue(0, 0, 0); ambientLighting.setValue(0, 0, 0);
  }
};

struct SoRetainedCommandDiagnostics {
  enum Status { UNAVAILABLE = 0, READY = 1, INVALID_LIMIT = 2,
                LIMIT_EXCEEDED = 3, INVALID_BORROWED_DATA = 4 };
  Status status = UNAVAILABLE;
  SbString detail;
  uint32_t generation = 0;
  uint64_t contentRevision = 0;
  std::vector<SoRetainedCommandDiagnostic> commands;
};

#endif
