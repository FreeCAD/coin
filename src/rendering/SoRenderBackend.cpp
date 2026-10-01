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

#include "rendering/SoRenderBackend.h"

#include <Inventor/errors/SoDebugError.h>

#include <cassert>

SoRenderBackend::SoRenderBackend()
  : initialized(FALSE), initparams()
{
}

SoRenderBackend::~SoRenderBackend()
{
#ifdef COIN_DEBUG
  assert(!this->initialized &&
         "SoRenderBackend destroyed without calling shutdown()");
#endif // COIN_DEBUG
}

SbBool
SoRenderBackend::isInitialized() const
{
  return this->initialized;
}

void
SoRenderBackend::setInitialized(const SbBool state)
{
  this->initialized = state;
}

void
SoRenderBackend::setInitParams(const SoRenderBackendInitParams & params)
{
  this->initparams = params;
}

const SoRenderBackendInitParams &
SoRenderBackend::getInitParams() const
{
  return this->initparams;
}

void
SoRenderBackend::emitLog(const char * message) const
{
  if (this->initparams.logCallback) {
    this->initparams.logCallback(message, this->initparams.userData);
    return;
  }
  SoDebugError::postInfo("SoRenderBackend", "%s", message);
}

void
SoRenderBackend::emitError(const char * message) const
{
  if (this->initparams.errorCallback) {
    this->initparams.errorCallback(message, this->initparams.userData);
    return;
  }
  SoDebugError::post("SoRenderBackend", "%s", message);
}

void
SoRenderBackend::debugValidateDrawList(const SoDrawList & drawlist) const
{
#ifdef COIN_DEBUG
  const int num = drawlist.getNumCommands();
  for (int i = 0; i < num; ++i) {
    const SoRenderCommand & command = drawlist.getCommand(i);
    if (command.geometry.topology >= SO_TOPOLOGY_COUNT) {
      SoDebugError::post("SoRenderBackend",
                         "Command %d has invalid topology (%d)",
                         i, static_cast<int>(command.geometry.topology));
    }
    if (command.geometry.vertexCount == 0 &&
        command.geometry.indexCount == 0) {
      SoDebugError::post("SoRenderBackend",
                         "Command %d has no vertices or indices", i);
    }
    if (command.geometry.vertexCount > 0 &&
        command.geometry.positions == nullptr) {
      SoDebugError::post("SoRenderBackend",
                         "Command %d is missing its position buffer", i);
    }
    if (command.geometry.indexCount > 0 &&
        command.geometry.indices == nullptr) {
      SoDebugError::post("SoRenderBackend",
                         "Command %d is missing its index buffer", i);
    }
  }
#else
  (void) drawlist;
#endif // COIN_DEBUG
}
