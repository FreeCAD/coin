// src/rendering/SoVulkanRenderBackend/VmaImpl.cpp
//
// Compile the Vulkan Memory Allocator exactly once.  VMA is header-only; this
// translation unit provides the single implementation.  The header is not
// bundled with Coin -- a Vulkan build requires VMA to be installed on the
// system (see COIN_VMA_INCLUDE_DIR in the top-level CMakeLists.txt), and the
// include below resolves through that path.
//
// Coin links the Vulkan loader directly (find_package(Vulkan) + Vulkan::Vulkan)
// and calls the vk* entry points directly, so VMA uses the statically linked
// functions rather than resolving them through vkGetInstanceProcAddr at
// runtime.  Both macros are pinned here so the choice does not depend on
// whether VK_NO_PROTOTYPES happens to be defined elsewhere in the build.

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#include <vk_mem_alloc.h>
