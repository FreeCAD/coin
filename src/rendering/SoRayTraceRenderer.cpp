#include "rendering/SoRayTraceRendererP.h"

#include <Inventor/SbVec2d.h>
#include <Inventor/SbVec3d.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cfenv>
#include <cmath>
#include <limits>
#include <new>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <numeric>
#include <thread>
#include <utility>
#include <sstream>
#include <iomanip>
#include <locale>
#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#define COIN_RAYTRACE_X86_FP 1
#endif

namespace coin_raytrace {
namespace {

const double epsilon = 1e-9;

struct Failure {
  Code code;
  const char * detail;
  Failure(Code c, const char * d) : code(c), detail(d) {}
};

void require(bool value, Code code, const char * detail) {
  if (!value) throw Failure(code, detail);
}

Vec3 add(const Vec3 & a, const Vec3 & b) {
  return Vec3{{a[0] + b[0], a[1] + b[1], a[2] + b[2]}};
}
Vec3 sub(const Vec3 & a, const Vec3 & b) {
  return Vec3{{a[0] - b[0], a[1] - b[1], a[2] - b[2]}};
}
Vec3 mul(const Vec3 & a, double factor) {
  return Vec3{{a[0] * factor, a[1] * factor, a[2] * factor}};
}
double dot(const Vec3 & a, const Vec3 & b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
Vec3 cross(const Vec3 & a, const Vec3 & b) {
  return Vec3{{a[1] * b[2] - a[2] * b[1],
               a[2] * b[0] - a[0] * b[2],
               a[0] * b[1] - a[1] * b[0]}};
}
Vec3 unit(const Vec3 & value) {
  const double length = std::sqrt(dot(value, value));
  require(std::isfinite(length) && length > 0, INVALID_GEOMETRY, "zero/nonfinite vector");
  return mul(value, 1 / length);
}
Vec3 vec(const SbVec3d & value, double bound) {
  const Vec3 result = {{value[0], value[1], value[2]}};
  for (int k = 0; k < 3; ++k)
    require(std::isfinite(result[k]) && std::fabs(result[k]) <= bound,
            INVALID_CAMERA, "ray endpoint magnitude");
  return result;
}
double clamp(double value) { return std::max(0.0, std::min(1.0, value)); }

Vec3 interpolate(const std::array<Vec3, 3> & values, const Vec3 & weights) {
  Vec3 result = {{0, 0, 0}};
  for (int k = 0; k < 3; ++k) for (int c = 0; c < 3; ++c)
    result[c] += values[k][c] * weights[k];
  return result;
}

Material interpolateMaterial(const std::array<Material, 3> & values, const Vec3 & weights) {
  Material result;
  for (int k = 0; k < 3; ++k) {
    for (int c = 0; c < 3; ++c) {
      result.ambient[c] += weights[k] * values[k].ambient[c];
      result.diffuse[c] += weights[k] * values[k].diffuse[c];
      result.specular[c] += weights[k] * values[k].specular[c];
      result.emission[c] += weights[k] * values[k].emission[c];
    }
    result.shininess += weights[k] * values[k].shininess;
    result.transparency += weights[k] * values[k].transparency;
  }
  return result;
}

Vec3 shade(const Material & material, const Vec3 & normal, const Vec3 & direction) {
  const Vec3 view = mul(direction, -1);
  const Vec3 shadingNormal = dot(normal, view) >= 0 ? normal : mul(normal, -1);
  const Vec3 light = unit(Vec3{{1, 2, 3}});
  const double diffuse = std::max(0.0, dot(shadingNormal, light));
  const Vec3 halfRaw = add(light, view);
  double specular = 0;
  if (dot(halfRaw, halfRaw) > 0) {
    const Vec3 halfway = unit(halfRaw);
    specular = std::pow(std::max(0.0, dot(shadingNormal, halfway)), 1 + 31 * material.shininess);
  }
  Vec3 result;
  for (int c = 0; c < 3; ++c) {
    result[c] = clamp(.25 * material.ambient[c] + .75 * material.diffuse[c] * diffuse +
                      material.emission[c] + .25 * material.specular[c] * specular);
    require(std::isfinite(result[c]), RENDER_FAILURE, "nonfinite shaded color");
  }
  return result;
}

struct Candidate {
  const Instance * instance;
  const Triangle * triangle;
  std::size_t triangleIndex;
  double t;
  Vec3 weights;
};

bool repeatedProducerVertex(const Triangle & triangle, double bound) {
  // Equality of two finite producer vertices proves a non-emitting filled
  // triangle. A zero floating cross product or CPU-rounded world coincidence
  // alone does not: either can discard a real, distinct tiny triangle.
  for (const auto * positions : {&triangle.positionsObject, &triangle.positionsWorld})
    for (const auto & position : *positions) for (double value : position)
      if (!std::isfinite(value) || std::fabs(value) > bound) return false;
  for (int a = 0; a != 3; ++a) for (int b = a + 1; b != 3; ++b)
    if (triangle.positionsObject[a] == triangle.positionsObject[b] &&
        triangle.positionsWorld[a] == triangle.positionsWorld[b]) return true;
  return false;
}

bool intersect(const Triangle & triangle, const Vec3 & origin, const Vec3 & direction,
               double length, double & t, Vec3 & weights, bool allowRepeatedProducerVertex = false,
               double coordinateBound = 1000000.0) {
  const Vec3 edge1 = sub(triangle.positionsWorld[1], triangle.positionsWorld[0]);
  const Vec3 edge2 = sub(triangle.positionsWorld[2], triangle.positionsWorld[0]);
  const Vec3 geometricNormal = cross(edge1, edge2);
  const double normalLength = std::sqrt(dot(geometricNormal, geometricNormal));
  if (allowRepeatedProducerVertex && (!(normalLength > 0) || !std::isfinite(normalLength)) &&
      repeatedProducerVertex(triangle, coordinateBound)) return false;
  require(std::isfinite(normalLength) && normalLength > 0,
          INVALID_GEOMETRY, "degenerate world triangle");
  const Vec3 p = cross(direction, edge2);
  const double determinant = dot(edge1, p);
  require(std::isfinite(determinant), INVALID_GEOMETRY, "nonfinite ray determinant");
  // Parallelism is angular. An absolute area threshold silently loses valid
  // small-unit triangles even when they occupy a large part of the viewport.
  const double parallelTolerance = 64 * std::numeric_limits<double>::epsilon() * normalLength;
  if (std::fabs(determinant) <= parallelTolerance) return false;
  const double inverse = 1 / determinant;
  const Vec3 offset = sub(origin, triangle.positionsWorld[0]);
  const double u = dot(offset, p) * inverse;
  if (u < -epsilon || u > 1 + epsilon) return false;
  const Vec3 q = cross(offset, edge1);
  const double v = dot(direction, q) * inverse;
  if (v < -epsilon || u + v > 1 + epsilon) return false;
  t = dot(edge2, q) * inverse;
  // t is distance from this pixel's near-plane point, in Coin scene units.
  if (!std::isfinite(t) || t < 0 || t > length) return false;
  weights = Vec3{{1 - u - v, u, v}};
  return true;
}

bool identityLess(const Candidate & a, const Candidate & b) {
  if (a.instance->path != b.instance->path) return a.instance->path < b.instance->path;
  return a.triangleIndex < b.triangleIndex;
}

double windowDepth(const Vec3 & position, const Camera & camera) {
  double world[4] = {position[0], position[1], position[2], 1};
  double eye[4] = {0, 0, 0, 0}, clip[4] = {0, 0, 0, 0};
  // Coin matrices multiply row vectors. Preserve homogeneous W until projection.
  for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
    eye[c] += world[r] * camera.viewing[r][c];
  for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
    clip[c] += eye[r] * camera.projection[r][c];
  require(std::isfinite(clip[2]) && std::isfinite(clip[3]) && clip[3] > 0,
          INVALID_CAMERA, "invalid homogeneous camera depth");
  const double value = .5 * (clip[2] / clip[3]) + .5;
  require(std::isfinite(value) && value >= -1e-5 && value <= 1 + 1e-5,
          INVALID_CAMERA, "ray/clip depth disagreement");
  return clamp(value);
}

bool validRenderLimits(const Limits & limits) {
  const bool large = limits.profile == RenderProfile::LARGE_REVIEW;
  if (!large && limits.profile != RenderProfile::DIAGNOSTIC) return false;
  return limits.maxWidth > 0 && limits.maxWidth <= (large ? 4096 : 2048) &&
    limits.maxHeight > 0 && limits.maxHeight <= (large ? 4096 : 2048) &&
    limits.maxPixels > 0 && limits.maxPixels <= (large ? 4194304u : 1048576u) &&
    limits.maxRayTests <= 50000000 &&
    limits.maxInstances <= (large ? 2048u : 4096u) &&
    limits.maxTriangles <= (large ? 262144u : 4096u) &&
    std::isfinite(limits.maxMagnitude) && limits.maxMagnitude > 0 && limits.maxMagnitude <= 1000000 &&
    limits.maxMilliseconds > 0 && limits.maxMilliseconds <= 10000 &&
    limits.maxSnapshotBytes > 0 && limits.maxSnapshotBytes <= (large ? 256u : 8u) * 1024 * 1024 &&
    limits.maxAccelerationBytes > 0 && limits.maxAccelerationBytes <= (large ? 96u : 4u) * 1024 * 1024 &&
    limits.maxWorkerScratchBytes > 0 && limits.maxWorkerScratchBytes <= (large ? 128u : 4u) * 1024 * 1024;
}

void chargePayload(std::size_t count, std::size_t width, std::size_t limit, std::size_t & used,
                   const char * detail) {
  require(used <= limit && (width == 0 || count <= (limit - used) / width), ALLOCATION_FAILURE, detail);
  used += count * width;
}

void validateSnapshotPayload(const Snapshot & snapshot, const Limits & limits) {
  // Charge actual vector capacities, not just logical lengths. This bounds the
  // externally owned input admitted to a render; its producer owns allocation.
  std::size_t bytes = sizeof(Snapshot);
  chargePayload(snapshot.instances.capacity(), sizeof(Instance), limits.maxSnapshotBytes, bytes, "snapshot instance byte bound");
  for (const auto & instance : snapshot.instances) {
    chargePayload(instance.path.capacity(), sizeof(int), limits.maxSnapshotBytes, bytes, "snapshot path byte bound");
    chargePayload(instance.triangles.capacity(), sizeof(Triangle), limits.maxSnapshotBytes, bytes, "snapshot triangle byte bound");
  }
}

} // namespace

static bool renderScalar(const Snapshot & snapshot, const Limits & limits, Frame & frame, Status & status,
                         const std::atomic<bool> * cancel) {
  const auto checkCancellation = [cancel]() {
    require(!cancel || !cancel->load(std::memory_order_acquire), CANCELLED, "ray tracing cancelled");
  };
  frame = Frame();
  status = Status();
  try {
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    checkCancellation();
    require(validRenderLimits(limits), UNSUPPORTED_STATE, "invalid private render limits");
    require(snapshot.cameraValid, INVALID_CAMERA, "missing captured camera");
    require(snapshot.instances.size() <= limits.maxInstances && snapshot.triangleCount <= limits.maxTriangles,
            TRIANGLE_LIMIT, "snapshot geometry budget");
    validateSnapshotPayload(snapshot, limits);
    const SbVec2s dimensions = snapshot.camera.viewport.getViewportSizePixels();
    const int width = dimensions[0], height = dimensions[1];
    require(width > 0 && height > 0 && width <= limits.maxWidth && height <= limits.maxHeight &&
      std::size_t(width) <= limits.maxPixels / std::size_t(height), PIXEL_LIMIT, "frame dimensions");
    const std::size_t pixels = std::size_t(width) * std::size_t(height);
    require(snapshot.triangleCount == 0 || pixels <= limits.maxRayTests / snapshot.triangleCount,
            RAY_TEST_LIMIT, "pixel/triangle product");
    std::size_t actualTriangles = 0;
    for (std::size_t i = 0; i < snapshot.instances.size(); ++i) {
      require(snapshot.instances[i].triangles.size() <= limits.maxTriangles - actualTriangles,
              TRIANGLE_LIMIT, "snapshot triangle count");
      actualTriangles += snapshot.instances[i].triangles.size();
    }
    require(actualTriangles == snapshot.triangleCount, INVALID_GEOMETRY, "snapshot count mismatch");
    Frame output;
    output.width = width; output.height = height;
    output.origin = snapshot.camera.viewport.getViewportOriginPixels();
    output.rgb.assign(pixels * 3, 0);
    output.depth.assign(pixels, 1);
    output.covered.assign(pixels, 0);
    std::vector<Candidate> candidates;
    require(snapshot.triangleCount <= limits.maxWorkerScratchBytes / sizeof(Candidate),
            ALLOCATION_FAILURE, "scalar hit scratch bound");
    candidates.reserve(snapshot.triangleCount);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
      const std::size_t pixel = std::size_t(y) * std::size_t(width) + std::size_t(x);
      if (pixel % 64 == 0) {
        checkCancellation();
        require(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started).count() <= limits.maxMilliseconds,
          TIME_LIMIT, "ray tracing deadline");
      }
      SbVec3d start, finish;
      snapshot.camera.volume.getDPViewVolume().projectPointToLine(
        SbVec2d((x + .5) / width, (y + .5) / height), start, finish);
      const Vec3 origin = vec(start, limits.maxMagnitude);
      const Vec3 delta = sub(vec(finish, limits.maxMagnitude), origin);
      const double length = std::sqrt(dot(delta, delta));
      require(std::isfinite(length) && length > 0, INVALID_CAMERA, "zero camera ray segment");
      const Vec3 direction = mul(delta, 1 / length);
      candidates.clear();
      double nearest = std::numeric_limits<double>::infinity();
      for (std::size_t i = 0; i < snapshot.instances.size(); ++i) {
        if ((i % 64) == 0) checkCancellation();
        const Instance & instance = snapshot.instances[i];
        for (std::size_t t = 0; t < instance.triangles.size(); ++t) {
          if ((t % 64) == 0) checkCancellation();
          ++output.rayTests;
          Candidate candidate;
          candidate.instance = &instance; candidate.triangle = &instance.triangles[t]; candidate.triangleIndex = t;
          if (intersect(instance.triangles[t], origin, direction, length, candidate.t, candidate.weights,
                        limits.profile == RenderProfile::LARGE_REVIEW, limits.maxMagnitude)) {
            nearest = std::min(nearest, candidate.t);
            candidates.push_back(candidate);
          }
        }
      }
      if (candidates.empty()) continue;
      const Candidate * selected = NULL;
      // Form the tie set around the true minimum, avoiding nontransitive pairwise ties.
      for (std::size_t c = 0; c < candidates.size(); ++c) {
        if ((c % 64) == 0) checkCancellation();
        const Candidate & candidate = candidates[c];
        if (candidate.t - nearest <= epsilon && (!selected || identityLess(candidate, *selected))) selected = &candidate;
      }
      require(selected != NULL, RENDER_FAILURE, "missing nearest intersection");
      const Vec3 position = add(origin, mul(direction, selected->t));
      const Vec3 normal = unit(interpolate(selected->triangle->normalsWorld, selected->weights));
      const Material material = interpolateMaterial(selected->triangle->materials, selected->weights);
      const Vec3 rgb = shade(material, normal, direction);
      for (int c = 0; c < 3; ++c)
        output.rgb[3 * pixel + std::size_t(c)] = static_cast<unsigned char>(std::floor(rgb[c] * 255 + .5));
      output.depth[pixel] = static_cast<float>(windowDepth(position, snapshot.camera));
      output.covered[pixel] = 1;
      ++output.hitCount;
    }
    checkCancellation();
    frame = std::move(output);
    return true;
  }
  catch (const Failure & failure) {
    status.code = failure.code;
    try { status.detail = failure.detail; } catch (...) { status.code = ALLOCATION_FAILURE; }
  }
  catch (const std::bad_alloc &) { status.code = ALLOCATION_FAILURE; }
  catch (...) { status.code = RENDER_FAILURE; }
  return false;
}

bool renderScalarReference(const Snapshot & snapshot, const Limits & limits, Frame & frame, Status & status) {
  return renderScalar(snapshot, limits, frame, status, nullptr);
}

#include "rendering/SoRayTraceAccelerationP.h"

} // namespace coin_raytrace
