# Review the native retained ray preview

This draft targets FreeCAD/coin's integration/backend-command-profiling-wip branch at 4cc9a66417e0ad7bdaedb712dda7abf527555a5c. It extracts the native implementation and regressions from the private experiment at 28202566ea. It uses the retained DrawList already present on this base.

The host opts into the diagnostic path through SoRenderManager::setCpuRayTracingEnabled(TRUE). The private COIN_CPU_RAY_ASYNC=1 environment switch enables background execution. The GUI thread captures owned data, a CPU reference renderer processes it, and Coin checks current scene input before presentation. Unsupported passes use retained OpenGL. The public capability headers and render-manager API need maintainer review; build consumers against the matching Coin headers and library.

## Reproduce on Windows

Install Git, VS2022 Desktop C++, CMake, Python 3.10+ and a working desktop OpenGL driver. From a fresh checkout initialize the packaging submodule, then run:

    git submodule update --init cpack.d
    ./review-preview.ps1

The script configures a Release x64 shared build with C++20, builds the normal Windows unit suite and native probes, and runs CTest. The Python-based probe registrations require a discoverable Python interpreter. The review suite creates its cube scenes and expected pixel values from source. It checks retained depth preservation, pixel-store byte order, async camera replacement and scalar cancellation after an observed intersection. The generic GLFW and optional visual suites are outside this recipe.

## Requested feedback

1. Is this retained branch the appropriate dependency, and should the owned snapshot and reference renderer be proposed separately?
2. Is the render-manager capability surface appropriate for existing Coin consumers? Review lifetime, thread ownership, unsupported-pass fallback, and header/ABI implications.
3. What private compiler floor and OpenGL platforms should a first upstream contribution support?

The FreeCAD viewer adapter, alternate instance backend, scheduling experiments, build artifacts, historical test archives and machine-specific models are outside this patch. Default enablement in FreeCAD is a separate decision. Linux/macOS, driver portability, external-consumer ABI and performance, and a reviewer-run reproduction remain open.
