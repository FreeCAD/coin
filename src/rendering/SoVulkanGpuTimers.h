// src/rendering/SoVulkanGpuTimers.h
//
// Per-pass GPU timestamps for the Vulkan renderer.  Internal, not public API.
// A VkQueryPool of VK_QUERY_TYPE_TIMESTAMP queries ringed over kRingFrames, so
// results are read only once complete (no pipeline stall).  Gated by
// diagnostics.gpuTimestamps (COIN_VULKAN_GPU_TIMING); disabled, or without device/
// queue-family timestamp support, every method is a no-op.  Callers bracket passes
// with beginScope()/endScope(), then endFrame(): it reads back the frame
// kRingFrames-1 old and prints "[RTDBG] gpuTiming <scope>=<ms>" (cf. cpuTimingRaster).

#ifndef COIN_SOVULKANGPUTIMERS_H
#define COIN_SOVULKANGPUTIMERS_H

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

#include <cstdint>

#include "rendering/SoVulkanPlatform.h"
#include <vulkan/vulkan.h>

class SoVulkanGpuTimers {
public:
  //! Max scopes per frame; excess scopes are dropped, not misattributed.
  static constexpr uint32_t kMaxScopesPerFrame = 16;
  //! Timestamp ring size; reading the oldest slot hides submission latency.
  static constexpr uint32_t kRingFrames = 4;

  SoVulkanGpuTimers() = default;
  ~SoVulkanGpuTimers();
  SoVulkanGpuTimers(const SoVulkanGpuTimers &) = delete;
  SoVulkanGpuTimers & operator=(const SoVulkanGpuTimers &) = delete;

  //! Create the query pool; false (stays disabled) if timestamps unsupported.
  bool initialize(VkDevice device, VkPhysicalDevice physicalDevice,
                  uint32_t queueFamilyIndex);
  bool initialized() const { return this->queryPool != VK_NULL_HANDLE; }

  //! Open/close a named scope on \a commandBuffer.  Flat, non-nested.
  //!
  //! vkCmdWriteTimestamp needs the scope's queries reset for this frame.  The
  //! own-queue path resets on the first beginScope() (before vkCmdBeginRenderPass);
  //! the caller-owned path cannot reset in-pass, so it calls resetSlot() first.
  void beginScope(VkCommandBuffer commandBuffer, const char * name);
  void endScope(VkCommandBuffer commandBuffer);

  //! Reset this frame's query range on \a commandBuffer; must be outside a pass.
  void resetSlot(VkCommandBuffer commandBuffer);
  //! True once resetSlot() ran this frame: the external path records scopes only then.
  bool slotReset() const { return this->slotResetForFrame; }

  //! Advance the ring and read back the oldest completed frame.
  void endFrame();

  //! Destroy the query pool (before the VkDevice; backend calls at shutdown).
  void shutdown();

private:
  VkDevice device = VK_NULL_HANDLE;
  VkQueryPool queryPool = VK_NULL_HANDLE;
  float timestampPeriod = 0.0f;
  uint32_t timestampValidBits = 0;

  uint32_t ringIndex = 0;
  uint32_t scopeCount = 0;
  //! A beginScope() without its endScope() yet, so an unmatched begin cannot leak.
  bool scopePending = false;
  //! This frame's query range was reset; no later begin() resets it again.
  bool slotResetForFrame = false;
  uint32_t slotScopeCount[kRingFrames] = {};
  const char * scopeNames[kRingFrames][kMaxScopesPerFrame] = {};
};

#endif // COIN_SOVULKANGPUTIMERS_H
