#ifndef COIN_SOGLSLSHADERDIAGNOSTICS_H
#define COIN_SOGLSLSHADERDIAGNOSTICS_H

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

// Private helpers shared by the GLSL shader and program implementations.

#include <Inventor/SbString.h>

#include "shaders/SoGLShaderObject.h"
#include "glue/glp.h"
#include "glue/glslp.h"

#include <vector>

static inline const char *
soglsl_stage_name(const SoGLShaderObject::ShaderType type)
{
  switch (type) {
  case SoGLShaderObject::VERTEX:
    return "vertex shader";
  case SoGLShaderObject::FRAGMENT:
    return "fragment shader";
  case SoGLShaderObject::GEOMETRY:
    return "geometry shader";
  default:
    return "shader";
  }
}

static inline SbString
soglsl_get_info_log(const cc_glglue * glue,
                    const GLuint handle,
                    const SbBool program)
{
  GLint length = 0;
  if (program) {
    cc_glglue_glGetGLSLProgramiv(glue, handle, GL_INFO_LOG_LENGTH, &length);
  }
  else {
    cc_glglue_glGetShaderiv(glue, handle, GL_INFO_LOG_LENGTH, &length);
  }

  if (length <= 1) return SbString();

  std::vector<COIN_GLchar> infoLog(static_cast<size_t>(length), '\0');
  GLsizei charsWritten = 0;
  if (program) {
    cc_glglue_glGetProgramInfoLog(glue, handle, length, &charsWritten,
                                  infoLog.data());
  }
  else {
    cc_glglue_glGetShaderInfoLog(glue, handle, length, &charsWritten,
                                 infoLog.data());
  }

  if (charsWritten >= 0 && charsWritten < length) {
    infoLog[static_cast<size_t>(charsWritten)] = '\0';
  }
  else {
    infoLog.back() = '\0';
  }
  return SbString(infoLog.data());
}

#endif /* ! COIN_SOGLSLSHADERDIAGNOSTICS_H */
