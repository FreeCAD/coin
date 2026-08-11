#ifndef COIN_GLUE_GLSLP_H
#define COIN_GLUE_GLSLP_H

/**************************************************************************\
 * Copyright (c) 2026 FreeCAD contributors                              *
 *                                                                        *
 * This file is part of Coin.                                            *
 *                                                                        *
 * Coin is free software; you can redistribute it and/or modify it under *
 * the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your      *
 * option) any later version.                                            *
\**************************************************************************/

#ifndef COIN_INTERNAL
#error this is a private header file
#endif

#include <Inventor/C/glue/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

GLuint cc_glglue_glCreateShader(const cc_glglue * glue, GLenum type);
void cc_glglue_glShaderSource(const cc_glglue * glue, GLuint shader,
                              GLsizei count, const char * const * string,
                              const GLint * length);
void cc_glglue_glCompileShader(const cc_glglue * glue, GLuint shader);
void cc_glglue_glGetShaderiv(const cc_glglue * glue, GLuint shader,
                             GLenum pname, GLint * params);
void cc_glglue_glGetShaderInfoLog(const cc_glglue * glue, GLuint shader,
                                  GLsizei maxLength, GLsizei * length,
                                  char * infoLog);
void cc_glglue_glDeleteShader(const cc_glglue * glue, GLuint shader);
void cc_glglue_glAttachShader(const cc_glglue * glue, GLuint program,
                              GLuint shader);
void cc_glglue_glDetachShader(const cc_glglue * glue, GLuint program,
                              GLuint shader);
GLint cc_glglue_glGetUniformLocation(const cc_glglue * glue, GLuint program,
                                     const char * name);
void cc_glglue_glGetActiveUniform(const cc_glglue * glue, GLuint program,
                                  GLuint index, GLsizei maxLength,
                                  GLsizei * length, GLint * size,
                                  GLenum * type, char * name);
GLuint cc_glglue_glCreateProgram(const cc_glglue * glue);
void cc_glglue_glLinkProgram(const cc_glglue * glue, GLuint program);
void cc_glglue_glUseProgram(const cc_glglue * glue, GLuint program);
void cc_glglue_glDeleteProgram(const cc_glglue * glue, GLuint program);
void cc_glglue_glGetGLSLProgramiv(const cc_glglue * glue, GLuint program,
                                  GLenum pname, GLint * params);
void cc_glglue_glGetProgramInfoLog(const cc_glglue * glue, GLuint program,
                                   GLsizei maxLength, GLsizei * length,
                                   char * infoLog);
void cc_glglue_glProgramParameteriEXT(const cc_glglue * glue, GLuint program,
                                      GLenum pname, GLint value);

#ifdef __cplusplus
}
#endif

#endif /* !COIN_GLUE_GLSLP_H */
