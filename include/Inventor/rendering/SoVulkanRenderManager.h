// include/Inventor/rendering/SoVulkanRenderManager.h

#ifndef COIN_SOVULKANRENDERMANAGER_H
#define COIN_SOVULKANRENDERMANAGER_H

/*!
  The whole class is compiled only when COIN_BUILD_VULKAN_RENDERER is set (by
  Coin's own build and by applications that opt in).  Without it the header
  expands to nothing, so an installed Coin built without the Vulkan renderer
  does not force a Vulkan SDK dependency on its consumers.
*/

#ifndef COIN_BUILD_VULKAN_RENDERER
#define COIN_BUILD_VULKAN_RENDERER 0
#endif

// Public settings blob; always available (no Vulkan dependency).
#include <Inventor/rendering/SoVulkanViewSettings.h>

#if COIN_BUILD_VULKAN_RENDERER

#include <Inventor/SbColor4f.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/rendering/SoRenderIR.h>
#include <string>
#include <vector>

// Pull in Vulkan handle types for renderExternal().  This header is only
// fully compiled when COIN_BUILD_VULKAN_RENDERER is enabled.
#include <vulkan/vulkan.h>

class SbViewportRegion;
class SoCamera;
class SoNode;
class SoIRRenderAction;
class SoVulkanRenderBackend;

struct SoVulkanDeviceContext;

/*!
  \class SoVulkanRenderManager SoVulkanRenderManager.h
  \brief Qt-free scene-to-Vulkan orchestrator for the render-backend path.

  This class is the Vulkan counterpart of the legacy SoRenderManager.  It
  traverses a scene graph with SoIRRenderAction to produce a backend-neutral
  SoDrawList, then submits it to a SoVulkanRenderBackend bound to a
  SoVulkanRenderTarget supplied by the caller.

  Unlike SoRenderManager, it does not own a window system surface, a camera
  sensor, stereo handling, or superimpositions.  The caller owns the Vulkan
  device and the render target and drives render() once per frame.
*/
class COIN_DLL_API SoVulkanRenderManager {
public:
  SoVulkanRenderManager();
  ~SoVulkanRenderManager();

  void setSceneGraph(SoNode * root);
  SoNode * getSceneGraph(void) const;

  /*!
    \brief Set an optional screen-space overlay scene graph.

    The overlay scene is traversed after the main scene every frame (its
    commands are recorded into the same draw list) and drawn last in the
    overlay render pass, each command using its own view/projection matrices
    and viewport/scissor region.  Used for the navigation cube: the overlay
    node renders itself into a viewport corner without affecting the main
    scene's bounding box or camera.
  */
  void setOverlaySceneGraph(SoNode * root);
  SoNode * getOverlaySceneGraph(void) const;

  /*!
    \brief Set an optional decoration scene graph (axis cross overlay).

    Traversed after the overlay scene graph every frame (commands recorded
    into the same draw list, drawn in the overlay pass after the overlay
    scene's commands).  Like the overlay scene, its nodes carry their own
    view/projection matrices and viewport/scissor regions (screen-space
    decorations such as the axis cross).
  */
  void setDecorationSceneGraph(SoNode * root);
  SoNode * getDecorationSceneGraph(void) const;

  void setCamera(SoCamera * camera);
  SoCamera * getCamera(void) const;

  void setViewportRegion(const SbViewportRegion & region);
  const SbViewportRegion & getViewportRegion(void) const;

  /*!
    \brief Strategy for automatically adjusting the camera clipping planes.

    Mirrors SoRenderManager::AutoClippingStrategy.  With anything other than
    NO_AUTO_CLIPPING, render()/renderExternal() re-compute the camera's
    nearDistance/farDistance every frame from the scene bounding box so that
    navigation (zoom, orbit) never pushes geometry outside the view volume.
    Defaults to NO_AUTO_CLIPPING to match SoRenderManager; embedding
    applications that used the legacy GL auto-clipping should enable
    VARIABLE_NEAR_PLANE.
  */
  enum AutoClippingStrategy {
    NO_AUTO_CLIPPING,
    FIXED_NEAR_PLANE,
    VARIABLE_NEAR_PLANE
  };
  void setAutoClipping(AutoClippingStrategy strategy);
  AutoClippingStrategy getAutoClipping(void) const;

  //! Fraction of the depth range kept for the near plane (see SoRenderManager).
  void setNearPlaneValue(float value);
  float getNearPlaneValue(void) const;

  void setBackgroundColor(const SbColor4f & color);
  const SbColor4f & getBackgroundColor(void) const;

  //! Device-pixel ratio of the Vulkan surface.  The swapchain is in device
  //! pixels, so the renderer scales logical line widths / point sizes by
  //! this (see SoRenderParams::devicePixelRatio).
  void setDevicePixelRatio(float ratio);
  float getDevicePixelRatio(void) const;

  /*!
    \brief Publish a revision counter for state the scene graph cannot show.

    The camera-only-frame replay compares a fingerprint of the traversal-
    relevant scene graph against the last full traversal.  Changes made
    purely through application side models (FreeCAD's selection/preselection
    state rendered by SoFCSelectionRoot without touching node fields) do not
    move that fingerprint, so the embedding widget must bump \a revision
    whenever such state changes; any change forces a full re-traversal.
  */
  void setExternalRevision(uint64_t revision);

  /*!
    \brief Configure a vertical screen-space background gradient.

    When \a enabled is TRUE, render()/renderExternal() fills the viewport
    with a top-to-bottom gradient between \a topColor and \a bottomColor
    before drawing geometry (instead of the flat clear color).
  */
  void setBackgroundGradient(SbBool enabled,
                             const SbColor4f & topColor,
                             const SbColor4f & bottomColor);

  /*!
    \brief Configure Vulkan-only display overlays.

    These toggle the wireframe/point edge overlays and their color.  They are
    deliberately not part of the shared retained render state, so the OpenGL
    backend never consults them.
  */
  void setWireframeOverlay(SbBool enabled);
  void setPointsOverlay(SbBool enabled);
  void setTessellationOverlay(SbBool enabled);
  void setEdgeColor(const SbColor4f & color);
  SbBool getWireframeOverlay(void) const;
  SbBool getPointsOverlay(void) const;
  SbBool getTessellationOverlay(void) const;
  const SbColor4f & getEdgeColor(void) const;

  /*!
    \brief Apply the whole Vulkan viewport display/tuning settings blob.

    The manager diffs against the last applied blob and re-applies only when it
    changed, so a caller may push it every frame.  Structural state (scene,
    camera, viewport region, render target) remains a separate call.
  */
  void setViewSettings(const SoVulkanViewSettings & settings);
  //! Force the next setViewSettings() to re-apply even if unchanged.
  void invalidateViewSettings(void);

  void setClearEnabled(SbBool clearwindow, SbBool clearzbuffer);
  void getClearEnabled(SbBool & clearwindow, SbBool & clearzbuffer) const;

  /*!
    \brief Initialize the owned Vulkan backend from a device context.

    The context is borrowed; the caller must keep it alive until shutdown()
    or destruction.  Returns FALSE if the backend cannot initialize.
  */
  SbBool initialize(SoVulkanDeviceContext * context);

  /*!
    \brief Declare how many recorded frames the caller may keep in flight.

    Forwards to the raster backend (see SoVulkanRenderBackend::
    setMaxFramesInFlight()); drives deferred-resource destruction and the
    lighting UBO ring size.  Call once after initialize() and before the
    first render when the caller submits frames concurrently.
  */
  void setMaxFramesInFlight(uint32_t count);

  /*!
    \brief Path of a persistent (on-disk) Vulkan pipeline cache.

    When non-empty, initialize() loads the file's bytes as the initial
    pipeline-cache data and shutdown() writes the driver's cache blob back, so
    the lazily-created pipeline variants survive a process restart.  The
    embedding application owns the path and must create its directory; a
    missing, corrupt or stale (different device/driver) file is ignored and an
    empty cache is created.  Set it before initialize().
  */
  void setPipelineCachePath(const std::string & path);

  /*!
    \brief Shut down the owned backend while the Vulkan device/queue are
    still valid.

    Idempotent.  Call this before the window system tears down the Vulkan
    device; the manager destructor also attempts a shutdown, but that may be
    too late by then.
  */
  void shutdown(void);

  //! Render target used by the next render(); borrowed, not owned.
  void setRenderTarget(void * target);
  void * getRenderTarget(void) const;

  /*!
    \brief Traverse the scene and submit it to the Vulkan backend.

    The camera view/projection matrices are taken from the traversed scene
    state (normally produced by the camera node).  When the scene provides no
    geometry, the draw list is empty and only the clear state is applied.
  */
  SbBool render(SbBool clearwindow = TRUE, SbBool clearzbuffer = TRUE);

  /*!
    \brief Record the scene into a caller-owned command buffer/render pass.

    Mirrors render() but does not begin/end a command buffer, begin/end a
    render pass, create a framebuffer, or submit to the queue.  The caller
    must already be inside a render pass on \a commandBuffer with \a renderPass
    and a compatible framebuffer, and owns submission/presentation.

    The per-frame setup and the GPU geometry-LOD pre-pass are handled inside
    the backend, so this is the only external frame call: there is no separate
    prepare step to coordinate.  The backend records the compute pre-pass into
    a transient command buffer submitted before the caller's pass.
  */
  SbBool renderExternal(SbBool clearwindow,
                        SbBool clearzbuffer,
                        VkCommandBuffer commandBuffer,
                        VkRenderPass renderPass,
                        VkFramebuffer framebuffer);

  //! Record the frame's GPU-timestamp query reset on the caller's command
  //! buffer, immediately before vkCmdBeginRenderPass.  A caller that begins its
  //! own pass must call this so renderExternal() can write GPU timestamps inside
  //! the pass (vkCmdResetQueryPool is illegal inside one).  No-op unless GPU
  //! timing (FC_VULKAN_GPU_TIMING) is active, so it is safe to always call.
  void resetExternalGpuQueries(VkCommandBuffer commandBuffer);

  /*!
    \brief Provide the authoritative scene lighting (GL host -> both backends).

    \a lighting is the camera-anchored world-space viewer light set (headlight,
    backlight and fill light) plus the intensity-scaled scene ambient.  Fired
    through to the raster executor and the RT backend so both use the host's
    lights instead of the IR draw-list lighting capture (which can drop to zero
    lights on the retained/replayed frame, rendering surfaces at
    ambient-only/near-black).  Passing an empty light list restores the
    per-command IR lighting.
  */
  void setSceneLights(const SoLightingData & lighting);

  /*!
    \brief Enable/disable interaction LOD while the camera is moving.

    While engaged the raster backend may drop work invisible in motion (wide
    lines as 1px, sub-pixel geometry-LOD compaction) and restore full quality
    when the camera stops.
  */
  void setInteractionLod(SbBool active);

  SoVulkanRenderBackend * getBackend(void) const;


  //! Ordinal of the last presented frame (1-based; 0 before the first render).
  //! Bumped exactly once per render()/renderExternal() and copied into that
  //! frame's SoRenderParams::frame.  A stable correlation key shared by
  //! backend debug traces, captured frame dumps and probe phase markers.
  uint32_t getRenderFrameCount(void) const;

private:
  class SoVulkanRenderManagerP * pimpl;
};

#endif // COIN_BUILD_VULKAN_RENDERER

#endif // COIN_SOVULKANRENDERMANAGER_H
