#ifndef COIN_SORAYTRACERENDERERP_H
#define COIN_SORAYTRACERENDERERP_H

// Private, owned data for the experimental synchronous CPU renderer.
// Spatial values use the effective Coin world frame and scene units.
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbViewportRegion.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class SoNode;

namespace coin_raytrace {

enum Code {
  OK, UNSUPPORTED_NODE, CONNECTED_FIELD, IGNORED_FIELD,
  UNSUPPORTED_STATE, INVALID_GEOMETRY, INVALID_MATERIAL,
  INVALID_TRANSFORM, INVALID_CAMERA, CAMERA_MISMATCH,
  NODE_LIMIT, DEPTH_LIMIT, VALUE_LIMIT, TRIANGLE_LIMIT,
  INSTANCE_LIMIT, PIXEL_LIMIT, RAY_TEST_LIMIT, TIME_LIMIT,
  ALLOCATION_FAILURE, CAPTURE_FAILURE, RENDER_FAILURE, CANCELLED
};

struct Status {
  Code code;
  std::string detail;
  Status() : code(OK) {}
};

const char * codeString(Code code);

enum class RenderProfile { DIAGNOSTIC = 0, LARGE_REVIEW = 1 };

struct Limits {
  std::size_t maxNodes, maxDepth, maxValues, maxCoordinates, maxIndices;
  std::size_t maxTriangles, maxInstances, maxPixels, maxRayTests;
  int maxWidth, maxHeight, maxMilliseconds;
  double maxMagnitude;
  // Private explicit admission profile; default behavior remains diagnostic.
  RenderProfile profile;
  std::size_t maxSnapshotBytes, maxAccelerationBytes, maxWorkerScratchBytes;
  Limits() : maxNodes(4096), maxDepth(64), maxValues(262144),
    maxCoordinates(16384), maxIndices(65536), maxTriangles(4096),
    maxInstances(4096), maxPixels(1048576), maxRayTests(50000000),
    maxWidth(2048), maxHeight(2048), maxMilliseconds(2000),
    maxMagnitude(1000000.0), profile(RenderProfile::DIAGNOSTIC),
    maxSnapshotBytes(8 * 1024 * 1024), maxAccelerationBytes(4 * 1024 * 1024),
    maxWorkerScratchBytes(4 * 1024 * 1024) {}
};

typedef std::array<double, 3> Vec3;
typedef std::array<std::array<double, 4>, 4> Matrix4;

struct Material {
  Vec3 ambient, diffuse, specular, emission;
  double shininess, transparency;
  Material() : ambient(), diffuse(), specular(), emission(),
    shininess(0), transparency(0) {}
};

struct Triangle {
  std::array<Vec3, 3> positionsObject, positionsWorld;
  std::array<Vec3, 3> normalsObject, normalsWorld;
  std::array<int, 3> materialIndices;
  std::array<Material, 3> materials;
};

struct Instance {
  std::vector<int> path;
  std::uint64_t sourceShapeId, shapeRevision, geometryId;
  Matrix4 model;
  std::vector<Triangle> triangles;
  Instance() : sourceShapeId(0), shapeRevision(0), geometryId(0), model() {}
};

struct Camera {
  SbViewVolume volume;
  SbMatrix viewing, projection;
  SbViewportRegion viewport;
};

struct Snapshot {
  std::vector<Instance> instances;
  Camera camera;
  std::size_t triangleCount, overlayCount;
  bool cameraValid;
  Snapshot() : triangleCount(0), overlayCount(0), cameraValid(false) {}
};

struct Frame {
  int width, height;
  SbVec2s origin;
  // Bottom-up rows, matching OpenGL window coordinates. Misses remain uncovered.
  std::vector<unsigned char> rgb;
  std::vector<float> depth;
  std::vector<unsigned char> covered;
  std::size_t hitCount, rayTests;
  Frame() : width(0), height(0), origin(0, 0), hitCount(0), rayTests(0) {}
};

bool capture(SoNode * root, const SbViewportRegion & viewport,
             const Limits & limits, Snapshot & snapshot, Status & status);
bool render(const Snapshot & snapshot, const Limits & limits,
            Frame & frame, Status & status);

// Private implementation controls. Bridge callers use defaults; fault/hash
// overrides are reserved for an isolated challenger, never environment settings.
enum Algorithm { SCALAR_REFERENCE, BVH_SINGLE, BVH_WORKERS };
struct RenderOptions {
  Algorithm algorithm = BVH_WORKERS;
  unsigned maxWorkers = 8;
  std::size_t tilePixels = 256;
  const std::atomic<bool> * cancel = nullptr;
  int failWorkerStartAfter = -1;
  bool forceGeometryHashForTesting = false;
  std::uint64_t geometryHashForTesting = 0;
};
struct RenderDiagnostics {
  bool cacheHit = false;
  std::uint64_t geometryHash = 0, cacheGeneration = 0, cacheBuilds = 0, cacheHits = 0;
  std::size_t cacheBytes = 0, bvhNodes = 0, boxTests = 0, triangleTests = 0, tileCount = 0;
  // Worker functions include the caller. nativeThreadsStarted counts additional
  // std::threads, each joined before this synchronous call can return.
  unsigned workersUsed = 0, workersStarted = 0, workersCompleted = 0;
  unsigned activeWorkers = 0, nativeThreadsStarted = 0;
};
bool renderScalarReference(const Snapshot &, const Limits &, Frame &, Status &);
bool renderWithOptions(const Snapshot &, const Limits &, const RenderOptions &,
                       Frame &, Status &, RenderDiagnostics &);
void clearAccelerationCache();
unsigned activeWorkerCountForTesting();

} // namespace coin_raytrace

#endif // COIN_SORAYTRACERENDERERP_H
