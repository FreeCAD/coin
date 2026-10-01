// src/rendering/SoVulkanDebug.h

#ifndef COIN_SOVULKANDEBUG_H
#define COIN_SOVULKANDEBUG_H

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

#include <cstdarg>
#include <cstdio>

#include <Inventor/errors/SoDebugError.h>

namespace SoVulkanDebug {

//! Route a renderer diagnostic trace through Coin's message machinery instead
//! of a raw stderr write, so the embedding application's error handler sees it.
//! A trailing newline is dropped (the handler terminates the line itself).
inline void
post(const char * fmt, ...)
{
  char buffer[2048];
  va_list args;
  va_start(args, fmt);
  const int n = std::vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  if (n < 0) {
    return;
  }
  int len = (n < static_cast<int>(sizeof(buffer)))
              ? n : static_cast<int>(sizeof(buffer)) - 1;
  while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
    --len;
  }
  buffer[len] = '\0';
  SoDebugError::postInfo("SoVulkan", "%s", buffer);
}

} // namespace SoVulkanDebug

#endif // COIN_SOVULKANDEBUG_H
