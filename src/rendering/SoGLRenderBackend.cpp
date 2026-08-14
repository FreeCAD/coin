// src/rendering/SoGLRenderBackend.cpp

#include "rendering/SoGLRenderBackend.h"

#include <Inventor/errors/SoDebugError.h>

#include "glue/glp.h"
#include "glue/glslp.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <data/shaders/gl/visual/Fragment.h>
#include <data/shaders/gl/visual/Vertex.h>

namespace {

static constexpr int MAX_VERTEX_COUNT = 10000000;

GLenum
topologyToGL(const SoPrimitiveTopology topology)
{
  switch (topology) {
  case SO_TOPOLOGY_POINTS: return GL_POINTS;
  case SO_TOPOLOGY_LINES: return GL_LINES;
  case SO_TOPOLOGY_TRIANGLES: return GL_TRIANGLES;
  case SO_TOPOLOGY_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
  case SO_TOPOLOGY_LINE_STRIP: return GL_LINE_STRIP;
  default: return GL_TRIANGLES;
  }
}

void
applyViewport(const SoRenderParams & params)
{
  const SbVec2s & origin = params.viewport.getViewportOriginPixels();
  const SbVec2s & size = params.viewport.getViewportSizePixels();
  glViewport(origin[0], origin[1], size[0], size[1]);
}

void
logShaderSourceMap(const char * source)
{
  const std::string marker = "// coin-source-id: ";
  const std::string sourceText = source ? source : "";
  std::string::size_type position = 0;
  while ((position = sourceText.find(marker, position)) != std::string::npos) {
    const std::string::size_type end = sourceText.find('\n', position);
    const std::string mapping = sourceText.substr(
      position + marker.length(),
      end == std::string::npos ? std::string::npos : end - position - marker.length());
    SoDebugError::postInfo("SoGLRenderBackend::compileShader",
                           "source ID map: %s", mapping.c_str());
    position = end == std::string::npos ? sourceText.length() : end + 1;
  }
}

GLuint
compileShader(const cc_glglue * glue, const GLenum type, const char * source)
{
  GLuint shader = cc_glglue_glCreateShader(glue, type);
  cc_glglue_glShaderSource(glue, shader, 1, &source, nullptr);
  cc_glglue_glCompileShader(glue, shader);

  GLint status = GL_FALSE;
  cc_glglue_glGetShaderiv(glue, shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    GLint length = 0;
    cc_glglue_glGetShaderiv(glue, shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
      std::string log(static_cast<size_t>(length), '\0');
      cc_glglue_glGetShaderInfoLog(glue, shader, length, &length, &log[0]);
      SoDebugError::postInfo("SoGLRenderBackend::compileShader",
                             "%s", log.c_str());
    }
    logShaderSourceMap(source);
    cc_glglue_glDeleteShader(glue, shader);
    return 0;
  }
  return shader;
}

bool
textureDescriptionMatches(const CachedGPUCommand & entry,
                           const SoRenderCommand & command)
{
  const SoTextureData & texture = command.material.texture;
  return entry.texturePixelsKey == texture.pixels &&
    entry.textureWidth == texture.width &&
    entry.textureHeight == texture.height &&
    entry.textureComponents == texture.numComponents;
}

} // namespace

SoGLRenderBackend::SoGLRenderBackend()
{
}

SoGLRenderBackend::~SoGLRenderBackend()
{
  if (this->isInitialized()) this->shutdown();
}

const char *
SoGLRenderBackend::getName() const
{
  return "GLRenderBackend";
}

SbBool
SoGLRenderBackend::initialize(const SoRenderBackendInitParams & params)
{
  if (this->isInitialized()) return TRUE;

  this->setInitParams(params);
  void * context = coin_gl_current_context();
  this->glue = context ? cc_glglue_instance_from_context_ptr(context) : nullptr;
  if (!this->glue || !this->glue->glGenVertexArrays ||
      !this->glue->glBindVertexArray ||
      !this->glue->glDeleteVertexArrays ||
      !this->glue->glGetAttribLocation ||
      !this->glue->glVertexAttribPointer ||
      !this->glue->glEnableVertexAttribArray ||
      !this->glue->glDisableVertexAttribArray ||
      !this->glue->glVertexAttrib4f ||
      !this->glue->glVertexAttrib2f ||
      !this->glue->glUniform1f || !this->glue->glUniform1i ||
      !this->glue->glUniform4f || !this->glue->glUniformMatrix4fv) {
    this->emitError("active context does not provide retained-renderer GL dispatch");
    this->glue = nullptr;
    return FALSE;
  }

  if (!this->createShaders()) {
    this->emitError("failed to create retained core-profile shaders");
    this->glue = nullptr;
    return FALSE;
  }

  this->posLoc = this->glue->glGetAttribLocation(this->shaderProgram,
                                                  "a_position");
  this->colorLoc = this->glue->glGetAttribLocation(this->shaderProgram,
                                                    "a_color");
  this->texcoordLoc = this->glue->glGetAttribLocation(this->shaderProgram,
                                                       "a_texcoord");
  this->setInitialized(TRUE);
  return TRUE;
}

void
SoGLRenderBackend::destroyCacheEntry(CachedGPUCommand & entry)
{
  if (entry.posVBO) cc_glglue_glDeleteBuffers(this->glue, 1, &entry.posVBO);
  if (entry.colorVBO) cc_glglue_glDeleteBuffers(this->glue, 1, &entry.colorVBO);
  if (entry.texcoordVBO) {
    cc_glglue_glDeleteBuffers(this->glue, 1, &entry.texcoordVBO);
  }
  if (entry.textureId) {
    cc_glglue_glDeleteTextures(this->glue, 1, &entry.textureId);
  }
  if (entry.idxVBO) cc_glglue_glDeleteBuffers(this->glue, 1, &entry.idxVBO);
  if (entry.vao) this->glue->glDeleteVertexArrays(1, &entry.vao);
  entry = CachedGPUCommand();
}

void
SoGLRenderBackend::invalidateCache()
{
  if (this->glue) {
    for (CachedGPUCommand & entry : this->gpuCache) {
      this->destroyCacheEntry(entry);
    }
  }
  this->gpuCache.clear();
  this->commandToCache.clear();
  this->cachedCommandCount = 0;
  this->haveCacheGeneration = false;
}

void
SoGLRenderBackend::shutdown()
{
  if (!this->isInitialized()) return;

  this->invalidateCache();
  if (this->shaderProgram) {
    cc_glglue_glDeleteProgram(this->glue, this->shaderProgram);
    this->shaderProgram = 0;
  }
  this->glue = nullptr;
  this->setInitialized(FALSE);
  this->emitLog("shutdown");
}

CachedGPUCommand &
SoGLRenderBackend::getOrCreateCache(const SoRenderCommand * command)
{
  const auto found = this->commandToCache.find(command);
  if (found != this->commandToCache.end()) {
    return this->gpuCache[found->second];
  }

  const size_t index = this->gpuCache.size();
  this->gpuCache.emplace_back();
  this->commandToCache[command] = index;
  return this->gpuCache.back();
}

void
SoGLRenderBackend::uploadGeometry(CachedGPUCommand & entry,
                                  const SoRenderCommand & command)
{
  const SoGeometryDesc & geometry = command.geometry;
  const GLsizei vertexStride = static_cast<GLsizei>(
    geometry.vertexStride ? geometry.vertexStride : sizeof(float) * 3);

  if (!entry.posVBO) cc_glglue_glGenBuffers(this->glue, 1, &entry.posVBO);
  cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.posVBO);
  cc_glglue_glBufferData(this->glue, GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(geometry.vertexCount) *
                         vertexStride,
                         geometry.positions, GL_STATIC_DRAW);

  if (geometry.colors && geometry.vertexCount) {
    if (!entry.colorVBO) cc_glglue_glGenBuffers(this->glue, 1, &entry.colorVBO);
    cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.colorVBO);
    cc_glglue_glBufferData(this->glue, GL_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(geometry.vertexCount) *
                           sizeof(float) * 4,
                           geometry.colors, GL_STATIC_DRAW);
  }
  else if (entry.colorVBO) {
    cc_glglue_glDeleteBuffers(this->glue, 1, &entry.colorVBO);
    entry.colorVBO = 0;
  }

  const SoTextureData & texture = command.material.texture;
  const bool hasTexture = texture.pixels && texture.width > 0 &&
    texture.height > 0 && (texture.numComponents >= 1 &&
                           texture.numComponents <= 4) &&
    geometry.texcoords && geometry.vertexCount;

  if (hasTexture) {
    if (!entry.texcoordVBO) {
      cc_glglue_glGenBuffers(this->glue, 1, &entry.texcoordVBO);
    }
    cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.texcoordVBO);
    const uint32_t sourceStride = geometry.texcoordStride
      ? geometry.texcoordStride : sizeof(float) * 4;
    std::vector<float> texcoords(static_cast<size_t>(geometry.vertexCount) * 2);
    const char * raw = reinterpret_cast<const char *>(geometry.texcoords);
    for (uint32_t i = 0; i < geometry.vertexCount; ++i) {
      const float * source = reinterpret_cast<const float *>(
        raw + static_cast<size_t>(i) * sourceStride);
      texcoords[static_cast<size_t>(i) * 2] = source[0];
      texcoords[static_cast<size_t>(i) * 2 + 1] = source[1];
    }
    cc_glglue_glBufferData(this->glue, GL_ARRAY_BUFFER,
                           texcoords.size() * sizeof(float),
                           texcoords.data(), GL_STATIC_DRAW);

    if (!entry.textureId) {
      cc_glglue_glGenTextures(this->glue, 1, &entry.textureId);
    }
    cc_glglue_glBindTexture(this->glue, GL_TEXTURE_2D, entry.textureId);

    // Core profiles do not accept the legacy L/LA upload formats. Expand all
    // retained texture data to RGBA at the backend boundary.
    std::vector<unsigned char> rgba(static_cast<size_t>(texture.width) *
                                    static_cast<size_t>(texture.height) * 4);
    const unsigned char * source = texture.pixels;
    const int components = texture.numComponents;
    const size_t pixels = static_cast<size_t>(texture.width) *
      static_cast<size_t>(texture.height);
    for (size_t i = 0; i < pixels; ++i) {
      const unsigned char luminance = source[i * components];
      rgba[i * 4] = components == 3 || components == 4
        ? source[i * components] : luminance;
      rgba[i * 4 + 1] = components == 3 || components == 4
        ? source[i * components + 1] : luminance;
      rgba[i * 4 + 2] = components == 3 || components == 4
        ? source[i * components + 2] : luminance;
      rgba[i * 4 + 3] = components == 2 ? source[i * components + 1]
        : (components == 4 ? source[i * components + 3] : 255);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    cc_glglue_glBindTexture(this->glue, GL_TEXTURE_2D, 0);
  }
  else {
    if (entry.texcoordVBO) {
      cc_glglue_glDeleteBuffers(this->glue, 1, &entry.texcoordVBO);
      entry.texcoordVBO = 0;
    }
    if (entry.textureId) {
      cc_glglue_glDeleteTextures(this->glue, 1, &entry.textureId);
      entry.textureId = 0;
    }
  }

  if (geometry.indexCount && geometry.indices) {
    if (!entry.idxVBO) cc_glglue_glGenBuffers(this->glue, 1, &entry.idxVBO);
    cc_glglue_glBindBuffer(this->glue, GL_ELEMENT_ARRAY_BUFFER, entry.idxVBO);
    cc_glglue_glBufferData(this->glue, GL_ELEMENT_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(geometry.indexCount) *
                           sizeof(uint32_t),
                           geometry.indices, GL_STATIC_DRAW);
  }
  else if (entry.idxVBO) {
    cc_glglue_glDeleteBuffers(this->glue, 1, &entry.idxVBO);
    entry.idxVBO = 0;
  }

  cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, 0);
  cc_glglue_glBindBuffer(this->glue, GL_ELEMENT_ARRAY_BUFFER, 0);

  entry.posKey = geometry.positions;
  entry.colorKey = geometry.colors;
  entry.texcoordKey = geometry.texcoords;
  entry.texturePixelsKey = hasTexture ? texture.pixels : nullptr;
  entry.idxKey = geometry.indices;
  entry.vertexCount = geometry.vertexCount;
  entry.indexCount = geometry.indexCount;
  entry.vertexStride = static_cast<uint32_t>(vertexStride);
  entry.texcoordStride = geometry.texcoordStride;
  entry.textureWidth = hasTexture ? texture.width : 0;
  entry.textureHeight = hasTexture ? texture.height : 0;
  entry.textureComponents = hasTexture ? texture.numComponents : 0;
}

void
SoGLRenderBackend::setupVisualVAO(CachedGPUCommand & entry)
{
  if (!entry.vao) this->glue->glGenVertexArrays(1, &entry.vao);
  this->glue->glBindVertexArray(entry.vao);

  if (this->posLoc >= 0 && entry.posVBO) {
    cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.posVBO);
    cc_glglue_glEnableVertexAttribArray(this->glue, this->posLoc);
    cc_glglue_glVertexAttribPointer(this->glue, this->posLoc, 3, GL_FLOAT,
                                    GL_FALSE, entry.vertexStride, nullptr);
  }
  if (this->colorLoc >= 0) {
    if (entry.colorVBO) {
      cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.colorVBO);
      cc_glglue_glEnableVertexAttribArray(this->glue, this->colorLoc);
      cc_glglue_glVertexAttribPointer(this->glue, this->colorLoc, 4, GL_FLOAT,
                                      GL_FALSE, 0, nullptr);
    }
    else {
      cc_glglue_glDisableVertexAttribArray(this->glue, this->colorLoc);
      cc_glglue_glVertexAttrib4f(this->glue, this->colorLoc,
                                 1.0f, 1.0f, 1.0f, 1.0f);
    }
  }
  if (this->texcoordLoc >= 0) {
    if (entry.texcoordVBO) {
      cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.texcoordVBO);
      cc_glglue_glEnableVertexAttribArray(this->glue, this->texcoordLoc);
      cc_glglue_glVertexAttribPointer(this->glue, this->texcoordLoc, 2,
                                      GL_FLOAT, GL_FALSE, 0, nullptr);
    }
    else {
      cc_glglue_glDisableVertexAttribArray(this->glue, this->texcoordLoc);
      cc_glglue_glVertexAttrib2f(this->glue, this->texcoordLoc, 0.0f, 0.0f);
    }
  }
  if (entry.idxVBO) {
    cc_glglue_glBindBuffer(this->glue, GL_ELEMENT_ARRAY_BUFFER, entry.idxVBO);
  }
  this->glue->glBindVertexArray(0);
  cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, 0);
  cc_glglue_glBindBuffer(this->glue, GL_ELEMENT_ARRAY_BUFFER, 0);
}

void
SoGLRenderBackend::updateGeometryCache(const SoDrawList & drawlist)
{
  const uint32_t generation = drawlist.getGeneration();
  if ((this->haveCacheGeneration && this->cacheGeneration != generation) ||
      (this->haveCacheGeneration &&
       this->cachedCommandCount != static_cast<size_t>(drawlist.getNumCommands()))) {
    this->invalidateCache();
  }
  this->cacheGeneration = generation;
  this->haveCacheGeneration = true;
  this->cachedCommandCount = static_cast<size_t>(drawlist.getNumCommands());

  for (int i = 0; i < drawlist.getNumCommands(); ++i) {
    const SoRenderCommand & command = drawlist.getCommand(i);
    const SoGeometryDesc & geometry = command.geometry;
    if (!geometry.positions || geometry.vertexCount == 0 ||
        geometry.vertexCount > MAX_VERTEX_COUNT) continue;

    CachedGPUCommand & entry = this->getOrCreateCache(&command);
    const uint32_t vertexStride = geometry.vertexStride
      ? geometry.vertexStride : sizeof(float) * 3;
    const bool geometryMatches = entry.posVBO != 0 &&
      entry.cacheGeneration == generation &&
      entry.posKey == geometry.positions &&
      entry.colorKey == geometry.colors &&
      entry.texcoordKey == geometry.texcoords &&
      entry.idxKey == geometry.indices &&
      entry.vertexCount == geometry.vertexCount &&
      entry.indexCount == geometry.indexCount &&
      entry.vertexStride == vertexStride &&
      entry.texcoordStride == geometry.texcoordStride &&
      textureDescriptionMatches(entry, command) &&
      ((entry.texturePixelsKey != nullptr) ==
       (command.material.texture.pixels != nullptr));
    if (!geometryMatches) {
      this->uploadGeometry(entry, command);
      this->setupVisualVAO(entry);
      entry.cacheGeneration = generation;
    }
  }
}

void
SoGLRenderBackend::drawCommand(const SoRenderCommand & command,
                                const SbMat & viewMat,
                                const SbMat & projMat,
                                const SoRenderParams & params)
{
  if (!command.geometry.positions || command.geometry.vertexCount == 0) return;
  const auto found = this->commandToCache.find(&command);
  if (found == this->commandToCache.end()) return;
  const CachedGPUCommand & entry = this->gpuCache[found->second];
  if (!entry.vao) return;

  applyViewport(params);
  this->glue->glUniformMatrix4fv(this->uViewLocation, 1, GL_FALSE,
                                 &viewMat[0][0]);
  this->glue->glUniformMatrix4fv(this->uProjLocation, 1, GL_FALSE,
                                 &projMat[0][0]);
  SbMat model;
  command.modelMatrix.getValue(model);
  this->glue->glUniformMatrix4fv(this->uModelLocation, 1, GL_FALSE,
                                 &model[0][0]);

  const SbVec4f & color = command.material.diffuse;
  this->glue->glUniform4f(this->uColorLocation,
                          color[0], color[1], color[2], color[3]);
  this->glue->glUniform1f(this->uUseVertexColorLocation,
                          entry.colorVBO ? 1.0f : 0.0f);

  const bool textured = entry.textureId != 0 && entry.texcoordVBO != 0;
  this->glue->glUniform1f(this->uTextureEnabledLocation,
                          textured ? 1.0f : 0.0f);
  if (textured) {
    cc_glglue_glActiveTexture(this->glue, GL_TEXTURE0);
    cc_glglue_glBindTexture(this->glue, GL_TEXTURE_2D, entry.textureId);
    this->glue->glUniform1i(this->uTextureLocation, 0);
    this->glue->glUniform4f(this->uTexModColorLocation,
                            color[0], color[1], color[2], color[3]);
  }

  const GLenum primitive = topologyToGL(command.geometry.topology);
  this->glue->glBindVertexArray(entry.vao);
  if (command.geometry.indexCount && command.geometry.indices) {
    cc_glglue_glDrawElements(this->glue, primitive,
                             static_cast<GLsizei>(command.geometry.indexCount),
                             GL_UNSIGNED_INT, nullptr);
  }
  else {
    cc_glglue_glDrawArrays(this->glue, primitive, 0,
                           static_cast<GLsizei>(command.geometry.vertexCount));
  }
  this->glue->glBindVertexArray(0);
  if (textured) cc_glglue_glBindTexture(this->glue, GL_TEXTURE_2D, 0);
}

void
SoGLRenderBackend::beginFrame(const SoRenderParams & params)
{
  // Establish a deterministic baseline. These values are not interpretations
  // of retained Coin state; semantic depth/blend/raster execution is layered
  // above this executor.
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_TRUE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glPointSize(1.0f);
  glLineWidth(1.0f);

  if (params.flags & SO_PARAM_CLEAR_WINDOW) {
    const SbColor4f & color = params.clearColor;
    glClearColor(color[0], color[1], color[2], color[3]);
  }
  GLbitfield clearMask = 0;
  if (params.flags & SO_PARAM_CLEAR_WINDOW) clearMask |= GL_COLOR_BUFFER_BIT;
  if (params.flags & SO_PARAM_CLEAR_DEPTH) {
    glClearDepth(params.clearDepth);
    clearMask |= GL_DEPTH_BUFFER_BIT;
  }
  if (clearMask) glClear(clearMask);

  applyViewport(params);
  cc_glglue_glUseProgram(this->glue, this->shaderProgram);
}

bool
SoGLRenderBackend::createShaders()
{
  const GLuint vertex = compileShader(this->glue, GL_VERTEX_SHADER,
                                      coin_gl_visual_vertex_shadersource);
  const GLuint fragment = compileShader(this->glue, GL_FRAGMENT_SHADER,
                                        coin_gl_visual_fragment_shadersource);
  if (!vertex || !fragment) {
    if (vertex) cc_glglue_glDeleteShader(this->glue, vertex);
    if (fragment) cc_glglue_glDeleteShader(this->glue, fragment);
    return false;
  }

  const GLuint program = cc_glglue_glCreateProgram(this->glue);
  cc_glglue_glAttachShader(this->glue, program, vertex);
  cc_glglue_glAttachShader(this->glue, program, fragment);
  cc_glglue_glLinkProgram(this->glue, program);
  GLint linked = GL_FALSE;
  cc_glglue_glGetGLSLProgramiv(this->glue, program, GL_LINK_STATUS, &linked);
  cc_glglue_glDeleteShader(this->glue, vertex);
  cc_glglue_glDeleteShader(this->glue, fragment);
  if (linked == GL_FALSE) {
    cc_glglue_glDeleteProgram(this->glue, program);
    return false;
  }

  this->shaderProgram = program;
  this->uViewLocation = cc_glglue_glGetUniformLocation(this->glue, program,
                                                        "u_view");
  this->uProjLocation = cc_glglue_glGetUniformLocation(this->glue, program,
                                                         "u_proj");
  this->uModelLocation = cc_glglue_glGetUniformLocation(this->glue, program,
                                                          "u_model");
  this->uColorLocation = cc_glglue_glGetUniformLocation(this->glue, program,
                                                         "u_color");
  this->uUseVertexColorLocation = cc_glglue_glGetUniformLocation(
    this->glue, program, "u_useVertexColor");
  this->uTextureLocation = cc_glglue_glGetUniformLocation(this->glue, program,
                                                           "u_texture");
  this->uTextureEnabledLocation = cc_glglue_glGetUniformLocation(
    this->glue, program, "u_textureEnabled");
  this->uTexModColorLocation = cc_glglue_glGetUniformLocation(
    this->glue, program, "u_texModColor");
  return true;
}

SbBool
SoGLRenderBackend::render(const SoDrawList & drawlist,
                          const SoRenderParams & params)
{
  if (!this->isInitialized()) {
    this->emitError("render called before backend initialization");
    return FALSE;
  }

  this->debugValidateDrawList(drawlist);
  this->beginFrame(params);
  this->updateGeometryCache(drawlist);

  SbMat view;
  SbMat projection;
  params.viewMatrix.getValue(view);
  params.projMatrix.getValue(projection);

  const std::vector<int> & order = drawlist.getSortedOrder();
  for (int i = 0; i < drawlist.getNumCommands(); ++i) {
    const int index = (i < static_cast<int>(order.size())) ? order[i] : i;
    this->drawCommand(drawlist.getCommand(index), view, projection, params);
  }
  cc_glglue_glUseProgram(this->glue, 0);
  return TRUE;
}
