#ifndef COIN_RETAINED_ADAPTER_SNAPSHOT_H
#define COIN_RETAINED_ADAPTER_SNAPSHOT_H

#include <Inventor/rendering/SoRenderIR.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coin_raytrace { struct Snapshot; }

namespace retained_bridge {

using Vec3 = std::array<double, 3>;
using Vec4 = std::array<double, 4>;
using Matrix4 = std::array<std::array<double, 4>, 4>;

enum class Code {
  OK, INVALID_ENVELOPE, STALE_FRAME, INVALID_LIMIT, COMMAND_LIMIT,
  RESOURCE_LIMIT, VALUE_LIMIT, TRIANGLE_LIMIT, BYTE_LIMIT, TIME_LIMIT,
  CANCELLED, UNSUPPORTED_STATE, INVALID_GEOMETRY, INVALID_MATERIAL,
  INVALID_TRANSFORM, CAMERA_MISMATCH, INVALID_METADATA, ALLOCATION_FAILURE,
  CAPTURE_FAILURE
};

struct Status {
  Code code = Code::OK;
  std::string detail;
};
const char * codeString(Code code);

struct Limits {
  std::size_t maxCommands = 128;
  std::size_t maxResources = 32;
  std::size_t maxVertices = 16384;
  std::size_t maxIndices = 49152;
  std::size_t maxTriangles = 4096;
  std::size_t maxElementRanges = 4096;
  std::size_t maxSelectionTargets = 128;
  std::size_t maxBytes = 8 * 1024 * 1024;
  int maxMilliseconds = 2000;
  double maxMagnitude = 1000000.0;
  // Optional non-owned token; its lifetime covers this synchronous call.
  const std::atomic_bool * cancellation = nullptr;
  int maxWidth = 2048, maxHeight = 2048;
  std::size_t maxPixels = 1048576;
};

struct Envelope {
  // Coin's SbViewVolume/SbMatrix default constructors leave numeric fields
  // uninitialized. This safe placeholder is deliberately inadmissible (near=0)
  // until the caller supplies a consistent fixture camera.
  Envelope() {
    cameraVolume.ortho(-1, 1, -1, 1, 0, 1);
    viewMatrix.makeIdentity();
    projectionMatrix.makeIdentity();
  }
  std::string sourceCommit = "58d328fe777a01f354160d283279f0a9148a540a";
  std::string sceneUnits = "scene_unit";
  std::string coordinateFrame = "Coin world";
  std::uint32_t contractVersion = 1;
  std::uint64_t captureSerial = 1;
  std::uint32_t expectedGeneration = 0;
  std::uint64_t expectedContentRevision = 0;
  double devicePixelRatio = 1.0;
  // Caller attests this known producer fixture uses userData only as opaque
  // identity metadata whose omission is deliberate. No pointer is retained.
  bool allowProducerUserDataOmission = false;
  // Explicit integration policy: preserve/validate source lighting but use the
  // published fixed diagnostic shader during conversion.
  bool diagnosticIgnoreRetainedLighting = false;
  SbViewportRegion viewport;
  SbViewVolume cameraVolume;
  SbMatrix viewMatrix;
  SbMatrix projectionMatrix;
};

struct ElementRange {
  int type = 0;
  int elementIndex = -1;
  std::uint32_t drawStart = 0;
  std::uint32_t drawCount = 0;
};

struct Material {
  Vec4 ambient{}, diffuse{}, specular{}, emissive{};
  double shininess = 0;
  double opacity = 1;
  int shadingModel = 0;
  bool twoSidedLighting = false;
  bool textureAlphaIncludesOpacity = false;
  bool vertexColorAlphaIncludesOpacity = false;
};

struct Resource {
  std::size_t resourceId = 0;
  std::uint32_t sourceHandle = 0;
  std::uint64_t sourceKey = 0, revision = 0;
  std::uint64_t cacheKey = 0, geometryRevision = 0;
  std::uint32_t sourceVertexStride = 0, sourceTexcoordStride = 0;
  // Producer-local positions/normals. Command.model maps positions into the
  // Envelope coordinateFrame using its declared sceneUnits.
  std::vector<Vec3> positions, normals;
  std::vector<Vec4> colors, texcoords;
  std::vector<std::uint32_t> indices;
  std::vector<ElementRange> elementRanges;
};

struct Command {
  int commandIndex = -1;
  std::size_t resourceId = 0;
  std::uint64_t nodeId = 0, instanceId = 0, objectId = 0;
  Matrix4 model{}, view{}, projection{}; // Effective matrices, not inactive fields.
  Material material;
  int materialIndex = 0;
  int stage = 0;
  bool usesCommandMatrices = false;
  bool pickable = true;
  bool hadProducerUserData = false;
  bool hasLightingSetup = false;
  std::uint32_t sourceLightingHandle = 0;
  Vec3 ambientLighting{};
  std::vector<SoLightData> sourceLights; // Bounded copied source lights; diagnostic policy may ignore execution.
  SoRenderState state; // Value-only; contains no producer pointers.
  std::vector<ElementRange> elementRanges;
};

struct OwnedFrame {
  Envelope envelope;
  std::uint32_t generation = 0;
  std::uint64_t contentRevision = 0;
  std::vector<Resource> resources;
  std::vector<Command> commands;
  SoSelectionState selection; // Value-only targets; no scene paths or userData.
  std::size_t triangleCount = 0;
  // Charged owned object/array/string payload, not allocator/heap accounting.
  std::size_t bytesCopied = 0;
};

// Producer array pointers must be valid/stable for the full synchronous call.
// Descriptor checks cannot establish arbitrary pointer allocation capacity.
// Failures clear output. No producer pointer or SoPath is retained on success.
bool capture(const SoDrawList & source, const Envelope & envelope,
             const Limits & limits, OwnedFrame & output, Status & status);

// Diagnostic-only conversion to the unchanged reference ray renderer DTO.
// Retained lighting/selection presentation is deliberately not implemented.
bool toLegacySnapshot(const OwnedFrame & source, const Limits & limits,
                      coin_raytrace::Snapshot & output, Status & status);

} // namespace retained_bridge
#endif
