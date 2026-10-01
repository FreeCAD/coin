#ifndef COIN_SORENDERIRP_H
#define COIN_SORENDERIRP_H

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

#include <Inventor/actions/SoIRRenderAction.h>
#include <Inventor/rendering/SoRenderIR.h>

#include <cstddef>
#include <memory>

class SoState;

//! Chunk-based CPU scratch allocator for per-frame geometry; pointers stay valid until clear(), and growth adds chunks without moving old data.
class SoIRBuffer {
public:
  SoIRBuffer();
  ~SoIRBuffer() = default;

  void clear();
  void reserve(size_t bytes);
  void * allocate(size_t bytes, size_t alignment = alignof(float));

  template <typename T>
  T * allocateArray(size_t count, size_t alignment = alignof(T)) {
    return static_cast<T *>(this->allocate(count * sizeof(T), alignment));
  }

  size_t size() const { return this->totalAllocated; }

private:
  static constexpr size_t MIN_CHUNK_SIZE = 1024 * 1024; // 1 MB
  struct Chunk {
    std::vector<uint8_t> data;
    size_t cursor = 0;
  };
  std::vector<std::unique_ptr<Chunk>> chunks;
  size_t totalAllocated = 0;
  size_t highWaterMark = 0;  // largest total allocation seen across frames
};

//! Compute the coarse/fine sort key used by SoDrawList::buildSortedOrder().
uint64_t SoIRComputeSortKey(uint32_t passOrderBits,
                            uint32_t depthBucket);

/*! \namespace SoRenderIR \brief Helpers converting Coin state and caches into render IR. */
namespace SoRenderIR {
//! Fill a material snapshot from the current Inventor traversal state.
void fillMaterialFromState(SoState * state, SoMaterialData & material,
                           int materialIndex = 0);
//! Copy the current texture image into action-owned frame storage.
void fillTextureFromState(SoState * state, SoIRRenderAction * action,
                          SoMaterialData & material);
void fillRenderStateFromState(SoState * state, SoRenderState & renderState);
//! Complete blend state after material opacity has been captured.
void ensureMaterialBlendState(SoRenderState & renderState,
                              const SoMaterialData & material);
//! Extract the current lighting setup, append/deduplicate it, and return its handle.
SoLightingHandle fillLightingFromState(SoState * state, SoDrawList & drawlist);
bool isMaterialTransparent(const SoMaterialData & material);
}

#endif // COIN_SORENDERIRP_H
