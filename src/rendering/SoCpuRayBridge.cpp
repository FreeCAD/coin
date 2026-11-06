#include "rendering/SoCpuRayBridgeP.h"
#include "rendering/SoCpuRayOwnedSnapshotP.h"
#include "rendering/SoCpuRayProfileP.h"
#include "rendering/SoCpuRayOwnedCompareP.h"
#include "glue/glp.h"
#include <Inventor/system/gl.h>
#include <Inventor/SbDPViewVolume.h>
#include <Inventor/SbVec2d.h>
#include <Inventor/SbVec3d.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <future>
#include <iomanip>
#include <locale>
#include <map>
#include <new>
#include <optional>
#include <sstream>
#include <utility>

namespace {
// Private, process-latched research controls. The owned launcher supplies these
// only to its child process; there is no public ABI or persistent preference.
struct RayConfiguration {
  coin_raytrace::RenderOptions options;
  const char * name = "bvh_workers";
  const char * error = nullptr;
  coin_raytrace::RenderProfile profile = coin_raytrace::RenderProfile::DIAGNOSTIC;
  RayConfiguration() {
    const char * requestedProfile = std::getenv("COIN_CPU_RAY_PROFILE");
    if (requestedProfile) {
      std::size_t length = 0;
      while (length <= 16 && requestedProfile[length]) ++length;
      if (length > 16) { error = "invalid private CPU profile length"; return; }
      const std::string value(requestedProfile, length);
      if (value == "large_review") profile = coin_raytrace::RenderProfile::LARGE_REVIEW;
      else if (value != "diagnostic") { error = "unknown private CPU profile"; return; }
    }
    const char * variant = std::getenv("COIN_CPU_RAY_VARIANT");
    if (variant) {
      std::size_t length = 0;
      while (length <= 32 && variant[length]) ++length;
      if (length > 32) { error = "invalid private CPU variant length"; return; }
      const std::string value(variant, length);
      if (value == "scalar_reference") {
        options.algorithm = coin_raytrace::SCALAR_REFERENCE;
        options.maxWorkers = 1; name = "scalar_reference";
      } else if (value == "bvh_single") {
        options.algorithm = coin_raytrace::BVH_SINGLE;
        options.maxWorkers = 1; name = "bvh_single";
      } else if (value != "bvh_workers") {
        error = "unknown private CPU variant"; return;
      }
    }
    const char * threads = std::getenv("COIN_CPU_RAY_THREADS");
    if (threads) {
      if (threads[0] < '1' || threads[0] > '8' || threads[1] != '\0') {
        error = "private CPU thread count must be an integer from 1 through 8"; return;
      }
      const unsigned count = unsigned(threads[0] - '0');
      if (options.algorithm != coin_raytrace::BVH_WORKERS && count != 1) {
        error = "single-thread CPU variants require thread count 1"; return;
      }
      options.maxWorkers = count;
    }
  }
};
const RayConfiguration & rayConfiguration() {
  static const RayConfiguration configuration;
  return configuration;
}

std::string accelerationDetail(const RayConfiguration & configuration,
                               const coin_raytrace::RenderDiagnostics & counts,
                               const coin_raytrace::Frame & frame,
                               std::size_t triangles,
                               std::int64_t prepareNs, std::int64_t conversionNs, std::int64_t traceNs) {
  std::ostringstream text; text.imbue(std::locale::classic());
  text << "diagnostic CPU main surfaces with retained GL overlays\nCOIN_CPU_STATS_V1:{"
    << "\"mode\":\"" << configuration.name << "\",\"configuredWorkers\":" << configuration.options.maxWorkers
    << ",\"workersUsed\":" << counts.workersUsed << ",\"workersStarted\":" << counts.workersStarted
    << ",\"workersCompleted\":" << counts.workersCompleted << ",\"activeWorkers\":" << counts.activeWorkers
    << ",\"nativeThreadsStarted\":" << counts.nativeThreadsStarted
    << ",\"cacheHit\":" << (counts.cacheHit ? "true" : "false")
    << ",\"cacheGeneration\":" << counts.cacheGeneration << ",\"cacheBuilds\":" << counts.cacheBuilds
    << ",\"cacheHits\":" << counts.cacheHits << ",\"cacheBytes\":" << counts.cacheBytes
    << ",\"geometryHash\":\"" << std::hex << std::setw(16) << std::setfill('0') << counts.geometryHash << std::dec
    << "\",\"bvhNodes\":" << counts.bvhNodes << ",\"boxTests\":" << counts.boxTests
    << ",\"triangleTests\":" << counts.triangleTests << ",\"tileCount\":" << counts.tileCount
    << ",\"naiveTests\":" << std::size_t(frame.width) * std::size_t(frame.height) * triangles
    << ",\"profile\":\"" << (configuration.profile == coin_raytrace::RenderProfile::LARGE_REVIEW ? "large_review" : "diagnostic") << "\""
    << ",\"conversion_ns\":" << conversionNs
    << ",\"prepare_ns\":" << prepareNs << ",\"trace_join_ns\":" << traceNs << "}";
  return text.str();
}

// A finite affine model with an exactly zero linear block maps every vertex
// to one point. Filled triangles then emit no fragments. Leave that command
// in the native GL plan, but omit it from the diagnostic trace surface set.
bool fullyCollapsedAffineModel(const SbMatrix & model,
                               const retained_bridge::Limits & bounds) {
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 4; ++column) {
      const double value = model[row][column];
      if (!std::isfinite(value) || std::fabs(value) > bounds.maxMagnitude) return false;
    }
  }
  if (model[0][3] != 0 || model[1][3] != 0 || model[2][3] != 0 || model[3][3] != 1) return false;
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      if (model[row][column] != 0) return false;
    }
  }
  return true;
}

SoCpuRayTracingSurfaceInfo surfaceInfo(const SoRenderCommand & original,
                     const retained_bridge::Command & owned,
                     uint32_t index) {
  SoCpuRayTracingSurfaceInfo info;
  info.originalCommandIndex = index;
  info.sourceLightingHandle = original.lightingHandle;
  info.nodeId = original.nodeId; info.instanceId = original.instanceId;
  info.objectId = original.objectId; info.model = original.modelMatrix;
  info.ambient = original.material.ambient; info.diffuse = original.material.diffuse;
  info.specular = original.material.specular; info.emissive = original.material.emissive;
  info.shininess = original.material.shininess; info.opacity = original.material.opacity;
  info.sourceShadingModel = int(original.material.shadingModel);
  info.materialIndex = original.materialIndex;
  info.hadProducerUserData = original.userData != nullptr;
  info.hasLightingSetup = owned.hasLightingSetup;
  info.ambientLighting.setValue(float(owned.ambientLighting[0]),
    float(owned.ambientLighting[1]), float(owned.ambientLighting[2]));
  for (const auto & light : owned.sourceLights) {
    SoCpuRayTracingLight copy;
    copy.type = int(light.type); copy.color = light.color;
    copy.direction = light.direction; copy.position = light.position;
    copy.attenuation = light.attenuation;
    copy.spotCutoffCos = light.spotCutoffCos; copy.spotExponent = light.spotExponent;
    info.lights.push_back(copy);
  }
  for (const auto & range : owned.elementRanges) {
    SoCpuRayTracingElementRange copy;
    copy.type = range.type; copy.elementIndex = range.elementIndex;
    copy.drawStart = range.drawStart; copy.drawCount = range.drawCount;
    info.elementRanges.push_back(copy);
  }
  return info;
}

using RayClock = std::chrono::steady_clock;

struct RayCapture {
  retained_bridge::OwnedFrame owned;
  std::vector<unsigned char> skipCommands;
  std::vector<SoCpuRayTracingSurfaceInfo> sources;
  std::size_t triangles = 0;
  int64_t prepareNs = 0;
};

bool captureRay(const SoDrawList & source, const SoRenderParams & params,
                RayCapture & capture, std::string & detail) {
  const auto prepareStarted = RayClock::now();
  const auto & configuration = rayConfiguration();
  if (configuration.error) { detail = configuration.error; return false; }
  const auto wholeLimit = coin_ray_profile::wholeCommandLimit(configuration.profile);
  if (!params.cpuRayTracingResult || !params.hasCameraVolume) {
    detail = "diagnostic ray mode requires a configured explicit camera"; return false;
  }
  if (source.getNumCommands() < 0 || size_t(source.getNumCommands()) > wholeLimit) {
    detail = "whole retained command count exceeds diagnostic envelope"; return false;
  }
  if (source.getDepthClearEvents().size() > wholeLimit) {
    detail = "whole retained barrier count exceeds diagnostic envelope"; return false;
  }
  for (const auto & event : source.getDepthClearEvents()) {
    if (event.stage == SoRenderStage::Main) {
      detail = "main-stage depth barriers are outside diagnostic ray profile"; return false;
    }
  }
  try {
    SoDrawList surfaces; surfaces.clear();
    std::vector<uint32_t> originalIndices;
    std::map<SoGeometryHandle, SoGeometryHandle> geometryMap;
    std::map<SoLightingHandle, SoLightingHandle> lightingMap;
    auto bounds = coin_ray_profile::captureLimits(configuration.profile);
    const int captureBudgetMs = bounds.maxMilliseconds;
    auto captureExpired = [&] {
      return std::chrono::duration_cast<std::chrono::milliseconds>(
        RayClock::now() - prepareStarted).count() >= captureBudgetMs;
    };
    size_t stagedRanges = 0;
    auto chargeRanges = [&](size_t count) {
      if (count > bounds.maxElementRanges - stagedRanges) {
        detail = "diagnostic staging element-range limit"; return false;
      }
      stagedRanges += count;
      return true;
    };
    for (int index = 0; index < source.getNumCommands(); ++index) {
      if (captureExpired()) {
        detail = "diagnostic staging time limit"; return false;
      }
      const auto & command = source.getCommand(index);
      if (command.stage != SoRenderStage::Main ||
          command.state.raster.fillMode != SO_RASTER_FILL) continue;
      const auto & geometry = source.getCommandGeometry(command);
      if (geometry.topology == SO_TOPOLOGY_LINES ||
          geometry.topology == SO_TOPOLOGY_LINE_STRIP ||
          geometry.topology == SO_TOPOLOGY_POINTS) continue;
      if (geometry.topology != SO_TOPOLOGY_TRIANGLES) {
        detail = "unsupported main filled-surface topology"; return false;
      }
      if (originalIndices.size() >= bounds.maxCommands) {
        detail = "diagnostic main surface command limit"; return false;
      }
      const SoGeometryResource * resource = nullptr;
      if (command.geometryHandle != SO_INVALID_GEOMETRY_HANDLE) {
        resource = source.getGeometryResource(command.geometryHandle);
        if (!resource) { detail = "invalid main geometry resource handle"; return false; }
      }
      const auto & effectiveRanges = source.getCommandElementRanges(command);
      // Charge every range-bearing value copy before its first allocation.
      if (!chargeRanges(command.pick.elementRanges.size()) ||
          !chargeRanges(effectiveRanges.size()) ||
          (resource && geometryMap.find(command.geometryHandle) == geometryMap.end() &&
           !chargeRanges(resource->elementRanges.size()))) return false;
      SoRenderCommand copy = command;
      copy.pick.elementRanges = effectiveRanges;
      copy.pick.useResourceElementRanges = false;
      if (command.geometryHandle != SO_INVALID_GEOMETRY_HANDLE) {
        const auto existing = geometryMap.find(command.geometryHandle);
        if (existing != geometryMap.end()) copy.geometryHandle = existing->second;
        else {
          if (geometryMap.size() >= bounds.maxResources) { detail = "diagnostic geometry resource limit"; return false; }
          copy.geometryHandle = surfaces.addGeometryResource(*resource);
          geometryMap.emplace(command.geometryHandle, copy.geometryHandle);
        }
      }
      if (command.lightingHandle != 0) {
        const auto * lighting = source.getLighting(command.lightingHandle);
        if (!lighting || lighting->lights.size() > 8) {
          detail = "invalid or excessive source lighting metadata"; return false;
        }
        const auto existing = lightingMap.find(command.lightingHandle);
        if (existing != lightingMap.end()) copy.lightingHandle = existing->second;
        else {
          copy.lightingHandle = surfaces.addLightingSetup(*lighting);
          lightingMap.emplace(command.lightingHandle, copy.lightingHandle);
        }
      }
      // Topology, handle, range, resource and lighting preflight above still
      // applies to non-emitting commands. Do not weaken snapshot validation of
      // other singular or tiny-nonzero models, or skip the native GL command.
      if (fullyCollapsedAffineModel(copy.modelMatrix, bounds)) continue;
      surfaces.addCommand(std::move(copy));
      originalIndices.push_back(uint32_t(index));
    }
    if (originalIndices.empty()) { detail = "no admitted main triangle surfaces"; return false; }
    retained_bridge::Envelope envelope;
    envelope.sceneUnits = "mm";
    envelope.coordinateFrame = "FreeCAD world";
    envelope.viewport = params.viewport;
    envelope.devicePixelRatio = params.devicePixelRatio;
    envelope.cameraVolume = params.cameraVolume;
    envelope.viewMatrix = params.viewMatrix;
    envelope.projectionMatrix = params.projMatrix;
    envelope.expectedGeneration = surfaces.getGeneration();
    envelope.expectedContentRevision = surfaces.getContentRevision();
    envelope.allowProducerUserDataOmission = true;
    envelope.diagnosticIgnoreRetainedLighting = true;
    retained_bridge::OwnedFrame owned;
    retained_bridge::Status status;
    // Staging and copying share a cooperative deadline; do not grant the
    // adapter a fresh full budget after staging already consumed GUI time.
    const auto stagingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      RayClock::now() - prepareStarted).count();
    if (stagingMs >= captureBudgetMs) {
      detail = "diagnostic staging time limit"; return false;
    }
    bounds.maxMilliseconds = captureBudgetMs - int(stagingMs);
    if (!retained_bridge::capture(surfaces, envelope, bounds, owned, status)) {
      detail = std::string(retained_bridge::codeString(status.code)) + ": " + status.detail; return false;
    }
    capture.triangles = owned.triangleCount;
    capture.skipCommands.assign(size_t(source.getNumCommands()), 0);
    capture.sources.reserve(originalIndices.size());
    for (size_t index = 0; index < originalIndices.size(); ++index) {
      if (captureExpired()) { detail = "diagnostic metadata time limit"; return false; }
      const uint32_t original = originalIndices[index];
      capture.skipCommands[original] = 1;
      capture.sources.push_back(surfaceInfo(source.getCommand(int(original)),
                                            owned.commands[index], original));
    }
    if (captureExpired()) { detail = "diagnostic capture time limit"; return false; }
    capture.owned = std::move(owned);
    capture.prepareNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
      RayClock::now() - prepareStarted).count();
    return true;
  }
  catch (const std::bad_alloc &) { detail = "diagnostic preflight allocation failure"; }
  catch (...) { detail = "diagnostic preflight exception"; }
  return false;
}

struct RayOutput {
  coin_raytrace::Frame frame;
  coin_raytrace::RenderDiagnostics diagnostics;
  coin_raytrace::Status status;
  int64_t conversionNs = 0;
  int64_t traceNs = 0;
  bool success = false;
};

RayOutput traceRay(const retained_bridge::OwnedFrame & owned,
                   const coin_raytrace::RenderOptions & options) {
  RayOutput output;
  const auto started = RayClock::now();
  // Only owned values cross this boundary. No scene, Qt, GL or producer access.
  coin_raytrace::Snapshot snapshot;
  retained_bridge::Status conversionStatus;
  auto bounds = coin_ray_profile::conversionLimits(rayConfiguration().profile);
  bounds.cancellation = options.cancel;
  if (!retained_bridge::toLegacySnapshot(owned, bounds, snapshot, conversionStatus)) {
    output.status.code = conversionStatus.code == retained_bridge::Code::CANCELLED ?
      coin_raytrace::CANCELLED : coin_raytrace::CAPTURE_FAILURE;
    output.status.detail = std::string(retained_bridge::codeString(conversionStatus.code)) + ": " + conversionStatus.detail;
    return output;
  }
  const auto converted = RayClock::now();
  output.conversionNs = std::chrono::duration_cast<std::chrono::nanoseconds>(converted - started).count();
  output.success = coin_raytrace::renderWithOptions(snapshot,
    coin_ray_profile::renderLimits(rayConfiguration().profile), options,
    output.frame, output.status, output.diagnostics);
  output.traceNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
    RayClock::now() - converted).count();
  return output;
}

void publishRay(const SoDrawList & source, const SoRenderParams & params,
                const RayCapture & capture, const RayOutput & trace,
                SoCpuRayPreparedFrame & output) {
  output.skipCommands = capture.skipCommands;
  output.accelerationDetail = accelerationDetail(rayConfiguration(), trace.diagnostics, trace.frame,
    capture.triangles, capture.prepareNs, trace.conversionNs, trace.traceNs);
  auto & result = *params.cpuRayTracingResult;
  result.generation = source.getGeneration();
  result.contentRevision = source.getContentRevision();
  result.triangleCount = capture.triangles;
  result.hitCount = trace.frame.hitCount; result.rayTests = trace.frame.rayTests;
  result.width = trace.frame.width; result.height = trace.frame.height;
  result.viewportX = trace.frame.origin[0]; result.viewportY = trace.frame.origin[1];
  result.devicePixelRatio = params.devicePixelRatio;
  result.viewMatrix = params.viewMatrix; result.projectionMatrix = params.projMatrix;
  result.rgb = trace.frame.rgb; result.depth = trace.frame.depth;
  result.coverage = trace.frame.covered;
  result.sources = capture.sources;
}

struct RayIdentity {
  uint64_t scene, camera;
  uint32_t context, renderFlags;
  int framebuffer, samples, depthBits;
  bool srgb;
  float devicePixelRatio;
  SbVec2s window, viewportOrigin, viewportSize;
  SbMatrix view, projection;
  bool operator==(const RayIdentity &) const = default;
};

struct RayJobControl {
  std::atomic<bool> started{false};
  std::atomic<bool> cancelled{false};
};
} // namespace

bool coin_cpu_ray_async_enabled() noexcept {
  static const bool enabled = [] {
    const char * value = std::getenv("COIN_CPU_RAY_ASYNC");
    return value && value[0] == '1' && value[1] == '\0';
  }();
  return enabled;
}

bool coin_prepare_cpu_ray_frame(const SoDrawList & source,
                               const SoRenderParams & params,
                               SoCpuRayPreparedFrame & output,
                               std::string & detail) {
  output = SoCpuRayPreparedFrame();
  try {
    RayCapture capture;
    if (!captureRay(source, params, capture, detail)) return false;
    auto traced = traceRay(capture.owned, rayConfiguration().options);
    if (!traced.success) {
      detail = "CPU diagnostic render rejected: " + traced.status.detail; return false;
    }
    publishRay(source, params, capture, traced, output);
    output.frame = std::move(traced.frame);
    detail.clear();
    return true;
  }
  catch (const std::bad_alloc &) { detail = "diagnostic preflight allocation failure"; }
  catch (...) { detail = "diagnostic preflight exception"; }
  return false;
}

struct SoCpuRayAsync::State {
  std::optional<RayIdentity> live;
  RayClock::time_point changed;
  std::shared_ptr<RayJobControl> control;
  std::future<RayOutput> active;
  std::shared_ptr<const retained_bridge::OwnedFrame> snapshot;
  int64_t prepareNs = 0;
  std::optional<RayOutput> ready;
  std::string terminal;
  const char * phase = "quiet";
  unsigned pollMs = 0;
  uint64_t job = 0, started = 0, requested = 0, cancelled = 0, published = 0;
  uint64_t lastCancelled = 0, lastPublished = 0;

  void cancel() noexcept {
    if (control && !control->cancelled.exchange(true)) ++requested;
    ready.reset();
    snapshot.reset();
    terminal.clear();
  }
};

SoCpuRayAsync::SoCpuRayAsync() : state(new State) {}

SoCpuRayAsync::~SoCpuRayAsync() {
  state->cancel();
  if (state->active.valid()) state->active.wait();
}

void SoCpuRayAsync::requestCancel() noexcept {
  if (state->active.valid() && state->control &&
      !state->control->cancelled.exchange(true)) ++state->requested;
}

void SoCpuRayAsync::invalidate() noexcept {
  state->cancel();
  state->live.reset();
  state->pollMs = 0;
  state->phase = "rejected";
}

unsigned SoCpuRayAsync::pollMilliseconds() const noexcept { return state->pollMs; }

void SoCpuRayAsync::published() noexcept {
  if (state->job != state->lastPublished) {
    state->lastPublished = state->job;
    ++state->published;
  }
}

std::string SoCpuRayAsync::telemetry() const {
  const auto & s = *state;
  const bool started = s.control && s.control->started.load();
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << "\nCOIN_ASYNC_V1:{\"state\":\"" << s.phase << "\",\"submitted\":" << s.job
      << ",\"started\":" << s.started + (started ? 1 : 0)
      << ",\"cancel_requested\":" << s.requested << ",\"cancelled\":" << s.cancelled
      << ",\"published\":" << s.published << ",\"job\":" << s.job
      << ",\"last_cancelled_job\":" << s.lastCancelled
      << ",\"last_published_job\":" << s.lastPublished
      << ",\"worker_active\":" << (started ? coin_raytrace::activeWorkerCountForTesting() : 0)
      << "}";
  return out.str();
}

bool SoCpuRayAsync::prepare(const SoDrawList & source, const SoRenderParams & params, int depthBits,
                            SoCpuRayPreparedFrame & output, std::string & detail) {
  auto & s = *state;
  const auto now = RayClock::now();
  const auto & result = *params.cpuRayTracingResult;
  const RayIdentity identity{params.cpuRaySceneRevision, result.cameraNodeId, result.contextId,
    params.flags & (SO_PARAM_CLEAR_WINDOW | SO_PARAM_CLEAR_DEPTH),
    result.framebuffer, result.samples, depthBits, result.framebufferSrgbEnabled != FALSE,
    params.devicePixelRatio, params.viewport.getWindowSize(), params.viewport.getViewportOriginPixels(),
    params.viewport.getViewportSizePixels(), params.viewMatrix, params.projMatrix};
  if (!s.live || *s.live != identity) {
    // A scene notification can be only a native overlay update. Keep a
    // completed image provisionally if the rest of the target is unchanged;
    // the fresh capture/exact owned-input gate below remains the publish authority.
    auto previousTarget = s.live.value_or(identity);
    previousTarget.scene = identity.scene;
    const bool revalidateReady = s.live && s.ready && s.snapshot &&
      !s.active.valid() && previousTarget == identity;
    if (!revalidateReady) s.cancel();
    s.live = identity;
    if (!revalidateReady) s.changed = now;
  }
  s.pollMs = 0;
  try {
    if (s.active.valid() && s.active.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      const bool superseded = s.control->cancelled.load();
      if (s.control->started.load()) ++s.started;
      s.control.reset();
      auto traced = s.active.get();
      if (traced.status.code == coin_raytrace::CANCELLED) {
        ++s.cancelled;
        s.lastCancelled = s.job;
      }
      if (!superseded) {
        if (traced.success) s.ready.emplace(std::move(traced));
        else s.terminal = "CPU diagnostic render rejected: " + traced.status.detail;
      }
    }
    if (s.ready) {
      // FreeCAD may rebuild the DrawList for annotations on every frame.
      // Only the current admitted pass may supply its suppression indices.
      RayCapture current;
      if (!captureRay(source, params, current, detail)) {
        s.cancel();
        s.phase = "rejected";
        s.terminal = detail;
        return false;
      }
      if (coin_ray_owned_compare::same(*s.snapshot, current.owned)) {
        current.prepareNs += s.prepareNs;
        publishRay(source, params, current, *s.ready, output);
        output.frame = s.ready->frame;
        s.phase = "ready";
        detail.clear();
        return true;
      }
      s.cancel();
      s.changed = now;
    }
    if (!s.terminal.empty()) {
      s.phase = "rejected";
      detail = s.terminal;
      return false;
    }
    constexpr auto quiet = std::chrono::milliseconds(100);
    const auto elapsed = now - s.changed;
    if (elapsed < quiet) {
      s.phase = "quiet";
      s.pollMs = unsigned(std::chrono::ceil<std::chrono::milliseconds>(quiet - elapsed).count());
      detail = "CPU async waiting for a quiet view";
      return false;
    }
    if (!s.active.valid()) {
      RayCapture capture;
      if (!captureRay(source, params, capture, detail)) {
        s.phase = "rejected";
        s.terminal = detail;
        return false;
      }
      auto control = std::make_shared<RayJobControl>();
      auto options = rayConfiguration().options;
      auto snapshot = std::make_shared<const retained_bridge::OwnedFrame>(std::move(capture.owned));
      s.active = std::async(std::launch::async,
        [snapshot, control, options]() mutable {
          options.cancel = &control->cancelled;
          control->started.store(true);
          return traceRay(*snapshot, options);
        });
      s.snapshot = std::move(snapshot);
      s.prepareNs = capture.prepareNs;
      s.control = std::move(control);
      ++s.job;
    }
    s.phase = "tracing";
    s.pollMs = 16;
    detail = "CPU async trace pending";
    return false;
  }
  catch (...) {
    invalidate();
    s.live = identity;
    s.phase = "failed";
    s.terminal = detail = "CPU async coordinator failure";
    return false;
  }
}

bool coin_readback_cpu_ray_target(const cc_glglue * glue,
                                 SoCpuRayTracingResult & result,
                                 std::string & detail) {
  result.readbackValid = false;
  if (!glue || result.width <= 0 || result.height <= 0 ||
      size_t(result.width) * size_t(result.height) >
        coin_ray_profile::renderLimits(rayConfiguration().profile).maxPixels) {
    detail = "invalid ray readback dimensions"; return false;
  }
  GLint oldRead = 0, oldBuffer = 0, oldPack = 4, oldPackRowLength = 0;
  GLint targetBuffer = 0;
  GLint oldPackSkipRows = 0, oldPackSkipPixels = 0, oldPbo = 0;
  GLint oldPackSwapBytes = GL_FALSE;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldRead);
  glGetIntegerv(GL_READ_BUFFER, &oldBuffer);
  glGetIntegerv(GL_DRAW_BUFFER, &targetBuffer);
  glGetIntegerv(GL_PACK_ALIGNMENT, &oldPack);
  glGetIntegerv(GL_PACK_ROW_LENGTH, &oldPackRowLength);
  glGetIntegerv(GL_PACK_SKIP_ROWS, &oldPackSkipRows);
  glGetIntegerv(GL_PACK_SKIP_PIXELS, &oldPackSkipPixels);
  glGetIntegerv(GL_PACK_SWAP_BYTES, &oldPackSwapBytes);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &oldPbo);
  struct Restore {
    const cc_glglue * glue;
    GLint read, buffer, pack, row, rows, pixels, swapBytes, pbo;
    ~Restore() {
      cc_glglue_glBindFramebuffer(glue, GL_READ_FRAMEBUFFER, GLuint(read));
      glReadBuffer(GLenum(buffer));
      glPixelStorei(GL_PACK_ALIGNMENT, pack); glPixelStorei(GL_PACK_ROW_LENGTH, row);
      glPixelStorei(GL_PACK_SKIP_ROWS, rows); glPixelStorei(GL_PACK_SKIP_PIXELS, pixels);
      glPixelStorei(GL_PACK_SWAP_BYTES, swapBytes);
      cc_glglue_glBindBuffer(glue, GL_PIXEL_PACK_BUFFER, GLuint(pbo));
    }
  } restore{glue, oldRead, oldBuffer, oldPack, oldPackRowLength,
            oldPackSkipRows, oldPackSkipPixels, oldPackSwapBytes, oldPbo};
  try {
    const size_t pixels = size_t(result.width) * size_t(result.height);
    std::vector<unsigned char> rgb(pixels * 3);
    std::vector<float> depth(pixels);
    cc_glglue_glBindFramebuffer(glue, GL_READ_FRAMEBUFFER, GLuint(result.framebuffer));
    glReadBuffer(GLenum(targetBuffer));
    cc_glglue_glBindBuffer(glue, GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0); glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE); // Return native-order floats.
    glReadPixels(result.viewportX, result.viewportY, result.width, result.height,
                 GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    glReadPixels(result.viewportX, result.viewportY, result.width, result.height,
                 GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    if (glGetError() != GL_NO_ERROR) { detail = "bound-FBO color/depth readback GL error"; return false; }
    result.readbackRgb = std::move(rgb); result.readbackDepth = std::move(depth);
    result.readbackValid = true;
    return true;
  }
  catch (...) { detail = "bound-FBO readback allocation failure"; return false; }
}
