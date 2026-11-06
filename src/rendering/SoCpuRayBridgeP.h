#ifndef COIN_SOCPURAYBRIDGEP_H
#define COIN_SOCPURAYBRIDGEP_H
#include "rendering/SoRenderBackend.h"
#include "rendering/SoRayTraceRendererP.h"
#include <Inventor/C/glue/gl.h>
#include <memory>
#include <string>
#include <vector>

struct SoCpuRayPreparedFrame {
  coin_raytrace::Frame frame;
  std::vector<unsigned char> skipCommands;
  std::string accelerationDetail;
};

// Owned by one render manager. Only the owned snapshot and cancellation
// token reach the background coordinator.
class SoCpuRayAsync {
public:
  SoCpuRayAsync();
  ~SoCpuRayAsync();
  SoCpuRayAsync(const SoCpuRayAsync &) = delete;
  SoCpuRayAsync & operator=(const SoCpuRayAsync &) = delete;

  bool prepare(const SoDrawList &, const SoRenderParams &, int depthBits,
               SoCpuRayPreparedFrame &, std::string &);
  void invalidate() noexcept;
  // Owning GUI-thread notification path: no traversal, wait or ready reset.
  // Scene mutation and manager lifetime must be serialized on that thread.
  void requestCancel() noexcept;
  void published() noexcept;
  unsigned pollMilliseconds() const noexcept;
  std::string telemetry() const;

private:
  struct State;
  std::unique_ptr<State> state;
};

bool coin_cpu_ray_async_enabled() noexcept;
bool coin_prepare_cpu_ray_frame(const SoDrawList & source,
                               const SoRenderParams & params,
                               SoCpuRayPreparedFrame & output,
                               std::string & detail);
bool coin_readback_cpu_ray_target(const cc_glglue * glue,
                                 SoCpuRayTracingResult & result,
                                 std::string & detail);
#endif
