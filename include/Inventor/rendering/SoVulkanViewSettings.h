// include/Inventor/rendering/SoVulkanViewSettings.h

#ifndef COIN_SOVULKANVIEWSETTINGS_H
#define COIN_SOVULKANVIEWSETTINGS_H

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

/*!
  \file SoVulkanViewSettings.h
  \brief The Vulkan viewport's display/tuning settings as one value type.

  Before this type the same settings were plumbed as a dozen independent
  setters through every layer (application -> widget -> renderer -> render
  manager), each layer keeping its own "last applied" mirror.  Passing one
  struct lets the manager diff once and lets callers hand the whole display
  state over in a single call.

  Structural per-frame state (scene graph, camera, viewport region, render
  target) is deliberately NOT part of this struct; it is passed through its own
  calls.
*/

#include <Inventor/SbColor4f.h>

struct SoVulkanViewSettings {
  //! Viewport background (solid or gradient).
  SbColor4f backgroundColor {0.0f, 0.0f, 0.0f, 1.0f};
  bool backgroundGradient = false;
  SbColor4f backgroundTop {0.0f, 0.0f, 0.0f, 1.0f};
  SbColor4f backgroundBottom {0.0f, 0.0f, 0.0f, 1.0f};

  //! Raster overlays.
  bool wireframeOverlay = false;
  bool pointsOverlay = false;
  //! Re-draw triangle commands as polygon-LINES over the shaded geometry.
  bool tessellationOverlay = false;
  SbColor4f edgeColor {0.05f, 0.05f, 0.05f, 1.0f};

  bool operator==(const SoVulkanViewSettings & other) const
  {
    return backgroundColor == other.backgroundColor
      && backgroundGradient == other.backgroundGradient
      && backgroundTop == other.backgroundTop
      && backgroundBottom == other.backgroundBottom
      && wireframeOverlay == other.wireframeOverlay
      && pointsOverlay == other.pointsOverlay
      && tessellationOverlay == other.tessellationOverlay
      && edgeColor == other.edgeColor;
  }
  bool operator!=(const SoVulkanViewSettings & other) const
  {
    return !(*this == other);
  }
};

#endif // COIN_SOVULKANVIEWSETTINGS_H
