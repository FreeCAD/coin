#ifndef COIN_SOCPURAYPRESENTERP_H
#define COIN_SOCPURAYPRESENTERP_H
#include <string>
struct cc_glglue;
namespace coin_raytrace { struct Frame; }
// Private retained-context owner. Call shutdown with its owning context current;
// use discard only after context loss. The destructor makes no GL calls.
class SoCpuRayPresenter {
public:
  SoCpuRayPresenter();
  ~SoCpuRayPresenter();
  SoCpuRayPresenter(const SoCpuRayPresenter &) = delete;
  SoCpuRayPresenter & operator=(const SoCpuRayPresenter &) = delete;
  bool prepare(const cc_glglue * glue, std::string & detail);
  bool present(const coin_raytrace::Frame & frame, std::string & detail);
  void shutdown();
  void discard();
private:
  struct Impl;
  Impl * p;
};
#endif
