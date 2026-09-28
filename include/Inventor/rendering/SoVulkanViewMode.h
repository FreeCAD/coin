// include/Inventor/rendering/SoVulkanViewMode.h

#ifndef COIN_SOVULKANVIEWMODE_H
#define COIN_SOVULKANVIEWMODE_H

/*!
  \file SoVulkanViewMode.h
  \brief Public view-mode selector shared by the renderer and its embedding
  application.

  The mode crosses the application/renderer boundary as one type, so it is
  compiler-checked instead of being passed as a magic int.  This port carries
  the raster Vulkan renderer only; the enum keeps a single value so the raster
  path names its mode rather than passing a raw int.

  This header is always available (it has no Vulkan dependency) so an
  application can name the type even in a build without the Vulkan renderer.
*/
enum class SoVulkanViewMode : int {
  Raster = 0,
};

#endif // COIN_SOVULKANVIEWMODE_H
