# Review the native retained ray preview

This draft targets FreeCAD/coin's integration/backend-command-profiling-wip branch at 4cc9a66417e0ad7bdaedb712dda7abf527555a5c. It extracts the native implementation and regressions from the private experiment at 28202566ea. It uses the retained DrawList already present on this base.

The host opts into the diagnostic path through SoRenderManager::setCpuRayTracingEnabled(TRUE). The private COIN_CPU_RAY_ASYNC=1 environment switch enables background execution. The GUI thread captures owned data, a CPU reference renderer processes it, and Coin checks current scene input before presentation. Unsupported passes use retained OpenGL. The public capability headers and render-manager API need maintainer review; build consumers against the matching Coin headers and library.

## Design lineage and scope

The preview builds on the scene graph, actions and OpenGL implementation maintained by the Coin3D community. Coin credits Kongsberg Oil & Gas Technologies AS in its [license](COPYING).

* Joao Matos (tritao) and the FreeCAD/coin contributors developed the retained rendering stack. This preview consumes their DrawList, resource handles, instance IDs, and subelement ranges. Lgt2x opened the upstream coordination discussion, where VolkerEnderlein offered integration support for that modernization effort. [Retained interface at the base commit](https://github.com/FreeCAD/coin/blob/4cc9a66417e0ad7bdaedb712dda7abf527555a5c/include/Inventor/rendering/SoRenderIR.h) · [Coin3D discussion #639](https://github.com/orgs/coin3d/discussions/639)
* Clifford Yapp (starseeker) and the Obol project provide an independent example of collecting a Coin scene for other renderers. In our two-instance comparison, Obol's collector preserved world-space geometry and material after scene teardown. Our callback prototype also preserved the occurrences, but duplicated the mesh payload. The retained path preserved one mesh plus two instance commands. [Obol collector](https://github.com/starseeker/obol/blob/3f3df2e70d9bec0bbebdf4e443af5770e1d186a1/include/Inventor/SoSceneCollector.h) · [Obol viewer](https://github.com/starseeker/obol/blob/3f3df2e70d9bec0bbebdf4e443af5770e1d186a1/examples/fltk/obol_viewer.cpp) · [Comparison and limits](https://github.com/coin3d/coin/discussions/840#discussioncomment-19449303)
* Dikluwe described a separate CoinRender experiment connecting BGFX and Rust/wgpu executors to the retained planner. Its author reports synchronous offscreen tests on opaque, unlit geometry. That work illustrates how multiple backends can share Coin's traversal and execution order. [Author's account and tested scope](https://github.com/coin3d/coin/discussions/837#discussioncomment-19274519)

This draft contributes the owned capture, asynchronous CPU reference preview and its regression tests on top of the FreeCAD fork of Coin3D. The contract keeps geometry in mesh-local coordinates and carries transform, material and effective face ranges per occurrence. A future executor can reuse those inputs. The host supplies CAD names and resolves topology changes.

The headless CTest case `ray_face_provenance` models a pattern observed on a FreeCAD crankcase. Fourteen triangles belong to one face, then the next triangle belongs to a different face. A second occurrence of the mesh supplies its own face IDs. After an edit removes those IDs, a worker checks both captures following producer destruction. This checks retained numeric IDs and their lifetime. Native FreeCAD selection and topology changes require separate tests.

## Reproduce on Windows

Install Git, VS2022 Desktop C++, CMake, Python 3.10+ and a working desktop OpenGL driver. From a fresh checkout initialize the packaging submodule, then run:

    git submodule update --init cpack.d
    ./review-preview.ps1

The script configures a Release x64 shared build with C++20, builds the normal Windows unit suite and native probes, and runs CTest. The Python-based probe registrations require a discoverable Python interpreter. The review suite creates its cube scenes and expected pixel values from source. It checks retained depth preservation, pixel-store byte order, async camera replacement and scalar cancellation after an observed intersection. The generic GLFW and optional visual suites are outside this recipe.

## Requested feedback

1. Is this retained branch the appropriate dependency, and should the owned snapshot and reference renderer be proposed separately?
2. Is the render-manager capability surface appropriate for existing Coin consumers? Review lifetime, thread ownership, unsupported-pass fallback, and header/ABI implications.
3. What private compiler floor and OpenGL platforms should a first upstream contribution support?

The FreeCAD viewer adapter, alternate instance backend, scheduling experiments, build artifacts, historical test archives and machine-specific models are outside this patch. Default enablement in FreeCAD is a separate decision. Earlier local trials covered a Windows SDK consumer, a 1,024-link private FreeCAD host and a Linux software-rendering build. An instanced indexed-edge pick failed on both the Linux test branch and its public base. One private-host focus-loss teardown timed out. macOS, cross-driver display behavior, broader ABI compatibility and independent reproduction remain open.
