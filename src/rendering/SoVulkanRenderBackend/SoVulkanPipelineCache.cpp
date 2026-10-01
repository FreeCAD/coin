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

// src/rendering/SoVulkanRenderBackend/SoVulkanPipelineCache.cpp
//
// See the header for the design.

#include "rendering/SoVulkanRenderBackend/SoVulkanPipelineCache.h"

#include "rendering/SoVulkanDebugUtils.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

// File header: [magic][shaderKey][blobSize] + driver blob.  magic distinguishes
// our wrapper from a bare blob; shaderKey rejects a blob built from older shaders.
constexpr uint64_t kPipelineCacheMagic = 0x434f494e50495045ull; // "COINPIPE"
constexpr size_t kPipelineCacheHeaderSize = sizeof(uint64_t) * 3;

} // namespace

void
SoVulkanPipelineCache::emit(const char * message) const
{
  if (this->logger) {
    this->logger(message);
  }
}

bool
SoVulkanPipelineCache::initialize()
{
  // Pipelines are created lazily on the draw path; a persistent cache lets the
  // driver keep compiled blobs between creations, avoiding first-frame stutter.
  // The blob carries the device's pipelineCacheUUID, so a foreign-device file is
  // rejected (retry makes an empty cache rather than failing init).
  std::vector<uint8_t> initialData;
  const bool haveInitialData = this->readFile(initialData);
  VkPipelineCacheCreateInfo ci {};
  ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  ci.initialDataSize = haveInitialData ? initialData.size() : 0;
  ci.pInitialData = haveInitialData ? initialData.data() : nullptr;
  VkResult result = vkCreatePipelineCache(this->device, &ci, this->allocator,
                                          &this->cache);
  if (result != VK_SUCCESS && haveInitialData) {
    // Unreadable as a cache (corrupt or foreign device); start empty, don't fail.
    char msg[192];
    std::snprintf(msg, sizeof(msg),
                  "pipeline cache: rejected %s; starting with an empty cache",
                  this->path.c_str());
    this->emit(msg);
    ci.initialDataSize = 0;
    ci.pInitialData = nullptr;
    result = vkCreatePipelineCache(this->device, &ci, this->allocator,
                                   &this->cache);
  }
  else if (result == VK_SUCCESS && haveInitialData) {
    // "supplied" not "loaded": the impl may ignore unusable data (stale
    // pipelineCacheUUID), so acceptance does not guarantee reuse.
    char msg[192];
    std::snprintf(msg, sizeof(msg), "pipeline cache: supplied %zu bytes from %s",
                  initialData.size(), this->path.c_str());
    this->emit(msg);
  }
  if (result == VK_SUCCESS) {
    SoVulkanDebugUtils::nameObject(this->device, VK_OBJECT_TYPE_PIPELINE_CACHE,
                                   reinterpret_cast<uint64_t>(this->cache),
                                   "Coin raster pipeline cache");
  }
  return result == VK_SUCCESS;
}

void
SoVulkanPipelineCache::shutdown()
{
  for (auto & entry : this->pipelines) {
    if (entry.second != VK_NULL_HANDLE) {
      vkDestroyPipeline(this->device, entry.second, this->allocator);
    }
  }
  this->pipelines.clear();
  for (auto & entry : this->backgroundPipelines) {
    if (entry.second != VK_NULL_HANDLE) {
      vkDestroyPipeline(this->device, entry.second, this->allocator);
    }
  }
  this->backgroundPipelines.clear();
  if (this->cache != VK_NULL_HANDLE) {
    // Persist before the handle dies so lazily-built variants survive a restart.
    this->writeFile();
    vkDestroyPipelineCache(this->device, this->cache, this->allocator);
    this->cache = VK_NULL_HANDLE;
  }
}

bool
SoVulkanPipelineCache::readFile(std::vector<uint8_t> & data) const
{
  if (this->path.empty()) return false;
  std::ifstream in(this->path, std::ios::binary | std::ios::ate);
  if (!in) return false;
  const std::streamoff size = in.tellg();
  if (size <= 0) return false;
  // Bound the read: never slurp a corrupt/foreign file wholesale at device init.
  if (size > static_cast<std::streamoff>(64u * 1024u * 1024u)) return false;
  std::vector<uint8_t> raw(static_cast<size_t>(size));
  in.seekg(0, std::ios::beg);
  in.read(reinterpret_cast<char *>(raw.data()),
          static_cast<std::streamsize>(raw.size()));
  if (!(in.good() || in.eof())) return false;

  // Validate the wrapper; mismatch (different shaders, bare blob, truncation)
  // means unusable, so return false and let initialize() start empty.
  if (raw.size() < kPipelineCacheHeaderSize) return false;
  uint64_t magic = 0;
  uint64_t key = 0;
  uint64_t blobSize = 0;
  std::memcpy(&magic, raw.data(), sizeof(magic));
  std::memcpy(&key, raw.data() + sizeof(uint64_t), sizeof(key));
  std::memcpy(&blobSize, raw.data() + sizeof(uint64_t) * 2, sizeof(blobSize));
  if (magic != kPipelineCacheMagic || key != this->shaderKey ||
      blobSize != raw.size() - kPipelineCacheHeaderSize) {
    return false;
  }
  data.assign(raw.begin() + kPipelineCacheHeaderSize, raw.end());
  return true;
}

void
SoVulkanPipelineCache::writeFile() const
{
  if (this->path.empty() || this->cache == VK_NULL_HANDLE) {
    return;
  }
  size_t size = 0;
  if (vkGetPipelineCacheData(this->device, this->cache, &size, nullptr) !=
        VK_SUCCESS || size == 0) {
    return;
  }
  std::vector<uint8_t> blob(size);
  if (vkGetPipelineCacheData(this->device, this->cache, &size, blob.data()) !=
      VK_SUCCESS) {
    return;
  }
  blob.resize(size);

  // Wrap the driver blob with the header readFile() validates.
  std::vector<uint8_t> data;
  data.reserve(kPipelineCacheHeaderSize + blob.size());
  const uint64_t magic = kPipelineCacheMagic;
  const uint64_t key = this->shaderKey;
  const uint64_t blobSize = blob.size();
  const auto append = [&data](const void * p, const size_t n) {
    const auto * bytes = static_cast<const uint8_t *>(p);
    data.insert(data.end(), bytes, bytes + n);
  };
  append(&magic, sizeof(magic));
  append(&key, sizeof(key));
  append(&blobSize, sizeof(blobSize));
  append(blob.data(), blob.size());

  // Write a sibling temp then swap in.  std::rename does not replace on Windows,
  // so remove the target first; losing an advisory cache to a crash is harmless.
  const std::string tmpPath = this->path + ".tmp";
  {
    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(reinterpret_cast<const char *>(data.data()),
              static_cast<std::streamsize>(data.size()));
    if (!out) return;
  }
  std::remove(this->path.c_str());
  std::rename(tmpPath.c_str(), this->path.c_str());
  char msg[192];
  std::snprintf(msg, sizeof(msg), "pipeline cache: saved %zu bytes to %s",
                data.size(), this->path.c_str());
  this->emit(msg);
}
