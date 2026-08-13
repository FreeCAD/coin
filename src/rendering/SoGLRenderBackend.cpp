// src/rendering/SoGLRenderBackend.cpp

#include "rendering/SoGLRenderBackend.h"

#include <Inventor/C/glue/gl.h>
#include <Inventor/SbName.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/misc/SoGLDriverDatabase.h>

#include "glue/glp.h"
#include "glue/glslp.h"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <vector>

#include <data/shaders/gl/visual/Fragment.h>
#include <data/shaders/gl/visual/Vertex.h>
#include <data/shaders/gl/wide-line/Fragment.h>
#include <data/shaders/gl/wide-line/Geometry.h>
#include <data/shaders/gl/wide-line/TriangleGeometry.h>
#include <data/shaders/gl/wide-line/Vertex.h>
#include <data/shaders/gl/point/Fragment.h>
#include <data/shaders/gl/point/Geometry.h>
#include <data/shaders/gl/point/TriangleGeometry.h>
#include <data/shaders/gl/point/Vertex.h>
#include <data/shaders/gl/pixel/Fragment.h>
#include <data/shaders/gl/pixel/Vertex.h>

namespace {

static constexpr int MAX_VERTEX_COUNT = 10000000;
static constexpr int MAX_SHADER_LIGHTS = 8;

GLenum
textureWrapToGL(const SoTextureWrap wrap)
{
  switch (wrap) {
  case SO_TEXTURE_WRAP_REPEAT: return GL_REPEAT;
  case SO_TEXTURE_WRAP_CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;
  case SO_TEXTURE_WRAP_CLAMP_TO_EDGE:
  default: return GL_CLAMP_TO_EDGE;
  }
}

GLenum
textureMinFilterToGL(const SoTextureFilter filter)
{
  switch (filter) {
  case SO_TEXTURE_FILTER_LINEAR: return GL_LINEAR;
  case SO_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
    return GL_NEAREST_MIPMAP_NEAREST;
  case SO_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
    return GL_LINEAR_MIPMAP_NEAREST;
  case SO_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
    return GL_NEAREST_MIPMAP_LINEAR;
  case SO_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
    return GL_LINEAR_MIPMAP_LINEAR;
  case SO_TEXTURE_FILTER_NEAREST:
  default: return GL_NEAREST;
  }
}

GLenum
textureMagFilterToGL(const SoTextureFilter filter)
{
  return filter == SO_TEXTURE_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;
}

GLenum
blendFactorToGL(const SoBlendFactor factor)
{
  switch (factor) {
  case SO_BLEND_FACTOR_ZERO: return GL_ZERO;
  case SO_BLEND_FACTOR_ONE: return GL_ONE;
  case SO_BLEND_FACTOR_SRC_COLOR: return GL_SRC_COLOR;
  case SO_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;
  case SO_BLEND_FACTOR_DST_COLOR: return GL_DST_COLOR;
  case SO_BLEND_FACTOR_ONE_MINUS_DST_COLOR: return GL_ONE_MINUS_DST_COLOR;
  case SO_BLEND_FACTOR_SRC_ALPHA: return GL_SRC_ALPHA;
  case SO_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
  case SO_BLEND_FACTOR_DST_ALPHA: return GL_DST_ALPHA;
  case SO_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: return GL_ONE_MINUS_DST_ALPHA;
  case SO_BLEND_FACTOR_CONSTANT_COLOR: return GL_CONSTANT_COLOR;
  case SO_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
    return GL_ONE_MINUS_CONSTANT_COLOR;
  case SO_BLEND_FACTOR_CONSTANT_ALPHA: return GL_CONSTANT_ALPHA;
  case SO_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
    return GL_ONE_MINUS_CONSTANT_ALPHA;
  case SO_BLEND_FACTOR_SRC_ALPHA_SATURATE: return GL_SRC_ALPHA_SATURATE;
  // The Visual program has no secondary fragment output. Keep the semantic
  // factor in the IR and make the executor's deterministic primary-source
  // fallback only at this API boundary.
  case SO_BLEND_FACTOR_SRC1_COLOR: return GL_SRC_COLOR;
  case SO_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR: return GL_ONE_MINUS_SRC_COLOR;
  case SO_BLEND_FACTOR_SRC1_ALPHA: return GL_SRC_ALPHA;
  case SO_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;
  default: return GL_ONE;
  }
}

bool
isDualSourceBlendFactor(const SoBlendFactor factor)
{
  return factor == SO_BLEND_FACTOR_SRC1_COLOR ||
         factor == SO_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR ||
         factor == SO_BLEND_FACTOR_SRC1_ALPHA ||
         factor == SO_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
}

GLenum
blendEquationToGL(const SoBlendEquation equation)
{
  switch (equation) {
  case SO_BLEND_EQUATION_SUBTRACT: return GL_FUNC_SUBTRACT;
  case SO_BLEND_EQUATION_REVERSE_SUBTRACT: return GL_FUNC_REVERSE_SUBTRACT;
  case SO_BLEND_EQUATION_MIN: return GL_MIN;
  case SO_BLEND_EQUATION_MAX: return GL_MAX;
  case SO_BLEND_EQUATION_ADD:
  default: return GL_FUNC_ADD;
  }
}

GLenum
depthFunctionToGL(const SoDepthFunction function)
{
  switch (function) {
  case SO_DEPTH_NEVER: return GL_NEVER;
  case SO_DEPTH_ALWAYS: return GL_ALWAYS;
  case SO_DEPTH_LESS: return GL_LESS;
  case SO_DEPTH_LEQUAL: return GL_LEQUAL;
  case SO_DEPTH_EQUAL: return GL_EQUAL;
  case SO_DEPTH_GEQUAL: return GL_GEQUAL;
  case SO_DEPTH_GREATER: return GL_GREATER;
  case SO_DEPTH_NOTEQUAL: return GL_NOTEQUAL;
  default: return GL_LEQUAL;
  }
}

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

GLuint
linkProgram(const cc_glglue * glue, const char * vertexSource,
            const char * fragmentSource, const char * geometrySource = nullptr)
{
  const GLuint vertex = compileShader(glue, GL_VERTEX_SHADER, vertexSource);
  const GLuint fragment = compileShader(glue, GL_FRAGMENT_SHADER, fragmentSource);
  const GLuint geometry = geometrySource
    ? compileShader(glue, GL_GEOMETRY_SHADER, geometrySource) : 0;
  if (!vertex || !fragment || (geometrySource && !geometry)) {
    if (vertex) cc_glglue_glDeleteShader(glue, vertex);
    if (fragment) cc_glglue_glDeleteShader(glue, fragment);
    if (geometry) cc_glglue_glDeleteShader(glue, geometry);
    return 0;
  }

  const GLuint program = cc_glglue_glCreateProgram(glue);
  cc_glglue_glAttachShader(glue, program, vertex);
  cc_glglue_glAttachShader(glue, program, fragment);
  if (geometry) cc_glglue_glAttachShader(glue, program, geometry);
  cc_glglue_glLinkProgram(glue, program);
  GLint linked = GL_FALSE;
  cc_glglue_glGetGLSLProgramiv(glue, program, GL_LINK_STATUS, &linked);
  if (linked == GL_FALSE) {
    GLint length = 0;
    cc_glglue_glGetGLSLProgramiv(glue, program, GL_INFO_LOG_LENGTH, &length);
    if (length > 0) {
      std::string log(static_cast<size_t>(length), '\0');
      cc_glglue_glGetProgramInfoLog(glue, program, length, &length, &log[0]);
      SoDebugError::postInfo("SoGLRenderBackend::linkProgram", "%s",
                             log.c_str());
    }
    cc_glglue_glDeleteProgram(glue, program);
  }
  cc_glglue_glDeleteShader(glue, vertex);
  cc_glglue_glDeleteShader(glue, fragment);
  if (geometry) cc_glglue_glDeleteShader(glue, geometry);
  return linked == GL_FALSE ? 0 : program;
}

bool
textureDescriptionMatches(const CachedGPUCommand & entry,
                           const SoRenderCommand & command)
{
  const SoTextureData & texture = command.material.texture;
  return entry.texturePixelsKey == texture.pixels &&
    entry.textureWidth == texture.width &&
    entry.textureHeight == texture.height &&
    entry.textureComponents == texture.numComponents &&
    entry.textureMinFilter == texture.minFilter &&
    entry.textureMagFilter == texture.magFilter &&
    entry.textureWrapS == texture.wrapS &&
    entry.textureWrapT == texture.wrapT &&
    entry.textureMaxAnisotropy == texture.maxAnisotropy;
}

} // namespace

SoGLRenderBackend::SoGLRenderBackend()
{
}

SoGLRenderBackend::~SoGLRenderBackend()
{
  if (!this->isInitialized()) return;
  if (coin_gl_current_context() == this->context) this->shutdown();
  else this->discard();
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
  this->context = coin_gl_current_context();
  this->glue = this->context
    ? cc_glglue_instance_from_context_ptr(this->context) : nullptr;
  if (!this->glue || !this->glue->glGenVertexArrays ||
      !this->glue->glBindVertexArray ||
      !this->glue->glDeleteVertexArrays ||
      !this->glue->glGetAttribLocation ||
      !this->glue->glVertexAttribPointer ||
      !this->glue->glEnableVertexAttribArray ||
      !this->glue->glDisableVertexAttribArray ||
      !this->glue->glVertexAttrib4f ||
      !this->glue->glVertexAttrib3f ||
      !this->glue->glVertexAttrib2f ||
      !this->glue->glVertexAttrib1f ||
      !this->glue->glUniform1f || !this->glue->glUniform1i ||
      !this->glue->glUniform2f ||
      !this->glue->glUniform3f || !this->glue->glUniform1iv ||
      !this->glue->glUniform2fv || !this->glue->glUniform3fv ||
      !this->glue->glUniform4f || !this->glue->glUniformMatrix4fv ||
      !this->glue->glBlendFuncSeparate) {
    this->emitError("active context does not provide retained-renderer GL dispatch");
    this->glue = nullptr;
    this->context = nullptr;
    return FALSE;
  }

  if (!this->createShaders()) {
    this->emitError("failed to create retained core-profile shaders");
    this->glue = nullptr;
    this->context = nullptr;
    return FALSE;
  }

  GLfloat lineRange[2] = { 1.0f, 1.0f };
  GLfloat pointRange[2] = { 1.0f, 1.0f };
  glGetFloatv(GL_LINE_WIDTH_RANGE, lineRange);
  glGetFloatv(GL_POINT_SIZE_RANGE, pointRange);
  this->nativeLineWidthMax = std::max(1.0f, lineRange[1]);
  this->nativePointSizeMax = std::max(1.0f, pointRange[1]);

  this->posLoc = this->glue->glGetAttribLocation(this->shaderProgram,
                                                  "a_position");
  this->colorLoc = this->glue->glGetAttribLocation(this->shaderProgram,
                                                    "a_color");
  this->texcoordLoc = this->glue->glGetAttribLocation(this->shaderProgram,
                                                       "a_texcoord");
  this->normLoc = this->glue->glGetAttribLocation(this->shaderProgram,
                                                   "a_normal");

  this->pickBuffer.reset(new SoIDPickBuffer);
  if (!this->pickBuffer->initialize(this->glue)) {
    this->emitLog("ID pick buffer initialization failed (picking disabled)");
    this->pickBuffer.reset();
  }
  this->setInitialized(TRUE);
  return TRUE;
}

void
SoGLRenderBackend::destroyCacheEntry(CachedGPUCommand & entry)
{
  if (entry.posVBO) cc_glglue_glDeleteBuffers(this->glue, 1, &entry.posVBO);
  if (entry.normVBO) cc_glglue_glDeleteBuffers(this->glue, 1, &entry.normVBO);
  if (entry.colorVBO) cc_glglue_glDeleteBuffers(this->glue, 1, &entry.colorVBO);
  if (entry.texcoordVBO) {
    cc_glglue_glDeleteBuffers(this->glue, 1, &entry.texcoordVBO);
  }
  if (entry.lineDistVBO) {
    cc_glglue_glDeleteBuffers(this->glue, 1, &entry.lineDistVBO);
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
  if (coin_gl_current_context() != this->context) {
    this->discard();
    return;
  }
  this->pickBuffer.reset();
  this->invalidateCache();
  if (this->shaderProgram) {
    cc_glglue_glDeleteProgram(this->glue, this->shaderProgram);
    this->shaderProgram = 0;
  }
  if (this->lineShaderProgram) {
    cc_glglue_glDeleteProgram(this->glue, this->lineShaderProgram);
    this->lineShaderProgram = 0;
  }
  if (this->lineTriangleShaderProgram) {
    cc_glglue_glDeleteProgram(this->glue, this->lineTriangleShaderProgram);
    this->lineTriangleShaderProgram = 0;
  }
  if (this->pointShaderProgram) {
    cc_glglue_glDeleteProgram(this->glue, this->pointShaderProgram);
    this->pointShaderProgram = 0;
  }
  if (this->pointTriangleShaderProgram) {
    cc_glglue_glDeleteProgram(this->glue, this->pointTriangleShaderProgram);
    this->pointTriangleShaderProgram = 0;
  }
  if (this->pixelShaderProgram) {
    cc_glglue_glDeleteProgram(this->glue, this->pixelShaderProgram);
    this->pixelShaderProgram = 0;
  }
  this->glue = nullptr;
  this->context = nullptr;
  this->setInitialized(FALSE);
  this->emitLog("shutdown");
}

void
SoGLRenderBackend::discard()
{
  if (this->pickBuffer) this->pickBuffer->discard();
  this->pickBuffer.reset();
  this->gpuCache.clear();
  this->commandToCache.clear();
  this->cachedCommandCount = 0;
  this->haveCacheGeneration = false;
  this->shaderProgram = 0;
  this->lineShaderProgram = 0;
  this->pointShaderProgram = 0;
  this->pixelShaderProgram = 0;
  this->glue = nullptr;
  this->context = nullptr;
  this->setInitialized(FALSE);
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

  if (geometry.normals && geometry.normalCount >= geometry.vertexCount) {
    if (!entry.normVBO) cc_glglue_glGenBuffers(this->glue, 1, &entry.normVBO);
    cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.normVBO);
    cc_glglue_glBufferData(this->glue, GL_ARRAY_BUFFER,
                           static_cast<GLsizeiptr>(geometry.vertexCount) *
                           vertexStride,
                           geometry.normals, GL_STATIC_DRAW);
  }
  else if (entry.normVBO) {
    cc_glglue_glDeleteBuffers(this->glue, 1, &entry.normVBO);
    entry.normVBO = 0;
  }

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    textureMinFilterToGL(texture.minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    textureMagFilterToGL(texture.magFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    textureWrapToGL(texture.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    textureWrapToGL(texture.wrapT));
    const bool mipmapped =
      texture.minFilter == SO_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ||
      texture.minFilter == SO_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST ||
      texture.minFilter == SO_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
      texture.minFilter == SO_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
    if (mipmapped && this->glue->glGenerateMipmap) {
      this->glue->glGenerateMipmap(GL_TEXTURE_2D);
    }
    if (texture.maxAnisotropy > 1.0f &&
        SoGLDriverDatabase::isSupported(this->glue,
                                        SO_GL_ANISOTROPIC_FILTERING)) {
      const float supported = cc_glglue_get_max_anisotropy(this->glue);
      if (supported > 1.0f) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        std::min(texture.maxAnisotropy, supported));
      }
    }
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

  const bool lineGeometry = geometry.topology == SO_TOPOLOGY_LINES ||
    geometry.topology == SO_TOPOLOGY_LINE_STRIP;
  if (lineGeometry && geometry.vertexCount) {
    if (!entry.lineDistVBO) {
      cc_glglue_glGenBuffers(this->glue, 1, &entry.lineDistVBO);
    }
    std::vector<float> distances(geometry.vertexCount, 0.0f);
    const uint32_t strideFloats = static_cast<uint32_t>(vertexStride) /
      sizeof(float);
    const uint32_t count = geometry.indexCount && geometry.indices
      ? geometry.indexCount : geometry.vertexCount;
    if (geometry.topology == SO_TOPOLOGY_LINE_STRIP) {
      for (uint32_t i = 1; i < count; ++i) {
        const uint32_t previous = geometry.indices ? geometry.indices[i - 1] : i - 1;
        const uint32_t current = geometry.indices ? geometry.indices[i] : i;
        const float * p0 = geometry.positions + previous * strideFloats;
        const float * p1 = geometry.positions + current * strideFloats;
        const float dx = p1[0] - p0[0];
        const float dy = p1[1] - p0[1];
        const float dz = p1[2] - p0[2];
        distances[current] = distances[previous] +
          std::sqrt(dx * dx + dy * dy + dz * dz);
      }
    }
    else {
      for (uint32_t i = 0; i + 1 < count; i += 2) {
        const uint32_t first = geometry.indices ? geometry.indices[i] : i;
        const uint32_t second = geometry.indices ? geometry.indices[i + 1] : i + 1;
        const float * p0 = geometry.positions + first * strideFloats;
        const float * p1 = geometry.positions + second * strideFloats;
        const float dx = p1[0] - p0[0];
        const float dy = p1[1] - p0[1];
        const float dz = p1[2] - p0[2];
        distances[first] = 0.0f;
        distances[second] = std::sqrt(dx * dx + dy * dy + dz * dz);
      }
    }
    cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.lineDistVBO);
    cc_glglue_glBufferData(this->glue, GL_ARRAY_BUFFER,
                           distances.size() * sizeof(float), distances.data(),
                           GL_STATIC_DRAW);
    entry.lineDistKey = geometry.positions;
  }
  else if (entry.lineDistVBO) {
    cc_glglue_glDeleteBuffers(this->glue, 1, &entry.lineDistVBO);
    entry.lineDistVBO = 0;
    entry.lineDistKey = nullptr;
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
  entry.normKey = geometry.normals;
  entry.colorKey = geometry.colors;
  entry.texcoordKey = geometry.texcoords;
  entry.texturePixelsKey = hasTexture ? texture.pixels : nullptr;
  entry.idxKey = geometry.indices;
  entry.vertexCount = geometry.vertexCount;
  entry.normalCount = geometry.normalCount;
  entry.indexCount = geometry.indexCount;
  entry.vertexStride = static_cast<uint32_t>(vertexStride);
  entry.texcoordStride = geometry.texcoordStride;
  entry.textureWidth = hasTexture ? texture.width : 0;
  entry.textureHeight = hasTexture ? texture.height : 0;
  entry.textureComponents = hasTexture ? texture.numComponents : 0;
  entry.textureMinFilter = hasTexture ? texture.minFilter
                                      : SO_TEXTURE_FILTER_NEAREST;
  entry.textureMagFilter = hasTexture ? texture.magFilter
                                      : SO_TEXTURE_FILTER_NEAREST;
  entry.textureWrapS = hasTexture ? texture.wrapS
                                  : SO_TEXTURE_WRAP_CLAMP_TO_EDGE;
  entry.textureWrapT = hasTexture ? texture.wrapT
                                  : SO_TEXTURE_WRAP_CLAMP_TO_EDGE;
  entry.textureMaxAnisotropy = hasTexture ? texture.maxAnisotropy : 1.0f;
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
  if (this->normLoc >= 0) {
    if (entry.normVBO) {
      cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.normVBO);
      cc_glglue_glEnableVertexAttribArray(this->glue, this->normLoc);
      cc_glglue_glVertexAttribPointer(this->glue, this->normLoc, 3, GL_FLOAT,
                                      GL_FALSE, entry.vertexStride, nullptr);
    }
    else {
      cc_glglue_glDisableVertexAttribArray(this->glue, this->normLoc);
      this->glue->glVertexAttrib3f(this->normLoc, 0.0f, 0.0f, 1.0f);
    }
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
  if (this->lineDistLoc >= 0) {
    if (entry.lineDistVBO) {
      cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.lineDistVBO);
      cc_glglue_glEnableVertexAttribArray(this->glue, this->lineDistLoc);
      cc_glglue_glVertexAttribPointer(this->glue, this->lineDistLoc, 1,
                                      GL_FLOAT, GL_FALSE, 0, nullptr);
    }
    else {
      cc_glglue_glDisableVertexAttribArray(this->glue, this->lineDistLoc);
      this->glue->glVertexAttrib1f(this->lineDistLoc, 0.0f);
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
SoGLRenderBackend::updateLineDistances(CachedGPUCommand & entry,
                                        const SoRenderCommand & command,
                                        const SbMat & viewMat,
                                        const SbMat & projMat,
                                        const SbVec2s & viewportSize)
{
  if (!entry.lineDistVBO || !command.geometry.positions ||
      command.geometry.vertexCount == 0) return;

  const SbMatrix view(viewMat);
  const SbMatrix projection(projMat);
  SbMatrix model(command.modelMatrix);
  const uint32_t strideFloats = static_cast<uint32_t>(
    command.geometry.vertexStride ? command.geometry.vertexStride
                                  : sizeof(float) * 3) / sizeof(float);
  const uint32_t count = command.geometry.indexCount && command.geometry.indices
    ? command.geometry.indexCount : command.geometry.vertexCount;
  std::vector<SbVec2f> windowPositions(command.geometry.vertexCount);
  for (uint32_t i = 0; i < command.geometry.vertexCount; ++i) {
    const float * p = command.geometry.positions + i * strideFloats;
    SbVec3f point(p[0], p[1], p[2]);
    SbVec3f transformed;
    model.multVecMatrix(point, transformed);
    view.multVecMatrix(transformed, transformed);
    projection.multVecMatrix(transformed, transformed);
    windowPositions[i].setValue(
      (transformed[0] * 0.5f + 0.5f) * viewportSize[0],
      (transformed[1] * 0.5f + 0.5f) * viewportSize[1]);
  }

  std::vector<float> distances(command.geometry.vertexCount, 0.0f);
  auto indexAt = [&command](uint32_t i) {
    return command.geometry.indices ? command.geometry.indices[i] : i;
  };
  auto segmentLength = [&windowPositions](uint32_t first, uint32_t second) {
    return (windowPositions[second] - windowPositions[first]).length();
  };
  if (command.geometry.topology == SO_TOPOLOGY_LINE_STRIP) {
    for (uint32_t i = 1; i < count; ++i) {
      const uint32_t previous = indexAt(i - 1);
      const uint32_t current = indexAt(i);
      distances[current] = distances[previous] +
        segmentLength(previous, current);
    }
  }
  else {
    for (uint32_t i = 0; i + 1 < count; i += 2) {
      const uint32_t first = indexAt(i);
      const uint32_t second = indexAt(i + 1);
      distances[first] = 0.0f;
      distances[second] = segmentLength(first, second);
    }
  }
  cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, entry.lineDistVBO);
  cc_glglue_glBufferData(this->glue, GL_ARRAY_BUFFER,
                         distances.size() * sizeof(float), distances.data(),
                         GL_DYNAMIC_DRAW);
  cc_glglue_glBindBuffer(this->glue, GL_ARRAY_BUFFER, 0);
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
    const bool lineGeometry = geometry.topology == SO_TOPOLOGY_LINES ||
      geometry.topology == SO_TOPOLOGY_LINE_STRIP;
    const bool geometryMatches = entry.posVBO != 0 &&
      entry.cacheGeneration == generation &&
      entry.posKey == geometry.positions &&
      entry.normKey == geometry.normals &&
      entry.colorKey == geometry.colors &&
      entry.texcoordKey == geometry.texcoords &&
      entry.idxKey == geometry.indices &&
      entry.vertexCount == geometry.vertexCount &&
      entry.normalCount == geometry.normalCount &&
      entry.indexCount == geometry.indexCount &&
      entry.vertexStride == vertexStride &&
      entry.texcoordStride == geometry.texcoordStride &&
      entry.lineDistKey == (lineGeometry ? geometry.positions : nullptr) &&
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
SoGLRenderBackend::uploadLighting(const SoDrawList & drawlist,
                                  const SoRenderCommand & command,
                                  const GLuint requestedProgram)
{
  const GLuint program = requestedProgram ? requestedProgram : this->shaderProgram;
  const bool visualProgram = program == this->shaderProgram;
  auto location = [this, program, visualProgram](GLint visualLocation,
                                                  const char * name) {
    return visualProgram ? visualLocation
      : cc_glglue_glGetUniformLocation(this->glue, program, name);
  };
  const SoLightingData * lighting = drawlist.getLighting(command.lightingHandle);
  static const SoLightingData emptyLighting;
  if (!lighting) {
    lighting = &emptyLighting;
    if (command.lightingHandle != 0) {
      static std::once_flag invalidHandleWarning;
      std::call_once(invalidHandleWarning, []() {
        SoDebugError::postWarning(
          "SoGLRenderBackend::uploadLighting",
          "Draw command references missing lighting data; no headlight is "
          "synthesized.");
      });
    }
  }

  const SbVec3f & ambient = lighting->ambient;
  this->glue->glUniform3f(location(this->uAmbientLightLocation, "u_ambientLight"),
                          ambient[0], ambient[1], ambient[2]);

  GLint types[MAX_SHADER_LIGHTS] = {};
  GLfloat colors[MAX_SHADER_LIGHTS * 3] = {};
  GLfloat directions[MAX_SHADER_LIGHTS * 3] = {};
  GLfloat positions[MAX_SHADER_LIGHTS * 3] = {};
  GLfloat attenuations[MAX_SHADER_LIGHTS * 3] = {};
  GLfloat spotParams[MAX_SHADER_LIGHTS * 2] = {};
  const int count = std::min<int>(static_cast<int>(lighting->lights.size()),
                                  MAX_SHADER_LIGHTS);
  if (static_cast<int>(lighting->lights.size()) > MAX_SHADER_LIGHTS) {
    static std::once_flag lightLimitWarning;
    std::call_once(lightLimitWarning, []() {
      SoDebugError::postWarning(
        "SoGLRenderBackend::uploadLighting",
        "The Visual program supports eight lights; additional retained "
        "lights are ignored by this executor.");
    });
  }
  for (int i = 0; i < count; ++i) {
    const SoLightData & light = lighting->lights[static_cast<size_t>(i)];
    types[i] = static_cast<GLint>(light.type);
    colors[i * 3 + 0] = light.color[0];
    colors[i * 3 + 1] = light.color[1];
    colors[i * 3 + 2] = light.color[2];
    directions[i * 3 + 0] = light.direction[0];
    directions[i * 3 + 1] = light.direction[1];
    directions[i * 3 + 2] = light.direction[2];
    positions[i * 3 + 0] = light.position[0];
    positions[i * 3 + 1] = light.position[1];
    positions[i * 3 + 2] = light.position[2];
    attenuations[i * 3 + 0] = light.attenuation[0];
    attenuations[i * 3 + 1] = light.attenuation[1];
    attenuations[i * 3 + 2] = light.attenuation[2];
    spotParams[i * 2 + 0] = light.spotCutoffCos;
    spotParams[i * 2 + 1] = light.spotExponent;
  }
  this->glue->glUniform1i(location(this->uLightCountLocation, "u_lightCount"), count);
  this->glue->glUniform1iv(location(this->uLightTypeLocation, "u_lightType"), MAX_SHADER_LIGHTS, types);
  this->glue->glUniform3fv(location(this->uLightColorLocation, "u_lightColor"), MAX_SHADER_LIGHTS,
                           colors);
  this->glue->glUniform3fv(location(this->uLightDirectionLocation, "u_lightDirection"), MAX_SHADER_LIGHTS,
                           directions);
  this->glue->glUniform3fv(location(this->uLightPositionLocation, "u_lightPosition"), MAX_SHADER_LIGHTS,
                           positions);
  this->glue->glUniform3fv(location(this->uLightAttenuationLocation, "u_lightAttenuation"), MAX_SHADER_LIGHTS,
                           attenuations);
  this->glue->glUniform2fv(location(this->uLightSpotParamsLocation, "u_lightSpotParams"), MAX_SHADER_LIGHTS,
                           spotParams);
}

void
SoGLRenderBackend::bindRasterCommon(const SoDrawList & drawlist,
                                    const SoRenderCommand & command,
                                    const SbMat & viewMat,
                                    const SbMat & projMat,
                                    const SbVec4f & color,
                                    const bool useVertexColor,
                                    const bool textured,
                                    const GLuint program)
{
  auto uniform = [this, program](const char * name) {
    return cc_glglue_glGetUniformLocation(this->glue, program, name);
  };
  SbMat model;
  command.modelMatrix.getValue(model);
  this->glue->glUniformMatrix4fv(uniform("u_view"), 1, GL_FALSE,
                                 &viewMat[0][0]);
  this->glue->glUniformMatrix4fv(uniform("u_proj"), 1, GL_FALSE,
                                 &projMat[0][0]);
  this->glue->glUniformMatrix4fv(uniform("u_model"), 1, GL_FALSE,
                                 &model[0][0]);
  this->glue->glUniform4f(uniform("u_color"),
                          color[0], color[1], color[2], color[3]);
  this->glue->glUniform1f(uniform("u_useVertexColor"),
                          useVertexColor ? 1.0f : 0.0f);

  const SoShadingModel shadingModel =
    (command.material.featureFlags & SO_FEAT_BASE_COLOR)
      ? SO_SHADING_UNLIT : command.material.shadingModel;
  this->glue->glUniform1i(uniform("u_shadingModel"),
                          static_cast<GLint>(shadingModel));
  const SbVec4f & emissive = command.material.emissive;
  const SbVec4f & ambient = command.material.ambient;
  const SbVec4f & specular = command.material.specular;
  this->glue->glUniform3f(uniform("u_emissiveColor"),
                          emissive[0], emissive[1], emissive[2]);
  this->glue->glUniform3f(uniform("u_materialAmbient"),
                          ambient[0], ambient[1], ambient[2]);
  this->glue->glUniform3f(uniform("u_materialSpecular"),
                          specular[0], specular[1], specular[2]);
  this->glue->glUniform1f(uniform("u_materialShininess"),
                          command.material.shininess);
  this->glue->glUniform1f(uniform("u_twoSidedLighting"),
                          command.material.twoSidedLighting ? 1.0f : 0.0f);
  this->glue->glUniform1f(uniform("u_vertexColorAlphaIncludesOpacity"),
                          command.material.vertexColorAlphaIncludesOpacity
                            ? 1.0f : 0.0f);
  this->glue->glUniform1f(uniform("u_textureAlphaIncludesOpacity"),
                          command.material.textureAlphaIncludesOpacity
                            ? 1.0f : 0.0f);
  const bool textureHasAlpha = command.material.texture.numComponents == 2 ||
    command.material.texture.numComponents == 4;
  this->glue->glUniform1f(uniform("u_textureHasAlpha"),
                          textureHasAlpha ? 1.0f : 0.0f);
  this->glue->glUniform1f(uniform("u_textureEnabled"),
                          textured ? 1.0f : 0.0f);
  this->glue->glUniform1i(uniform("u_texture"), 0);
  this->glue->glUniform1i(uniform("u_textureModel"),
                          static_cast<GLint>(command.material.texture.model));
  const SbVec4f & textureBlend = command.material.texture.blendColor;
  this->glue->glUniform4f(uniform("u_textureBlendColor"),
                          textureBlend[0], textureBlend[1],
                          textureBlend[2], textureBlend[3]);
  this->glue->glUniform1i(
    uniform("u_alphaTestFunction"),
    command.state.alphaTest.policy == SO_ALPHA_TEST_POLICY_NONE
      ? 0 : static_cast<GLint>(command.state.alphaTest.function));
  this->glue->glUniform1f(uniform("u_alphaTestReference"),
                          command.state.alphaTest.reference);
  this->uploadLighting(drawlist, command, program);
}

void
SoGLRenderBackend::bindPointShader(const SoRenderCommand & command,
                                   const SbMat & viewMat,
                                   const SbMat & projMat,
                                   const SbVec4f & color,
                                   const bool useVertexColor,
                                   const float pointSize,
                                   const SbVec2s & viewportSize,
                                   const bool triangleInput,
                                   const SoDrawList & drawlist,
                                   const bool textured)
{
  const GLuint program = triangleInput ? this->pointTriangleShaderProgram
                                       : this->pointShaderProgram;
  cc_glglue_glUseProgram(this->glue, program);
  this->bindRasterCommon(drawlist, command, viewMat, projMat, color,
                         useVertexColor, textured, program);
  auto uniform = [this, program](const char * name) {
    return cc_glglue_glGetUniformLocation(this->glue, program, name);
  };
  this->glue->glUniform1f(uniform("u_pointSize"), pointSize);
  this->glue->glUniform2f(uniform("u_vpSize"),
                          static_cast<float>(viewportSize[0]),
                          static_cast<float>(viewportSize[1]));
  if (triangleInput) {
    this->glue->glUniform1f(
      uniform("u_cullBackFaces"),
      command.state.raster.cullMode ? 1.0f : 0.0f);
    this->glue->glUniform1f(
      uniform("u_frontFaceCCW"),
      command.state.raster.frontFaceCCW ? 1.0f : 0.0f);
    this->glue->glUniform1f(
      uniform("u_triangleStrip"),
      command.geometry.topology == SO_TOPOLOGY_TRIANGLE_STRIP
        ? 1.0f : 0.0f);
  }
}

void
SoGLRenderBackend::bindLineShader(const SoRenderCommand & command,
                                  const SbMat & viewMat,
                                  const SbMat & projMat,
                                   const SbVec4f & color,
                                   const bool useVertexColor,
                                   const float lineWidth,
                                   const SbVec2s & viewportSize,
                                   const bool triangleInput,
                                   const SoDrawList & drawlist,
                                   const bool textured)
{
  const GLuint program = triangleInput ? this->lineTriangleShaderProgram
                                       : this->lineShaderProgram;
  cc_glglue_glUseProgram(this->glue, program);
  this->bindRasterCommon(drawlist, command, viewMat, projMat, color,
                         useVertexColor, textured, program);
  auto uniform = [this, program](const char * name) {
    return cc_glglue_glGetUniformLocation(this->glue, program, name);
  };
  this->glue->glUniform1f(uniform("u_lineWidth"), lineWidth);
  this->glue->glUniform2f(uniform("u_vpSize"),
                          static_cast<float>(viewportSize[0]),
                          static_cast<float>(viewportSize[1]));
  if (triangleInput) {
    this->glue->glUniform1f(
      uniform("u_cullBackFaces"),
      command.state.raster.cullMode ? 1.0f : 0.0f);
    this->glue->glUniform1f(
      uniform("u_frontFaceCCW"),
      command.state.raster.frontFaceCCW ? 1.0f : 0.0f);
    this->glue->glUniform1f(
      uniform("u_triangleStrip"),
      command.geometry.topology == SO_TOPOLOGY_TRIANGLE_STRIP
        ? 1.0f : 0.0f);
  }
  this->glue->glUniform1i(
    uniform("u_stipplePattern"),
    static_cast<GLint>(command.state.raster.linePattern));
  this->glue->glUniform1f(
    uniform("u_stippleScale"),
    static_cast<GLfloat>(std::max(1, static_cast<int>(
      command.state.raster.linePatternScale))));
}

void
SoGLRenderBackend::bindPixelShader(const SoRenderCommand & command,
                                   const SbMat & viewMat,
                                   const SbMat & projMat,
                                   const SbVec2s & viewportOrigin,
                                   const SbVec2s & viewportSize)
{
  cc_glglue_glUseProgram(this->glue, this->pixelShaderProgram);
  SbMat model;
  command.modelMatrix.getValue(model);
  this->glue->glUniformMatrix4fv(this->pixelUViewLocation, 1, GL_FALSE,
                                 &viewMat[0][0]);
  this->glue->glUniformMatrix4fv(this->pixelUProjLocation, 1, GL_FALSE,
                                 &projMat[0][0]);
  this->glue->glUniformMatrix4fv(this->pixelUModelLocation, 1, GL_FALSE,
                                 &model[0][0]);

  const GLsizei stride = static_cast<GLsizei>(
    command.geometry.vertexStride ? command.geometry.vertexStride : sizeof(float) * 3);
  const char * raw = reinterpret_cast<const char *>(command.geometry.positions);
  SbVec3f center(0.0f, 0.0f, 0.0f);
  for (uint32_t i = 0; i < command.geometry.vertexCount; ++i) {
    const float * position = reinterpret_cast<const float *>(raw + i * stride);
    center += SbVec3f(position[0], position[1], position[2]);
  }
  if (command.geometry.vertexCount) {
    center /= static_cast<float>(command.geometry.vertexCount);
  }
  this->glue->glUniform3f(this->pixelUQuadCenterLocation,
                          center[0], center[1], center[2]);
  this->glue->glUniform2f(this->pixelUTexSizeLocation,
                          static_cast<float>(command.material.texture.width),
                          static_cast<float>(command.material.texture.height));
  this->glue->glUniform2f(this->pixelUViewportOriginLocation,
                          static_cast<float>(viewportOrigin[0]),
                          static_cast<float>(viewportOrigin[1]));
  this->glue->glUniform2f(this->pixelUVpSizeLocation,
                          static_cast<float>(viewportSize[0]),
                          static_cast<float>(viewportSize[1]));
  this->glue->glUniform2f(this->pixelUPixelOriginLocation,
                          static_cast<float>(command.pixelRaster.originX),
                          static_cast<float>(command.pixelRaster.originY));
  this->glue->glUniform1i(this->pixelUTextureLocation, 0);
  this->glue->glUniform4f(this->pixelUTexModColorLocation, 1.0f, 1.0f,
                          1.0f, 1.0f);
  const SbVec4f & color = command.material.diffuse;
  this->glue->glUniform4f(this->pixelUColorLocation,
                          color[0], color[1], color[2], color[3]);
  this->glue->glUniform1f(
    this->pixelUVertexColorAlphaIncludesOpacityLocation,
    command.material.vertexColorAlphaIncludesOpacity ? 1.0f : 0.0f);
  this->glue->glUniform1f(
    this->pixelUTextureAlphaIncludesOpacityLocation,
    command.material.textureAlphaIncludesOpacity ? 1.0f : 0.0f);
  this->glue->glUniform1i(
    this->pixelUAlphaTestFunctionLocation,
    command.state.alphaTest.policy == SO_ALPHA_TEST_POLICY_NONE
      ? 0 : static_cast<GLint>(command.state.alphaTest.function));
  this->glue->glUniform1f(this->pixelUAlphaTestReferenceLocation,
                          command.state.alphaTest.reference);
}

void
SoGLRenderBackend::drawCommand(const SoDrawList & drawlist,
                               const SoRenderCommand & command,
                               const SbMat & viewMat,
                               const SbMat & projMat,
                               const SoRenderParams & params)
{
  if (!command.state.raster.visible) return;
  if (!command.geometry.positions || command.geometry.vertexCount == 0) return;
  if (command.state.raster.viewportOverride &&
      !command.state.raster.viewportEnabled) return;
  clearCommandDepth(command, params);
  const auto found = this->commandToCache.find(&command);
  if (found == this->commandToCache.end()) return;
  CachedGPUCommand & entry = this->gpuCache[found->second];
  if (!entry.vao) return;

  const GLenum primitive = topologyToGL(command.geometry.topology);
  const bool textured = entry.textureId != 0 && entry.texcoordVBO != 0;
  const bool pixelRaster = textured &&
    command.pixelRaster.kind != SO_PIXEL_RASTER_NONE;
  const float dpr = params.devicePixelRatio > 0.0f
    ? params.devicePixelRatio : 1.0f;
  const SbVec2s defaultViewportSize = params.viewport.getViewportSizePixels();
  const SbVec2s commandViewportSize = command.state.raster.viewportOverride
    ? SbVec2s(static_cast<short>(command.state.raster.viewportWidth),
              static_cast<short>(command.state.raster.viewportHeight))
    : defaultViewportSize;
  const SbVec2s & viewportSize = commandViewportSize;
  const float pointSize = std::max(1.0f, command.state.raster.pointSize) * dpr;
  const float lineWidth = std::max(1.0f, command.state.raster.lineWidth) * dpr;
  const uint8_t fillMode = command.state.raster.fillMode;
  const bool triangleTopology = primitive == GL_TRIANGLES ||
    primitive == GL_TRIANGLE_STRIP;
  const bool lineTopology = primitive == GL_LINES ||
    primitive == GL_LINE_STRIP;
  const bool pointTopology = primitive == GL_POINTS;
  const bool lineRaster = lineTopology || (fillMode == 1 && triangleTopology);
  const bool pointRaster = pointTopology || (fillMode == 2 && triangleTopology);
  const bool lineEmulationRequired = lineWidth > this->nativeLineWidthMax ||
    command.state.raster.linePattern != 0xFFFF;
  const bool usePointShader = !pixelRaster && pointRaster &&
    this->pointShaderProgram != 0 && pointSize > this->nativePointSizeMax;
  const bool useLineShader = !pixelRaster && lineRaster &&
    this->lineShaderProgram != 0 && lineEmulationRequired;
  const bool lineTriangleInput = useLineShader && fillMode == 1 &&
    triangleTopology;
  const bool pointTriangleInput = usePointShader && fillMode == 2 &&
    triangleTopology;

  if (useLineShader && lineTopology) {
    this->updateLineDistances(entry, command, viewMat, projMat,
                              params.viewport.getViewportSizePixels());
  }

  applyCommandViewport(command, params);
  SbMat effectiveView;
  SbMat effectiveProj;
  std::memcpy(effectiveView, viewMat, sizeof(effectiveView));
  std::memcpy(effectiveProj, projMat, sizeof(effectiveProj));
  if (command.state.useCommandMatrices) {
    command.viewMatrix.getValue(effectiveView);
    command.projMatrix.getValue(effectiveProj);
  }
  this->glue->glUniformMatrix4fv(this->uViewLocation, 1, GL_FALSE,
                                 &effectiveView[0][0]);
  this->glue->glUniformMatrix4fv(this->uProjLocation, 1, GL_FALSE,
                                 &effectiveProj[0][0]);
  SbMat model;
  command.modelMatrix.getValue(model);
  this->glue->glUniformMatrix4fv(this->uModelLocation, 1, GL_FALSE,
                                 &model[0][0]);

  const SbVec4f & color = command.material.diffuse;
  this->glue->glUniform4f(this->uColorLocation,
                          color[0], color[1], color[2], color[3]);
  this->glue->glUniform1f(this->uUseVertexColorLocation,
                          entry.colorVBO ? 1.0f : 0.0f);
  this->glue->glUniform1f(this->uVertexColorAlphaIncludesOpacityLocation,
                          command.material.vertexColorAlphaIncludesOpacity
                            ? 1.0f : 0.0f);
  this->glue->glUniform1f(this->uTextureAlphaIncludesOpacityLocation,
                          command.material.textureAlphaIncludesOpacity
                            ? 1.0f : 0.0f);
  const bool textureHasAlpha = command.material.texture.numComponents == 2 ||
    command.material.texture.numComponents == 4;
  this->glue->glUniform1f(this->uTextureHasAlphaLocation,
                          textureHasAlpha ? 1.0f : 0.0f);
  const SoShadingModel shadingModel =
    (command.material.featureFlags & SO_FEAT_BASE_COLOR)
      ? SO_SHADING_UNLIT : command.material.shadingModel;
  this->glue->glUniform1i(this->uShadingModelLocation,
                          static_cast<GLint>(shadingModel));
  const SbVec4f & emissive = command.material.emissive;
  const SbVec4f & ambient = command.material.ambient;
  const SbVec4f & specular = command.material.specular;
  this->glue->glUniform3f(this->uEmissiveColorLocation,
                          emissive[0], emissive[1], emissive[2]);
  this->glue->glUniform3f(this->uMaterialAmbientLocation,
                          ambient[0], ambient[1], ambient[2]);
  this->glue->glUniform3f(this->uMaterialSpecularLocation,
                          specular[0], specular[1], specular[2]);
  this->glue->glUniform1f(this->uMaterialShininessLocation,
                          command.material.shininess);
  this->glue->glUniform1f(this->uTwoSidedLightingLocation,
                          command.material.twoSidedLighting ? 1.0f : 0.0f);
  this->uploadLighting(drawlist, command);

  if (command.state.depth.enabled) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(depthFunctionToGL(command.state.depth.func));
  }
  else {
    glDisable(GL_DEPTH_TEST);
  }
  // Match LegacyGL's transparent-object contract and keep transparent
  // geometry out of the depth buffer; otherwise triangle order inside a
  // retained command changes visibility and later transparent passes
  // self-occlude unpredictably.
  glDepthMask(command.state.depth.writeEnabled &&
              command.pass != SO_RENDERPASS_TRANSPARENT
                ? GL_TRUE : GL_FALSE);
  glDepthRange(command.state.depth.range[0], command.state.depth.range[1]);

  const bool triangleFallback = lineTriangleInput || pointTriangleInput;
  if (command.state.raster.cullMode && !triangleFallback) {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
  }
  else {
    glDisable(GL_CULL_FACE);
  }
  glFrontFace(command.state.raster.frontFaceCCW ? GL_CCW : GL_CW);
  if (!useLineShader && fillMode == 1 && triangleTopology) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  else if (!usePointShader && fillMode == 2 && triangleTopology) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
  }
  if (!usePointShader && (primitive == GL_POINTS || fillMode == 2)) {
    glPointSize(pointSize);
  }
  if (!useLineShader &&
      (primitive == GL_LINES || primitive == GL_LINE_STRIP || fillMode == 1)) {
    glLineWidth(lineWidth);
  }

  const bool blending = command.state.blend.enabled ||
    command.pass == SO_RENDERPASS_TRANSPARENT || color[3] < 0.999f;
  if (blending) {
    glEnable(GL_BLEND);
    if (isDualSourceBlendFactor(command.state.blend.srcRGBFactor) ||
        isDualSourceBlendFactor(command.state.blend.dstRGBFactor) ||
        isDualSourceBlendFactor(command.state.blend.srcAlphaFactor) ||
        isDualSourceBlendFactor(command.state.blend.dstAlphaFactor)) {
      static std::once_flag dualSourceWarning;
      std::call_once(dualSourceWarning, []() {
        SoDebugError::postWarning(
          "SoGLRenderBackend::drawCommand",
          "Dual-source blend factors are not supported by the Visual "
          "program; using primary-source factors for execution.");
      });
    }
    cc_glglue_glBlendFuncSeparate(
      this->glue, blendFactorToGL(command.state.blend.srcRGBFactor),
      blendFactorToGL(command.state.blend.dstRGBFactor),
      blendFactorToGL(command.state.blend.srcAlphaFactor),
      blendFactorToGL(command.state.blend.dstAlphaFactor));
    if (cc_glglue_has_blendequation(this->glue) &&
        command.state.blend.rgbEquation == command.state.blend.alphaEquation) {
      cc_glglue_glBlendEquation(
        this->glue, blendEquationToGL(command.state.blend.rgbEquation));
    }
  }
  else {
    glDisable(GL_BLEND);
  }
  const bool linePrimitive = primitive == GL_LINES || primitive == GL_LINE_STRIP ||
    fillMode == 1;
  const bool pointPrimitive = primitive == GL_POINTS || fillMode == 2;
  const bool filledPrimitive = !linePrimitive && !pointPrimitive;
  const bool polygonOffsetApplies = (filledPrimitive &&
                                     command.state.raster.polygonOffsetFilled) ||
    (linePrimitive && command.state.raster.polygonOffsetLines) ||
    (pointPrimitive && command.state.raster.polygonOffsetPoints);
  const bool polygonOffset = polygonOffsetApplies &&
    (command.state.raster.polygonOffsetFactor != 0.0f ||
     command.state.raster.polygonOffsetUnits != 0.0f);
  if (polygonOffset) {
    const GLenum polygonOffsetTarget = filledPrimitive
      ? GL_POLYGON_OFFSET_FILL
      : (linePrimitive ? GL_POLYGON_OFFSET_LINE : GL_POLYGON_OFFSET_POINT);
    glEnable(polygonOffsetTarget);
    glPolygonOffset(command.state.raster.polygonOffsetFactor,
                    command.state.raster.polygonOffsetUnits);
  }
  this->glue->glUniform1i(
    this->uAlphaTestFunctionLocation,
    command.state.alphaTest.policy == SO_ALPHA_TEST_POLICY_NONE
      ? 0 : static_cast<GLint>(command.state.alphaTest.function));
  this->glue->glUniform1f(this->uAlphaTestReferenceLocation,
                          command.state.alphaTest.reference);

  this->glue->glUniform1f(this->uTextureEnabledLocation,
                          textured ? 1.0f : 0.0f);
  if (textured) {
    cc_glglue_glActiveTexture(this->glue, GL_TEXTURE0);
    cc_glglue_glBindTexture(this->glue, GL_TEXTURE_2D, entry.textureId);
    this->glue->glUniform1i(this->uTextureLocation, 0);
  }
  this->glue->glUniform1i(
    this->uTextureModelLocation,
    static_cast<GLint>(command.material.texture.model));
  const SbVec4f & textureBlend = command.material.texture.blendColor;
  this->glue->glUniform4f(this->uTextureBlendColorLocation,
                          textureBlend[0], textureBlend[1],
                          textureBlend[2], textureBlend[3]);

  if (pixelRaster) {
    this->bindPixelShader(command, viewMat, projMat,
                          params.viewport.getViewportOriginPixels(),
                          params.viewport.getViewportSizePixels());
  }
  else if (usePointShader) {
    this->bindPointShader(command, viewMat, projMat, color,
                          entry.colorVBO != 0, pointSize,
                          params.viewport.getViewportSizePixels(),
                          pointTriangleInput, drawlist, textured);
  }
  else if (useLineShader) {
    this->bindLineShader(command, viewMat, projMat, color,
                         entry.colorVBO != 0, lineWidth,
                         params.viewport.getViewportSizePixels(),
                         lineTriangleInput, drawlist, textured);
  }

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
  if (pixelRaster || usePointShader || useLineShader) {
    cc_glglue_glUseProgram(this->glue, this->shaderProgram);
  }
  if (textured) cc_glglue_glBindTexture(this->glue, GL_TEXTURE_2D, 0);
  if (polygonOffset) {
    const GLenum polygonOffsetTarget = filledPrimitive
      ? GL_POLYGON_OFFSET_FILL
      : (linePrimitive ? GL_POLYGON_OFFSET_LINE : GL_POLYGON_OFFSET_POINT);
    glDisable(polygonOffsetTarget);
  }
  if (fillMode != 0 &&
      (primitive == GL_TRIANGLES || primitive == GL_TRIANGLE_STRIP)) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
  glDepthRange(0.0, 1.0);
  glFrontFace(GL_CCW);
  if (!usePointShader) glPointSize(1.0f);
  if (!useLineShader) glLineWidth(1.0f);
}

void
SoGLRenderBackend::renderOpaquePass(const SoDrawList & drawlist,
                                    const SbMat & viewMat,
                                    const SbMat & projMat,
                                    const SoRenderParams & params)
{
  const std::vector<int> & order = drawlist.getSortedOrder();
  for (int i = 0; i < drawlist.getNumCommands(); ++i) {
    const int index = i < static_cast<int>(order.size()) ? order[i] : i;
    const SoRenderCommand & command = drawlist.getCommand(index);
    if (command.pass == SO_RENDERPASS_OPAQUE) {
      this->drawCommand(drawlist, command, viewMat, projMat, params);
    }
  }
}

void
SoGLRenderBackend::renderTransparentPass(const SoDrawList & drawlist,
                                         const SbMat & viewMat,
                                         const SbMat & projMat,
                                         const SoRenderParams & params)
{
  const std::vector<int> & order = drawlist.getSortedOrder();
  for (int i = 0; i < drawlist.getNumCommands(); ++i) {
    const int index = i < static_cast<int>(order.size()) ? order[i] : i;
    const SoRenderCommand & command = drawlist.getCommand(index);
    if (command.pass == SO_RENDERPASS_TRANSPARENT) {
      this->drawCommand(drawlist, command, viewMat, projMat, params);
    }
  }
}

void
SoGLRenderBackend::renderSelectionPass(const SoDrawList & drawlist,
                                       const SbMat & viewMat,
                                       const SbMat & projMat,
                                       const SoRenderParams & params)
{
  bool hasOverlay = false;
  for (int i = 0; i < drawlist.getNumCommands(); ++i) {
    const SoSelectionData & selection = drawlist.getCommand(i).selection;
    if (selection.selectWholeObject || selection.highlightWholeObject ||
        !selection.selectedElements.empty() ||
        !selection.highlightedElements.empty()) {
      hasOverlay = true;
      break;
    }
  }
  if (!hasOverlay) return;

  cc_glglue_glUseProgram(this->glue, this->shaderProgram);
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  cc_glglue_glBlendFuncSeparate(this->glue, GL_SRC_ALPHA,
                                GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                GL_ONE_MINUS_SRC_ALPHA);
  this->glue->glUniform1f(this->uUseVertexColorLocation, 0.0f);
  this->glue->glUniform1f(this->uTextureEnabledLocation, 0.0f);
  this->glue->glUniform1i(this->uShadingModelLocation, 0);
  this->glue->glUniform1i(this->uAlphaTestFunctionLocation, 0);

  auto drawSelection = [&](const SoRenderCommand & command,
                           const SbVec4f & color,
                           bool whole,
                           const std::vector<int> & elements) {
    const auto found = this->commandToCache.find(&command);
    if (found == this->commandToCache.end()) return;
    const CachedGPUCommand & entry = this->gpuCache[found->second];
    if (!entry.vao) return;

    SbMat model;
    command.modelMatrix.getValue(model);
    this->glue->glUniformMatrix4fv(this->uViewLocation, 1, GL_FALSE,
                                   &viewMat[0][0]);
    this->glue->glUniformMatrix4fv(this->uProjLocation, 1, GL_FALSE,
                                   &projMat[0][0]);
    this->glue->glUniformMatrix4fv(this->uModelLocation, 1, GL_FALSE,
                                   &model[0][0]);
    this->glue->glUniform4f(this->uColorLocation,
                            color[0], color[1], color[2], color[3]);
    this->glue->glBindVertexArray(entry.vao);

    auto drawRange = [&](const SoRenderElementRange & range) {
      if (range.drawCount <= 0) return;
      const GLenum primitive = topologyToGL(command.geometry.topology);
      if (command.geometry.indexCount && command.geometry.indices) {
        const uintptr_t offset = static_cast<uintptr_t>(range.drawStart) *
                                 sizeof(uint32_t);
        cc_glglue_glDrawElements(this->glue, primitive, range.drawCount,
                                 GL_UNSIGNED_INT,
                                 reinterpret_cast<const void *>(offset));
      }
      else {
        cc_glglue_glDrawArrays(this->glue, primitive, range.drawStart,
                               range.drawCount);
      }
    };

    if (whole) {
      if (command.geometry.indexCount && command.geometry.indices) {
        cc_glglue_glDrawElements(this->glue,
                                 topologyToGL(command.geometry.topology),
                                 static_cast<GLsizei>(command.geometry.indexCount),
                                 GL_UNSIGNED_INT, nullptr);
      }
      else {
        cc_glglue_glDrawArrays(this->glue,
                               topologyToGL(command.geometry.topology), 0,
                               static_cast<GLsizei>(command.geometry.vertexCount));
      }
    }
    else {
      for (int element : elements) {
        for (const SoRenderElementRange & range : command.pick.elementRanges) {
          if (range.elementIndex == element) drawRange(range);
        }
      }
    }
    this->glue->glBindVertexArray(0);
  };

  for (int i = 0; i < drawlist.getNumCommands(); ++i) {
    const SoRenderCommand & command = drawlist.getCommand(i);
    const SoSelectionData & selection = command.selection;
    if (selection.selectWholeObject || !selection.selectedElements.empty()) {
      drawSelection(command, selection.selectionColor,
                    selection.selectWholeObject, selection.selectedElements);
    }
    if (selection.highlightWholeObject || !selection.highlightedElements.empty()) {
      drawSelection(command, selection.highlightColor,
                    selection.highlightWholeObject, selection.highlightedElements);
    }
  }

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}

void
SoGLRenderBackend::renderIDBufferPass(const SoDrawList & drawlist,
                                      const SbMat & viewMat,
                                      const SbMat & projMat,
                                      const SoRenderParams & params)
{
  if (!this->pickBuffer || (params.flags & SO_PARAM_INTERACTIVE) ||
      (params.flags & SO_PARAM_SKIP_ID)) return;

  SoDrawList & mutableDrawList = const_cast<SoDrawList &>(drawlist);
  mutableDrawList.buildPickLUT();
  const SbVec2s viewport = params.viewport.getViewportSizePixels();
  if (viewport[0] <= 0 || viewport[1] <= 0) return;

  const int idWidth = std::max(1, static_cast<int>(viewport[0]) / 2);
  const int idHeight = std::max(1, static_cast<int>(viewport[1]) / 2);
  this->pickBuffer->resize(idWidth, idHeight);
  this->pickBuffer->setPickScale(
    static_cast<float>(idWidth) / static_cast<float>(viewport[0]),
    static_cast<float>(idHeight) / static_cast<float>(viewport[1]));
  this->pickBuffer->buildIdColorVBOs(mutableDrawList, params.contextId);

  std::vector<SoIDPassVBOInfo> vboInfo(
    static_cast<size_t>(drawlist.getNumCommands()), SoIDPassVBOInfo{0, 0, 0});
  for (int i = 0; i < drawlist.getNumCommands(); ++i) {
    const SoRenderCommand & command = drawlist.getCommand(i);
    const auto found = this->commandToCache.find(&command);
    if (found == this->commandToCache.end()) continue;
    const CachedGPUCommand & entry = this->gpuCache[found->second];
    vboInfo[static_cast<size_t>(i)] = {
      entry.posVBO, entry.idxVBO, entry.vertexStride
    };
  }

  this->pickBuffer->render(&viewMat[0][0], &projMat[0][0], drawlist,
                           vboInfo.data(), static_cast<int>(vboInfo.size()));
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
  this->shaderProgram = linkProgram(this->glue,
                                    coin_gl_visual_vertex_shadersource,
                                    coin_gl_visual_fragment_shadersource);
  this->lineShaderProgram = linkProgram(this->glue,
                                        coin_gl_wide_line_vertex_shadersource,
                                        coin_gl_wide_line_fragment_shadersource,
                                        coin_gl_wide_line_geometry_shadersource);
  this->lineTriangleShaderProgram = linkProgram(
    this->glue, coin_gl_wide_line_vertex_shadersource,
    coin_gl_wide_line_fragment_shadersource,
    coin_gl_wide_line_triangle_geometry_shadersource);
  this->pointShaderProgram = linkProgram(this->glue,
                                         coin_gl_point_vertex_shadersource,
                                         coin_gl_point_fragment_shadersource,
                                         coin_gl_point_geometry_shadersource);
  this->pointTriangleShaderProgram = linkProgram(
    this->glue, coin_gl_point_vertex_shadersource,
    coin_gl_point_fragment_shadersource,
    coin_gl_point_triangle_geometry_shadersource);
  this->pixelShaderProgram = linkProgram(
    this->glue, coin_gl_pixel_vertex_shadersource,
    coin_gl_pixel_fragment_shadersource);
  if (!this->shaderProgram || !this->lineShaderProgram ||
      !this->lineTriangleShaderProgram || !this->pointShaderProgram ||
      !this->pointTriangleShaderProgram || !this->pixelShaderProgram) {
    if (this->shaderProgram) cc_glglue_glDeleteProgram(this->glue, this->shaderProgram);
    if (this->lineShaderProgram) cc_glglue_glDeleteProgram(this->glue, this->lineShaderProgram);
    if (this->lineTriangleShaderProgram) cc_glglue_glDeleteProgram(this->glue, this->lineTriangleShaderProgram);
    if (this->pointShaderProgram) cc_glglue_glDeleteProgram(this->glue, this->pointShaderProgram);
    if (this->pointTriangleShaderProgram) cc_glglue_glDeleteProgram(this->glue, this->pointTriangleShaderProgram);
    if (this->pixelShaderProgram) cc_glglue_glDeleteProgram(this->glue, this->pixelShaderProgram);
    this->shaderProgram = this->lineShaderProgram =
      this->lineTriangleShaderProgram = this->pointShaderProgram =
      this->pointTriangleShaderProgram = this->pixelShaderProgram = 0;
    return false;
  }

  auto uniform = [this](GLuint program, const char * name) {
    return cc_glglue_glGetUniformLocation(this->glue, program, name);
  };
  const GLuint visual = this->shaderProgram;
  this->uViewLocation = uniform(visual, "u_view");
  this->uProjLocation = uniform(visual, "u_proj");
  this->uModelLocation = uniform(visual, "u_model");
  this->uColorLocation = uniform(visual, "u_color");
  this->uUseVertexColorLocation = uniform(visual, "u_useVertexColor");
  this->uShadingModelLocation = uniform(visual, "u_shadingModel");
  this->uEmissiveColorLocation = uniform(visual, "u_emissiveColor");
  this->uMaterialAmbientLocation = uniform(visual, "u_materialAmbient");
  this->uMaterialSpecularLocation = uniform(visual, "u_materialSpecular");
  this->uMaterialShininessLocation = uniform(visual, "u_materialShininess");
  this->uTwoSidedLightingLocation = uniform(visual, "u_twoSidedLighting");
  this->uVertexColorAlphaIncludesOpacityLocation =
    uniform(visual, "u_vertexColorAlphaIncludesOpacity");
  this->uTextureAlphaIncludesOpacityLocation =
    uniform(visual, "u_textureAlphaIncludesOpacity");
  this->uTextureHasAlphaLocation = uniform(visual, "u_textureHasAlpha");
  this->uAmbientLightLocation = uniform(visual, "u_ambientLight");
  this->uLightCountLocation = uniform(visual, "u_lightCount");
  this->uLightTypeLocation = uniform(visual, "u_lightType");
  this->uLightColorLocation = uniform(visual, "u_lightColor");
  this->uLightDirectionLocation = uniform(visual, "u_lightDirection");
  this->uLightPositionLocation = uniform(visual, "u_lightPosition");
  this->uLightAttenuationLocation = uniform(visual, "u_lightAttenuation");
  this->uLightSpotParamsLocation = uniform(visual, "u_lightSpotParams");
  this->uTextureLocation = uniform(visual, "u_texture");
  this->uTextureEnabledLocation = uniform(visual, "u_textureEnabled");
  this->uTextureModelLocation = uniform(visual, "u_textureModel");
  this->uTextureBlendColorLocation = uniform(visual, "u_textureBlendColor");
  this->uAlphaTestFunctionLocation = uniform(visual, "u_alphaTestFunction");
  this->uAlphaTestReferenceLocation = uniform(visual, "u_alphaTestReference");

  const GLuint line = this->lineShaderProgram;
  this->lineUViewLocation = uniform(line, "u_view");
  this->lineUProjLocation = uniform(line, "u_proj");
  this->lineUModelLocation = uniform(line, "u_model");
  this->lineUColorLocation = uniform(line, "u_color");
  this->lineUUseVertexColorLocation = uniform(line, "u_useVertexColor");
  this->lineULineWidthLocation = uniform(line, "u_lineWidth");
  this->lineUVpSizeLocation = uniform(line, "u_vpSize");
  this->lineUStipplePatternLocation = uniform(line, "u_stipplePattern");
  this->lineUStippleScaleLocation = uniform(line, "u_stippleScale");

  const GLuint point = this->pointShaderProgram;
  this->pointUViewLocation = uniform(point, "u_view");
  this->pointUProjLocation = uniform(point, "u_proj");
  this->pointUModelLocation = uniform(point, "u_model");
  this->pointUColorLocation = uniform(point, "u_color");
  this->pointUUseVertexColorLocation = uniform(point, "u_useVertexColor");
  this->pointUPointSizeLocation = uniform(point, "u_pointSize");
  this->pointUVpSizeLocation = uniform(point, "u_vpSize");

  const GLuint pixel = this->pixelShaderProgram;
  this->pixelUViewLocation = uniform(pixel, "u_view");
  this->pixelUProjLocation = uniform(pixel, "u_proj");
  this->pixelUModelLocation = uniform(pixel, "u_model");
  this->pixelUQuadCenterLocation = uniform(pixel, "u_quadCenter");
  this->pixelUTexSizeLocation = uniform(pixel, "u_texSize");
  this->pixelUViewportOriginLocation = uniform(pixel, "u_viewportOrigin");
  this->pixelUVpSizeLocation = uniform(pixel, "u_vpSize");
  this->pixelUPixelOriginLocation = uniform(pixel, "u_pixelOrigin");
  this->pixelUTextureLocation = uniform(pixel, "u_texture");
  this->pixelUTexModColorLocation = uniform(pixel, "u_texModColor");
  this->pixelUColorLocation = uniform(pixel, "u_color");
  this->pixelUVertexColorAlphaIncludesOpacityLocation =
    uniform(pixel, "u_vertexColorAlphaIncludesOpacity");
  this->pixelUTextureAlphaIncludesOpacityLocation =
    uniform(pixel, "u_textureAlphaIncludesOpacity");
  this->pixelUAlphaTestFunctionLocation = uniform(pixel, "u_alphaTestFunction");
  this->pixelUAlphaTestReferenceLocation = uniform(pixel, "u_alphaTestReference");
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

  this->renderOpaquePass(drawlist, view, projection, params);
  this->renderTransparentPass(drawlist, view, projection, params);
  this->renderSelectionPass(drawlist, view, projection, params);
  this->renderIDBufferPass(drawlist, view, projection, params);
  cc_glglue_glUseProgram(this->glue, 0);
  return TRUE;
}

uint32_t
SoGLRenderBackend::pick(const int x, const int y, const int pickRadius) const
{
  return this->pickBuffer ? this->pickBuffer->pick(x, y, pickRadius) : 0;
}
