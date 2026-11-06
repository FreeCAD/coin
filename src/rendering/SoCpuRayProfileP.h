#ifndef COIN_SOCPURAYPROFILEP_H
#define COIN_SOCPURAYPROFILEP_H

#include "rendering/SoCpuRayOwnedSnapshotP.h"
#include "rendering/SoRayTraceRendererP.h"

// Private, explicit review profile. Selection of this profile belongs to the
// host bridge; no environment is read by the geometry adapter or ray kernel.
namespace coin_ray_profile {

inline retained_bridge::Limits captureLimits(coin_raytrace::RenderProfile profile) {
  retained_bridge::Limits result;
  if (profile == coin_raytrace::RenderProfile::LARGE_REVIEW) {
    result.maxCommands = 2048;
    result.maxResources = 2048;
    result.maxVertices = 786432;
    result.maxIndices = 786432;
    result.maxTriangles = 262144;
    result.maxElementRanges = 262144;
    result.maxSelectionTargets = 2048;
    result.maxBytes = 64 * 1024 * 1024;
    result.maxMilliseconds = 250;
    result.maxWidth = result.maxHeight = 4096;
    result.maxPixels = 4194304;
  }
  return result;
}

inline retained_bridge::Limits conversionLimits(coin_raytrace::RenderProfile profile) {
  auto result = captureLimits(profile);
  if (profile == coin_raytrace::RenderProfile::LARGE_REVIEW) {
    result.maxBytes = 256 * 1024 * 1024;
    result.maxMilliseconds = 5000;
  }
  return result;
}

inline coin_raytrace::Limits renderLimits(coin_raytrace::RenderProfile profile) {
  coin_raytrace::Limits result;
  result.profile = profile;
  if (profile == coin_raytrace::RenderProfile::LARGE_REVIEW) {
    result.maxTriangles = 262144;
    result.maxInstances = 2048;
    result.maxPixels = 4194304;
    result.maxWidth = result.maxHeight = 4096;
    result.maxMilliseconds = 5000;
    result.maxSnapshotBytes = 256 * 1024 * 1024;
    result.maxAccelerationBytes = 96 * 1024 * 1024;
    result.maxWorkerScratchBytes = 128 * 1024 * 1024;
  }
  return result;
}

inline std::size_t wholeCommandLimit(coin_raytrace::RenderProfile profile) {
  return profile == coin_raytrace::RenderProfile::LARGE_REVIEW ? 16384 : 4096;
}

} // namespace coin_ray_profile
#endif
