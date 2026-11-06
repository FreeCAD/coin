// Included only by SoRayTraceRenderer.cpp, inside coin_raytrace. This private
// implementation owns no scene nodes, Qt objects or OpenGL resources.
namespace {
using Clock = std::chrono::steady_clock;
std::atomic<unsigned> liveWorkers(0);

// One bounded record on first invalid large-profile triangle; never a scene
// dump. Object positions distinguish true producer duplicates from CPU-rounded
// world collapse. Diagnostic profile and successful frames stay unchanged.
struct TriangleFailure : Failure {
  std::string context;
  TriangleFailure(const Failure & failure, const Triangle & triangle,
                  std::size_t instance, std::size_t index)
    : Failure(failure.code, failure.detail) {
    std::ostringstream text; text.imbue(std::locale::classic()); text << std::setprecision(17);
    text << failure.detail << "; COIN_INVALID_TRIANGLE_V1 instance=" << instance << " triangle=" << index;
    auto vector = [&](const Vec3 & v) { text << '[' << v[0] << ',' << v[1] << ',' << v[2] << ']'; };
    for (const auto & named : {std::make_pair(" object=", &triangle.positionsObject),
                              std::make_pair(" world=", &triangle.positionsWorld)}) {
      text << named.first; for (const auto & point : *named.second) vector(point);
      const auto normal = cross(sub((*named.second)[1], (*named.second)[0]),
                                sub((*named.second)[2], (*named.second)[0]));
      text << " cross="; vector(normal); text << " squared=" << dot(normal, normal);
    }
    context = text.str();
    require(context.size() <= 2048, RENDER_FAILURE, "triangle diagnostic byte bound");
  }
};

struct FloatingEnvironment {
  std::fenv_t value;
#if defined(COIN_RAYTRACE_X86_FP)
  unsigned mxcsr;
#endif
  FloatingEnvironment() {
    require(std::fegetenv(&value) == 0, UNSUPPORTED_STATE, "cannot capture floating environment");
#if defined(COIN_RAYTRACE_X86_FP)
    mxcsr = _mm_getcsr();
#endif
  }
};
struct WorkerFloatingEnvironment {
  bool active;
  std::fenv_t saved;
#if defined(COIN_RAYTRACE_X86_FP)
  unsigned savedMxcsr;
#endif
  WorkerFloatingEnvironment(const FloatingEnvironment & caller, bool additional) : active(false) {
    if (!additional) return; // Do not reset the GUI caller's sticky status flags.
    require(std::fegetenv(&saved) == 0, UNSUPPORTED_STATE, "cannot save worker floating environment");
#if defined(COIN_RAYTRACE_X86_FP)
    savedMxcsr = _mm_getcsr();
#endif
    active = true;
    if (std::fesetenv(&caller.value) != 0) {
      restore(); throw Failure(UNSUPPORTED_STATE, "cannot install worker floating environment");
    }
#if defined(COIN_RAYTRACE_X86_FP)
    _mm_setcsr(caller.mxcsr); // Includes FTZ/DAZ, not just C rounding state.
#endif
  }
  bool restore() noexcept {
    if (!active) return true;
    const bool okay = std::fesetenv(&saved) == 0;
#if defined(COIN_RAYTRACE_X86_FP)
    _mm_setcsr(savedMxcsr);
#endif
    active = false; return okay;
  }
  ~WorkerFloatingEnvironment() { restore(); }
};

struct Bounds { Vec3 low, high; };
struct Primitive {
  std::size_t instance, triangle, order;
  std::array<Vec3, 3> positions;
  Bounds bounds;
};
struct BvhNode {
  Bounds bounds;
  std::uint32_t begin = 0, count = 0, left = 0, right = 0;
};
struct Acceleration {
  std::uint64_t hash = 0, generation = 0;
  std::vector<std::size_t> instanceCounts;
  std::vector<Primitive> primitives;
  std::vector<BvhNode> nodes;
  std::size_t bytes = 0;
};
struct CacheState {
  std::timed_mutex mutex;
  std::shared_ptr<const Acceleration> entry;
  std::uint64_t builds = 0, hits = 0;
};
CacheState & cacheState() { static CacheState state; return state; }

void checkControl(const RenderOptions & options, Clock::time_point started,
                  const Limits & limits) {
  require(!options.cancel || !options.cancel->load(std::memory_order_acquire),
          CANCELLED, "ray tracing cancelled");
  require(Clock::now() - started <= std::chrono::milliseconds(limits.maxMilliseconds),
          TIME_LIMIT, "ray tracing deadline");
}

Bounds triangleBounds(const Triangle & triangle, const Limits & limits) {
  Bounds result{triangle.positionsWorld[0], triangle.positionsWorld[0]};
  for (int axis = 0; axis != 3; ++axis) {
    double scale = 0;
    for (int vertex = 0; vertex != 3; ++vertex) {
      const double value = triangle.positionsWorld[vertex][axis];
      require(std::isfinite(value) && std::fabs(value) <= limits.maxMagnitude,
              INVALID_GEOMETRY, "invalid world triangle position");
      result.low[axis] = std::min(result.low[axis], value);
      result.high[axis] = std::max(result.high[axis], value);
      scale = std::max(scale, std::fabs(value));
    }
    // The scalar barycentric inequalities admit weights slightly outside the
    // geometric triangle. Enclose those extrapolations, plus coordinate ULPs.
    const double span = result.high[axis] - result.low[axis];
    const double padding = 4 * epsilon * span +
      64 * std::numeric_limits<double>::epsilon() * std::max(1.0, scale + span);
    result.low[axis] = std::nextafter(result.low[axis] - padding,
                                    -std::numeric_limits<double>::infinity());
    result.high[axis] = std::nextafter(result.high[axis] + padding,
                                     std::numeric_limits<double>::infinity());
  }
  const Vec3 normal = cross(sub(triangle.positionsWorld[1], triangle.positionsWorld[0]),
                            sub(triangle.positionsWorld[2], triangle.positionsWorld[0]));
  const double length = std::sqrt(dot(normal, normal));
  require((std::isfinite(length) && length > 0) ||
          (limits.profile == RenderProfile::LARGE_REVIEW && repeatedProducerVertex(triangle, limits.maxMagnitude)),
          INVALID_GEOMETRY, "degenerate world triangle");
  return result;
}

std::uint64_t geometryHash(const Snapshot & snapshot, const RenderOptions & options,
                           Clock::time_point started, const Limits & limits) {
  std::uint64_t hash = 14695981039346656037ULL;
  auto append = [&hash](std::uint64_t value) {
    for (unsigned i = 0; i != 8; ++i) { hash ^= (value >> (8 * i)) & 255; hash *= 1099511628211ULL; }
  };
  append(snapshot.instances.size());
  std::size_t visited = 0;
  for (const auto & instance : snapshot.instances) {
    append(instance.triangles.size());
    for (const auto & triangle : instance.triangles) {
      if ((visited++ & 63) == 0) checkControl(options, started, limits);
      for (const auto & position : triangle.positionsWorld)
        for (double value : position) {
          std::uint64_t bits;
          static_assert(sizeof(bits) == sizeof(value), "64-bit double required");
          std::memcpy(&bits, &value, sizeof(bits)); append(bits);
        }
    }
  }
  return hash;
}

bool sameGeometry(const Acceleration & entry, const Snapshot & snapshot,
                  const RenderOptions & options, Clock::time_point started, const Limits & limits) {
  if (entry.instanceCounts.size() != snapshot.instances.size()) return false;
  for (std::size_t i = 0; i != snapshot.instances.size(); ++i)
    if (entry.instanceCounts[i] != snapshot.instances[i].triangles.size()) return false;
  // Primitives have been reordered by the BVH. Their original indices remain
  // authoritative; shading and semantic path data are never stored here.
  std::size_t visited = 0;
  for (const auto & primitive : entry.primitives) {
    if ((visited++ & 63) == 0) checkControl(options, started, limits);
    if (primitive.positions != snapshot.instances[primitive.instance].triangles[primitive.triangle].positionsWorld)
      return false;
  }
  return entry.primitives.size() == snapshot.triangleCount;
}

Bounds combined(const Bounds & a, const Bounds & b) {
  Bounds result;
  for (int k = 0; k != 3; ++k) {
    result.low[k] = std::min(a.low[k], b.low[k]); result.high[k] = std::max(a.high[k], b.high[k]);
  }
  return result;
}

std::uint32_t buildNode(Acceleration & entry, std::size_t begin, std::size_t count,
                        const RenderOptions & options, Clock::time_point started,
                        const Limits & limits, unsigned depth = 0) {
  checkControl(options, started, limits);
  require(count > 0 && depth < 32 && entry.nodes.size() < 2 * entry.primitives.size(),
          RENDER_FAILURE, "BVH construction bound");
  const auto index = static_cast<std::uint32_t>(entry.nodes.size());
  entry.nodes.emplace_back();
  Bounds bounds = entry.primitives[begin].bounds;
  for (std::size_t i = begin + 1; i < begin + count; ++i) {
    if ((i & 4095) == 0) checkControl(options, started, limits);
    bounds = combined(bounds, entry.primitives[i].bounds);
  }
  entry.nodes[index].bounds = bounds;
  if (count <= 4) {
    entry.nodes[index].begin = static_cast<std::uint32_t>(begin);
    entry.nodes[index].count = static_cast<std::uint32_t>(count);
  } else {
    int axis = 0;
    for (int k = 1; k != 3; ++k)
      if (bounds.high[k] - bounds.low[k] > bounds.high[axis] - bounds.low[axis]) axis = k;
    auto first = entry.primitives.begin() + begin;
    std::size_t comparisons = 0;
    auto before = [&](const Primitive & a, const Primitive & b) {
      if ((++comparisons & 4095) == 0) checkControl(options, started, limits);
      const double ac = a.bounds.low[axis] + .5 * (a.bounds.high[axis] - a.bounds.low[axis]);
      const double bc = b.bounds.low[axis] + .5 * (b.bounds.high[axis] - b.bounds.low[axis]);
      return ac < bc || (ac == bc && a.order < b.order);
    };
    const std::size_t half = count / 2;
    if (limits.profile == RenderProfile::LARGE_REVIEW)
      std::nth_element(first, first + half, first + count, before);
    else
      std::stable_sort(first, first + count, before);
    const auto left = buildNode(entry, begin, half, options, started, limits, depth + 1);
    const auto right = buildNode(entry, begin + half, count - half, options, started, limits, depth + 1);
    entry.nodes[index].left = left; entry.nodes[index].right = right;
  }
  return index;
}

std::shared_ptr<const Acceleration> acceleration(const Snapshot & snapshot, const Limits & limits,
                                                const RenderOptions & options, Clock::time_point started,
                                                RenderDiagnostics & diagnostics) {
  // Validate every primitive even if offscreen or a prior cache entry exists.
  std::size_t count = 0, instanceIndex = 0;
  for (const auto & instance : snapshot.instances) {
    require(instance.triangles.size() <= limits.maxTriangles - count, TRIANGLE_LIMIT, "snapshot triangle count");
    std::size_t triangleIndex = 0;
    for (const auto & triangle : instance.triangles) {
      if (count % 64 == 0) checkControl(options, started, limits);
      try { triangleBounds(triangle, limits); }
      catch (const Failure & failure) {
        if (limits.profile == RenderProfile::LARGE_REVIEW && failure.code == INVALID_GEOMETRY)
          throw TriangleFailure(failure, triangle, instanceIndex, triangleIndex);
        throw;
      }
      ++count; ++triangleIndex;
    }
    ++instanceIndex;
  }
  require(count == snapshot.triangleCount, INVALID_GEOMETRY, "snapshot count mismatch");
  const std::size_t byteBound = snapshot.instances.size() * sizeof(std::size_t) +
                              count * (sizeof(Primitive) + 2 * sizeof(BvhNode));
  require(byteBound <= limits.maxAccelerationBytes, ALLOCATION_FAILURE, "BVH cache byte bound");
  const std::uint64_t hash = options.forceGeometryHashForTesting ? options.geometryHashForTesting : geometryHash(snapshot, options, started, limits);
  CacheState & state = cacheState();
  std::unique_lock<std::timed_mutex> lock(state.mutex, std::defer_lock);
  while (!lock.try_lock_for(std::chrono::milliseconds(1))) checkControl(options, started, limits);
  checkControl(options, started, limits);
  if (state.entry && state.entry->bytes <= limits.maxAccelerationBytes && state.entry->hash == hash &&
      sameGeometry(*state.entry, snapshot, options, started, limits)) {
    require(state.hits != std::numeric_limits<std::uint64_t>::max(), RENDER_FAILURE, "cache hit counter bound");
    ++state.hits; diagnostics.cacheHit = true;
  } else {
    auto entry = std::make_shared<Acceleration>();
    entry->hash = hash;
    entry->instanceCounts.reserve(snapshot.instances.size());
    entry->primitives.reserve(count); entry->nodes.reserve(count * 2);
    std::size_t order = 0;
    for (std::size_t i = 0; i != snapshot.instances.size(); ++i) {
      const auto & instance = snapshot.instances[i]; entry->instanceCounts.push_back(instance.triangles.size());
      for (std::size_t t = 0; t != instance.triangles.size(); ++t) {
        if ((t & 63) == 0) checkControl(options, started, limits);
        const auto & triangle = instance.triangles[t];
        entry->primitives.push_back(Primitive{i, t, order++, triangle.positionsWorld, triangleBounds(triangle, limits)});
      }
    }
    if (count) buildNode(*entry, 0, count, options, started, limits);
    require(state.builds != std::numeric_limits<std::uint64_t>::max(), RENDER_FAILURE, "cache build counter bound");
    entry->generation = ++state.builds;
    entry->bytes = entry->instanceCounts.capacity() * sizeof(std::size_t) +
                   entry->primitives.capacity() * sizeof(Primitive) + entry->nodes.capacity() * sizeof(BvhNode);
    require(entry->bytes <= limits.maxAccelerationBytes, ALLOCATION_FAILURE, "allocated BVH cache byte bound");
    state.entry = std::move(entry);
  }
  const auto & entry = state.entry;
  diagnostics.geometryHash = hash; diagnostics.cacheGeneration = entry->generation;
  diagnostics.cacheBuilds = state.builds; diagnostics.cacheHits = state.hits;
  diagnostics.cacheBytes = entry->bytes; diagnostics.bvhNodes = entry->nodes.size();
  return entry;
}

bool intersectsBounds(const Bounds & bounds, const Vec3 & origin, const Vec3 & direction, double length) {
  double entry = 0, exit = length;
  for (int axis = 0; axis != 3; ++axis) {
    if (direction[axis] == 0) {
      if (origin[axis] < bounds.low[axis] || origin[axis] > bounds.high[axis]) return false;
    } else {
      double a = (bounds.low[axis] - origin[axis]) / direction[axis];
      double b = (bounds.high[axis] - origin[axis]) / direction[axis];
      if (a > b) std::swap(a, b);
      entry = std::max(entry, a); exit = std::min(exit, b);
      if (entry > exit) return false;
    }
  }
  return true;
}

struct BvhCandidate { Candidate hit; std::size_t order; };
bool stableIdentityLess(const BvhCandidate & a, const BvhCandidate & b) {
  return identityLess(a.hit, b.hit) ||
         (!identityLess(b.hit, a.hit) && !identityLess(a.hit, b.hit) && a.order < b.order);
}
struct WorkerCounts { std::size_t boxes = 0, triangles = 0, hits = 0, tiles = 0; };
struct WorkerLifetime {
  std::atomic<unsigned> & started, & completed, & active;
  WorkerLifetime(std::atomic<unsigned> & s, std::atomic<unsigned> & c, std::atomic<unsigned> & a)
    : started(s), completed(c), active(a) { ++started; ++active; ++liveWorkers; }
  ~WorkerLifetime() { --liveWorkers; --active; ++completed; }
};
struct JoinWorkers {
  std::vector<std::thread> & threads;
  void join() { for (auto & thread : threads) if (thread.joinable()) thread.join(); }
  ~JoinWorkers() { join(); }
};
} // anonymous namespace

void clearAccelerationCache() {
  CacheState & state = cacheState();
  std::lock_guard<std::timed_mutex> lock(state.mutex); state.entry.reset();
}
unsigned activeWorkerCountForTesting() { return liveWorkers.load(std::memory_order_acquire); }

bool renderWithOptions(const Snapshot & snapshot, const Limits & limits, const RenderOptions & options,
                       Frame & frame, Status & status, RenderDiagnostics & diagnostics) {
  frame = Frame(); status = Status(); diagnostics = RenderDiagnostics();
  if (options.algorithm == SCALAR_REFERENCE) {
    if (options.cancel && options.cancel->load(std::memory_order_acquire)) {
      status.code = CANCELLED;
      try { status.detail = "ray tracing cancelled"; } catch (...) { status.code = ALLOCATION_FAILURE; }
      return false;
    }
    const bool success = renderScalar(snapshot, limits, frame, status, options.cancel);
    diagnostics.triangleTests = frame.rayTests; return success;
  }
  try {
    const auto started = Clock::now();
    const FloatingEnvironment callerEnvironment;
    require(options.algorithm == BVH_SINGLE || options.algorithm == BVH_WORKERS,
            UNSUPPORTED_STATE, "unknown ray tracing algorithm");
    require(options.maxWorkers >= 1 && options.maxWorkers <= 8 && options.tilePixels >= 1 && options.tilePixels <= 4096 &&
            options.failWorkerStartAfter >= -1 && options.failWorkerStartAfter <= 7,
            UNSUPPORTED_STATE, "invalid private worker options");
    require(validRenderLimits(limits), UNSUPPORTED_STATE, "invalid private render limits");
    require(snapshot.cameraValid, INVALID_CAMERA, "missing captured camera");
    require(snapshot.instances.size() <= limits.maxInstances && snapshot.triangleCount <= limits.maxTriangles,
            TRIANGLE_LIMIT, "snapshot geometry budget");
    validateSnapshotPayload(snapshot, limits);
    const auto dimensions = snapshot.camera.viewport.getViewportSizePixels();
    const int width = dimensions[0], height = dimensions[1];
    require(width > 0 && height > 0 && width <= limits.maxWidth && height <= limits.maxHeight &&
            std::size_t(width) <= limits.maxPixels / std::size_t(height), PIXEL_LIMIT, "frame dimensions");
    const std::size_t pixels = std::size_t(width) * std::size_t(height);
    checkControl(options, started, limits);
    auto entry = acceleration(snapshot, limits, options, started, diagnostics);
    Frame output; output.width = width; output.height = height;
    output.origin = snapshot.camera.viewport.getViewportOriginPixels();
    output.rgb.assign(pixels * 3, 0); output.depth.assign(pixels, 1); output.covered.assign(pixels, 0);
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const unsigned workers = options.algorithm == BVH_SINGLE ? 1 :
      std::min<unsigned>(std::min(options.maxWorkers, hardware), static_cast<unsigned>((pixels + options.tilePixels - 1) / options.tilePixels));
    diagnostics.workersUsed = workers;
    const std::size_t maximumCandidates = limits.maxWorkerScratchBytes / workers / sizeof(BvhCandidate);
    require(maximumCandidates > 0 || snapshot.triangleCount == 0,
            ALLOCATION_FAILURE, "worker hit scratch bound");
    std::vector<WorkerCounts> counts(workers);
    std::atomic<std::size_t> next(0), reservedTests(0);
    std::atomic<bool> stop(false);
    std::atomic<unsigned> workerStarted(0), workerCompleted(0), workerActive(0);
    std::mutex errorMutex;
    Code errorCode = OK; const char * errorDetail = "";
    auto recordFailure = [&](Code code, const char * detail) {
      std::lock_guard<std::mutex> lock(errorMutex);
      if (errorCode == OK) { errorCode = code; errorDetail = detail; }
      stop.store(true, std::memory_order_release);
    };
    auto worker = [&](unsigned workerIndex) {
      WorkerLifetime lifetime(workerStarted, workerCompleted, workerActive);
      try {
        WorkerFloatingEnvironment floatingEnvironment(callerEnvironment, workerIndex != 0);
        const Camera camera = snapshot.camera;
        WorkerCounts & local = counts[workerIndex];
        std::vector<BvhCandidate> candidates;
        const bool boundedTies = limits.profile == RenderProfile::LARGE_REVIEW;
        const std::size_t initialCandidates = boundedTies ?
          std::min<std::size_t>(64, std::min(snapshot.triangleCount, maximumCandidates)) : snapshot.triangleCount;
        require(initialCandidates <= maximumCandidates, ALLOCATION_FAILURE, "worker initial scratch bound");
        candidates.reserve(initialCandidates);
        while (!stop.load(std::memory_order_acquire)) {
          const std::size_t begin = next.fetch_add(options.tilePixels, std::memory_order_relaxed);
          if (begin >= pixels) break;
          for (std::size_t pixel = begin; pixel < std::min(pixels, begin + options.tilePixels); ++pixel) {
            if (pixel % 64 == 0) { checkControl(options, started, limits); if (stop.load()) break; }
            const int x = int(pixel % width), y = int(pixel / width);
            SbVec3d start, finish;
            camera.volume.getDPViewVolume().projectPointToLine(SbVec2d((x + .5) / width, (y + .5) / height), start, finish);
            const Vec3 origin = vec(start, limits.maxMagnitude);
            const Vec3 delta = sub(vec(finish, limits.maxMagnitude), origin);
            const double length = std::sqrt(dot(delta, delta));
            require(std::isfinite(length) && length > 0, INVALID_CAMERA, "zero camera ray segment");
            const Vec3 direction = mul(delta, 1 / length);
            candidates.clear(); double nearest = std::numeric_limits<double>::infinity();
            double nearestLimit = length;
            std::array<std::uint32_t, 64> stack;
            std::size_t size = 0;
            if (!entry->nodes.empty()) stack[size++] = 0;
            while (size) {
              const auto & node = entry->nodes[stack[--size]]; ++local.boxes;
              if (boundedTies && (local.boxes & 255) == 0) {
                checkControl(options, started, limits);
                require(!stop.load(std::memory_order_acquire), CANCELLED, "peer ray worker stopped");
              }
              if (!intersectsBounds(node.bounds, origin, direction, nearestLimit)) continue;
              if (node.count) {
                // Reserve exactly this leaf's unconditional tests, rather than
                // an unused per-thread quota or a contended atomic per triangle.
                std::size_t before = reservedTests.load(std::memory_order_relaxed);
                do {
                  require(before <= limits.maxRayTests && node.count <= limits.maxRayTests - before,
                          RAY_TEST_LIMIT, "actual ray/triangle test budget");
                } while (!reservedTests.compare_exchange_weak(before, before + node.count, std::memory_order_relaxed));
                for (std::uint32_t i = 0; i != node.count; ++i) {
                  const auto & primitive = entry->primitives[node.begin + i];
                  BvhCandidate candidate;
                  candidate.order = primitive.order;
                  candidate.hit.instance = &snapshot.instances[primitive.instance];
                  candidate.hit.triangle = &candidate.hit.instance->triangles[primitive.triangle];
                  candidate.hit.triangleIndex = primitive.triangle; ++local.triangles;
                  if (intersect(*candidate.hit.triangle, origin, direction, length, candidate.hit.t, candidate.hit.weights,
                                limits.profile == RenderProfile::LARGE_REVIEW, limits.maxMagnitude)) {
                    nearest = std::min(nearest, candidate.hit.t);
                    if (entry->nodes.size() > 1) {
                      // Preserve all global-nearest epsilon ties and round
                      // conservatively outward without a per-node CRT call.
                      const double tied = nearest + epsilon;
                      const double rounding = 64 * std::numeric_limits<double>::epsilon() *
                                              std::max(1.0, std::fabs(tied));
                      nearestLimit = std::min(length, tied + rounding);
                    }
                    if (boundedTies) {
                      // The final tie set is around the global minimum. A hit
                      // already outside the current minimum's epsilon can
                      // never re-enter as the minimum decreases.
                      // Do not scan/compact prior candidates here: coincident
                      // geometry would make that work quadratic in hit count.
                      // Prior hits are filtered once against the final minimum.
                      if (candidate.hit.t - nearest > epsilon) continue;
                    }
                    require(candidates.size() < maximumCandidates, ALLOCATION_FAILURE, "worker tie scratch exhausted");
                    if (candidates.size() == candidates.capacity()) {
                      const auto nextCapacity = std::min(maximumCandidates,
                        std::max<std::size_t>(1, candidates.capacity() * 2));
                      candidates.reserve(nextCapacity);
                      require(candidates.capacity() <= maximumCandidates, ALLOCATION_FAILURE, "allocated worker scratch bound");
                    }
                    candidates.push_back(candidate);
                  }
                }
              } else {
                require(size + 2 <= stack.size(), RENDER_FAILURE, "BVH traversal stack bound");
                stack[size++] = node.right; stack[size++] = node.left;
              }
            }
            if (candidates.empty()) continue;
            const BvhCandidate * selected = nullptr;
            std::size_t inspected = 0;
            for (const auto & candidate : candidates) {
              if (boundedTies && (++inspected & 255) == 0) checkControl(options, started, limits);
              if (candidate.hit.t - nearest <= epsilon && (!selected || stableIdentityLess(candidate, *selected))) selected = &candidate;
            }
            require(selected != nullptr, RENDER_FAILURE, "missing nearest intersection");
            const auto & hit = selected->hit;
            const Vec3 position = add(origin, mul(direction, hit.t));
            const Vec3 normal = unit(interpolate(hit.triangle->normalsWorld, hit.weights));
            const Material material = interpolateMaterial(hit.triangle->materials, hit.weights);
            const Vec3 rgb = shade(material, normal, direction);
            for (int c = 0; c != 3; ++c) output.rgb[3 * pixel + c] = static_cast<unsigned char>(std::floor(rgb[c] * 255 + .5));
            output.depth[pixel] = static_cast<float>(windowDepth(position, camera)); output.covered[pixel] = 1; ++local.hits;
          }
          ++local.tiles;
        }
        require(floatingEnvironment.restore(), RENDER_FAILURE, "cannot restore worker floating environment");
      } catch (const Failure & failure) { recordFailure(failure.code, failure.detail); }
        catch (const std::bad_alloc &) { recordFailure(ALLOCATION_FAILURE, "worker allocation failure"); }
        catch (...) { recordFailure(RENDER_FAILURE, "worker exception"); }
    };
    std::vector<std::thread> threads; threads.reserve(workers - 1);
    JoinWorkers joiner{threads};
    try {
      for (unsigned i = 1; i < workers; ++i) {
        require(options.failWorkerStartAfter != int(threads.size()), RENDER_FAILURE, "injected worker start failure");
        threads.emplace_back(worker, i); ++diagnostics.nativeThreadsStarted;
      }
      require(options.failWorkerStartAfter != int(threads.size()), RENDER_FAILURE, "injected worker start failure");
      worker(0);
    } catch (const Failure & failure) { recordFailure(failure.code, failure.detail); }
      catch (const std::bad_alloc &) { recordFailure(ALLOCATION_FAILURE, "worker startup allocation failure"); }
      catch (...) { recordFailure(RENDER_FAILURE, "worker startup failure"); }
    joiner.join();
    diagnostics.workersStarted = workerStarted.load(); diagnostics.workersCompleted = workerCompleted.load();
    diagnostics.activeWorkers = workerActive.load();
    for (const auto & local : counts) {
      diagnostics.boxTests += local.boxes; diagnostics.triangleTests += local.triangles;
      diagnostics.tileCount += local.tiles; output.hitCount += local.hits; output.rayTests += local.triangles;
    }
    require(errorCode == OK, errorCode, errorDetail);
    checkControl(options, started, limits);
    require(diagnostics.activeWorkers == 0 && diagnostics.workersStarted == diagnostics.workersCompleted,
            RENDER_FAILURE, "worker lifetime mismatch");
    frame = std::move(output); return true;
  } catch (const TriangleFailure & failure) {
    status.code = failure.code;
    try { status.detail = failure.context; } catch (...) { status.code = ALLOCATION_FAILURE; }
  } catch (const Failure & failure) {
    status.code = failure.code;
    try { status.detail = failure.detail; } catch (...) { status.code = ALLOCATION_FAILURE; }
  } catch (const std::bad_alloc &) { status.code = ALLOCATION_FAILURE; }
    catch (...) { status.code = RENDER_FAILURE; }
  return false;
}

bool render(const Snapshot & snapshot, const Limits & limits, Frame & frame, Status & status) {
  RenderDiagnostics diagnostics;
  return renderWithOptions(snapshot, limits, RenderOptions(), frame, status, diagnostics);
}
