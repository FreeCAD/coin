#ifndef COIN_SORETAINEDCOMMANDDIAGNOSTICS_H
#define COIN_SORETAINEDCOMMANDDIAGNOSTICS_H

#include <Inventor/SbMatrix.h>
#include <Inventor/SbString.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbVec4f.h>
#include <Inventor/rendering/SoCpuRayTracing.h>

#include <cstdint>
#include <vector>

// Experimental, application-independent summary of one retained command.
// This value type owns its summary data; it does not transfer geometry or
// DrawList lifetime. The diagnostic API may change while the retained renderer
// remains experimental.
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
