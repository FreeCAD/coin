// src/rendering/SoVulkanDebugUtils.h
//
// Internal VK_EXT_debug_utils helpers (object names + command-buffer labels) for
// readable RenderDoc/Nsight captures.  Not public API.
//
// Gated by diagnostics.debugUtils (COIN_VULKAN_DEBUG_UTILS); when unavailable the
// vkGetDeviceProcAddr-resolved entry points stay null and every helper is a no-op.
// Cached once for the shared VkDevice; call setDevice() for a second device.

#ifndef COIN_SOVULKANDEBUGUTILS_H
#define COIN_SOVULKANDEBUGUTILS_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#include "rendering/SoVulkanPlatform.h"
#include <vulkan/vulkan.h>

#include "rendering/SoVulkanConfig.h"

namespace SoVulkanDebugUtils {

inline bool
enabled()
{
  return SoVulkanConfig::get().diagnostics.debugUtils;
}

// Device whose vkGetDeviceProcAddr resolves the entry points; null = no-op.
inline VkDevice &
deviceRef()
{
  static VkDevice device = VK_NULL_HANDLE;
  return device;
}

inline void
setDevice(VkDevice device)
{
  deviceRef() = device;
}

struct Functions {
  PFN_vkSetDebugUtilsObjectNameEXT setName = nullptr;
  PFN_vkCmdBeginDebugUtilsLabelEXT beginLabel = nullptr;
  PFN_vkCmdEndDebugUtilsLabelEXT endLabel = nullptr;
  PFN_vkCmdInsertDebugUtilsLabelEXT insertLabel = nullptr;
};

inline const Functions &
functions()
{
  static const Functions fns = [] {
    Functions f;
    const VkDevice device = deviceRef();
    if (device == VK_NULL_HANDLE) {
      return f;
    }
    f.setName = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
      vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
    f.beginLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT"));
    f.endLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT"));
    f.insertLabel = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
      vkGetDeviceProcAddr(device, "vkCmdInsertDebugUtilsLabelEXT"));
    return f;
  }();
  return fns;
}

inline void
nameObject(VkDevice device, VkObjectType type, uint64_t handle,
           const char * name)
{
  if (!enabled() || handle == 0 || name == nullptr) {
    return;
  }
  const Functions & f = functions();
  if (f.setName == nullptr) {
    return;
  }
  VkDebugUtilsObjectNameInfoEXT info {};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  info.objectType = type;
  info.objectHandle = handle;
  info.pObjectName = name;
  f.setName(device, &info);
}

inline void
beginLabel(VkCommandBuffer commandBuffer, const char * name,
           float r = 0.25f, float g = 0.55f, float b = 0.95f, float a = 1.0f)
{
  if (!enabled() || commandBuffer == VK_NULL_HANDLE || name == nullptr) {
    return;
  }
  const Functions & f = functions();
  if (f.beginLabel == nullptr) {
    return;
  }
  VkDebugUtilsLabelEXT label {};
  label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
  label.pLabelName = name;
  label.color[0] = r;
  label.color[1] = g;
  label.color[2] = b;
  label.color[3] = a;
  f.beginLabel(commandBuffer, &label);
}

inline void
endLabel(VkCommandBuffer commandBuffer)
{
  if (!enabled() || commandBuffer == VK_NULL_HANDLE) {
    return;
  }
  const Functions & f = functions();
  if (f.endLabel != nullptr) {
    f.endLabel(commandBuffer);
  }
}

} // namespace SoVulkanDebugUtils

#endif // COIN_SOVULKANDEBUGUTILS_H
