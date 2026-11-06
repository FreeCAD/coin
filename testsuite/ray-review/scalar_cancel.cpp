#include <Inventor/SoDB.h>
#include "rendering/SoRayTraceRendererP.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace R = coin_raytrace;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

namespace {
std::mutex mutex;
std::condition_variable changed;
bool armed = false;
bool entered = false;
bool released = false;

R::Snapshot square()
{
  R::Snapshot snapshot;
  snapshot.cameraValid = true;
  snapshot.triangleCount = 2;
  snapshot.camera.viewport = SbViewportRegion(512, 512);
  snapshot.camera.volume.ortho(-1, 1, -1, 1, 1, 10);
  snapshot.camera.volume.translateCamera(SbVec3f(0, 0, 5));
  snapshot.camera.volume.getMatrices(snapshot.camera.viewing, snapshot.camera.projection);
  R::Instance instance;
  instance.path = {0};
  instance.sourceShapeId = instance.geometryId = 1;
  for (int i = 0; i < 4; ++i) instance.model[i][i] = 1;
  const R::Vec3 corners[] = {{{-2,-2,0}}, {{2,-2,0}}, {{2,2,0}}, {{-2,2,0}}};
  const int indices[][3] = {{0,1,2}, {0,2,3}};
  for (const auto & face : indices) {
    R::Triangle triangle{};
    for (int j = 0; j < 3; ++j) {
      triangle.positionsWorld[j] = triangle.positionsObject[j] = corners[face[j]];
      triangle.normalsWorld[j] = triangle.normalsObject[j] = {{0,0,1}};
      triangle.materials[j].diffuse = {{.6,.5,.4}};
    }
    instance.triangles.push_back(triangle);
  }
  snapshot.instances.push_back(instance);
  return snapshot;
}

bool identical(const R::Frame & a, const R::Frame & b)
{
  return a.width == b.width && a.height == b.height && a.origin == b.origin &&
         a.rgb == b.rgb && a.depth == b.depth && a.covered == b.covered &&
         a.hitCount == b.hitCount && a.rayTests == b.rayTests;
}
} // namespace

// Called by the test translation unit after one completed intersection.
void cancellationTileBarrier(unsigned, std::size_t)
{
  std::unique_lock<std::mutex> lock(mutex);
  if (!armed) return;
  entered = true;
  changed.notify_all();
  if (!changed.wait_for(lock, 5s, [] { return released; }))
    throw std::runtime_error("scalar barrier timed out");
}

int main()
{
  SoDB::init();
  const R::Snapshot snapshot = square();
  R::Limits limits;
  limits.maxMilliseconds = 10000;
  R::Status status;
  R::Frame reference;
  if (!R::renderScalarReference(snapshot, limits, reference, status) || !reference.hitCount) return 2;
  R::RenderOptions options;
  options.algorithm = R::SCALAR_REFERENCE;
  std::atomic<bool> cancel{false};
  options.cancel = &cancel;
  R::RenderDiagnostics diagnostics;
  for (int trial = 0; trial < 5; ++trial) {
    R::Frame frame = reference; // Catch stale caller-owned output as well as partial new output.
    armed = true;
    released = entered = false;
    auto task = std::async(std::launch::async, [&] {
      return R::renderWithOptions(snapshot, limits, options, frame, status, diagnostics);
    });
    std::unique_lock<std::mutex> lock(mutex);
    const bool witnessed = changed.wait_for(lock, 5s, [] { return entered; });
    const auto start = Clock::now();
    cancel.store(true);
    released = true;
    lock.unlock();
    changed.notify_all();
    const bool bounded = task.wait_for(2s) == std::future_status::ready;
    const bool success = task.get();
    const auto ms = std::chrono::duration<double,std::milli>(Clock::now() - start).count();
    const bool interrupted = !success && status.code == R::CANCELLED;
    const bool empty = identical(frame, R::Frame{});
    armed = false;
    cancel.store(false);
    const bool recovered = R::renderWithOptions(snapshot, limits, options, frame, status, diagnostics)
                           && identical(reference, frame);
    std::cout << "trial=" << trial << " mid_trace=" << witnessed << " cancelled=" << interrupted
              << " frame_empty=" << empty << " exact_recovery=" << recovered << " join_ms=" << ms << '\n';
    if (!witnessed || !bounded || !interrupted || !empty || !recovered) return 1;
  }
  return 0;
}
