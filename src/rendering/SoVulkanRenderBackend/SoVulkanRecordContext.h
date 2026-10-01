// src/rendering/SoVulkanRenderBackend/SoVulkanRecordContext.h
//
// Per-recording command-buffer target plus the dedup dynamic-state/descriptor
// cache used while recording it.
//
// Recording remembers the state bound to skip redundant vkCmd* calls (see
// applyPipeline/applyViewportState/applyScissorState).  Backend-member caches
// limited recording to one thread; this context lets each (future) worker record
// with its own cache.  Descriptors/pipelines/rings stay backend-owned, read-only.

#ifndef COIN_SOVULKANRECORDCONTEXT_H
#define COIN_SOVULKANRECORDCONTEXT_H

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

struct VulkanRecordContext {
  // Command buffer being recorded: render() = backend's, renderExternal() = caller's.
  VkCommandBuffer buffer = VK_NULL_HANDLE;

  // Per-recording lighting/instance-model slot cursor, per worker so concurrent
  // recorders advance their own into disjoint (race-free) ring regions; planted at
  // an item's pre-assigned slotBase (M1b) before each item.
  uint32_t uboCmdIndex = 0;

  // Last dynamic state bound into `buffer` (see applyPipeline/applyViewportState/
  // applyScissorState); reset per frame, not per in-flight slot, so a reused slot's
  // prior content never suppresses a needed change.
  VkPipeline lastBoundPipeline = VK_NULL_HANDLE;
  VkViewport lastBoundViewport {};
  VkRect2D lastBoundScissor {};
  bool hasBoundViewport = false;
  bool hasBoundScissor = false;
  // Per-frame descriptor-bind caches: a frame usually shares one lighting handle
  // and one texture, avoiding an unordered_map lookup per draw; set 1 must still
  // re-bind every draw (its dynamic offset advances).
  uint32_t lastLightingHandle = UINT32_MAX;
  uint32_t lastLightingOffset = 0;
  uint32_t lastBoundLightingOffset = UINT32_MAX;
  VkDescriptorSet lastBoundTextureSet = VK_NULL_HANDLE;

  // Forget bound state (frame boundary); leaves `buffer` alone (set after reset).
  void reset()
  {
    uboCmdIndex = 0;
    lastBoundPipeline = VK_NULL_HANDLE;
    hasBoundViewport = false;
    hasBoundScissor = false;
    lastLightingHandle = UINT32_MAX;
    lastLightingOffset = 0;
    lastBoundLightingOffset = UINT32_MAX;
    lastBoundTextureSet = VK_NULL_HANDLE;
  }
};

#endif // COIN_SOVULKANRECORDCONTEXT_H
