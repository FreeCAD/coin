#include "rendering/SoCpuRayOwnedSnapshotP.h"
#include "SoRayTraceRendererP.h"

#include <Inventor/SbVec3f.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <stdexcept>
#include <utility>

namespace retained_bridge {
namespace {

struct Failure {
  Code code;
  char detail[512]{};
  Failure(Code value, const char * text) noexcept : code(value) {
    std::size_t length = 0;
    if (text) {
      while (length + 1 < sizeof(detail) && text[length]) {
        detail[length] = text[length];
        ++length;
      }
    }
    detail[length] = '\0';
  }
};
void require(bool condition, Code code, const char * detail) {
  if (!condition) throw Failure{code, detail};
}

void setFailure(Status & status, Code code, const char * detail) {
  status.code = code;
  try { status.detail = detail; }
  catch (const std::bad_alloc &) { status.detail.clear(); }
}

void clearOutput(OwnedFrame & output) {
  // Avoid constructing a fresh Envelope (its provenance string can allocate)
  // while handling an allocation failure. Invalid serial marks no publication.
  output.resources.clear();
  output.commands.clear();
  output.selection.selected.clear();
  output.selection.highlighted.clear();
  output.envelope.sourceCommit.clear();
  output.envelope.sceneUnits.clear();
  output.envelope.coordinateFrame.clear();
  output.envelope.captureSerial = 0;
  output.envelope.expectedGeneration = 0;
  output.envelope.expectedContentRevision = 0;
  output.envelope.allowProducerUserDataOmission = false;
  output.generation = 0;
  output.contentRevision = 0;
  output.triangleCount = 0;
  output.bytesCopied = 0;
}

struct Guard {
  const Limits & limits;
  std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  std::size_t bytes = 0, vertices = 0, indices = 0, ranges = 0;
  explicit Guard(const Limits & value) : limits(value) {
    require(limits.maxCommands <= 65536 && limits.maxResources <= 16384 &&
      limits.maxVertices <= 1048576 && limits.maxIndices <= 3145728 &&
      limits.maxTriangles <= 1048576 && limits.maxElementRanges <= 1048576 &&
      limits.maxSelectionTargets <= 65536 && limits.maxBytes > 0 &&
      limits.maxBytes <= 256 * 1024 * 1024 && limits.maxMilliseconds > 0 &&
      limits.maxMilliseconds <= 60000 && std::isfinite(limits.maxMagnitude) &&
      limits.maxMagnitude > 0 && limits.maxMagnitude <= 1.0e12 &&
      limits.maxWidth > 0 && limits.maxWidth <= 4096 &&
      limits.maxHeight > 0 && limits.maxHeight <= 4096 &&
      limits.maxPixels > 0 && limits.maxPixels <= 4194304,
      Code::INVALID_LIMIT, "adapter limits outside declared finite envelope");
    poll();
  }
  void poll() const {
    require(!limits.cancellation || !limits.cancellation->load(std::memory_order_relaxed),
      Code::CANCELLED, "capture cancelled");
    require(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started).count() <= limits.maxMilliseconds,
      Code::TIME_LIMIT, "adapter cooperative deadline");
  }
  void charge(std::size_t count, std::size_t width) {
    require(width == 0 || count <= std::numeric_limits<std::size_t>::max() / width,
      Code::BYTE_LIMIT, "owned payload size overflow");
    const std::size_t add = count * width;
    require(bytes <= limits.maxBytes && add <= limits.maxBytes - bytes,
      Code::BYTE_LIMIT, "owned payload byte limit");
    bytes += add;
  }
  double number(double value, Code code = Code::INVALID_GEOMETRY) const {
    require(std::isfinite(value) && std::fabs(value) <= limits.maxMagnitude,
      code, "nonfinite or excessive numeric value");
    return value;
  }
  Vec3 vector(const SbVec3f & value, Code code = Code::INVALID_GEOMETRY) const {
    return {{number(value[0], code), number(value[1], code), number(value[2], code)}};
  }
  Matrix4 matrix(const SbMatrix & value, bool affine,
                 const char * role = "unspecified", int commandIndex = -1,
                 std::uint64_t nodeId = 0) const {
    Matrix4 result{};
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
      result[r][c] = number(value[r][c], Code::INVALID_TRANSFORM);
    if (affine) {
      require(value[0][3] == 0 && value[1][3] == 0 && value[2][3] == 0 &&
        value[3][3] == 1, Code::INVALID_TRANSFORM, "model matrix must be affine");
    }
    const double determinant = value.det4();
    if (!(std::isfinite(determinant) && std::fabs(determinant) > 1.0e-12)) {
      char detail[512]{};
      std::snprintf(detail, sizeof(detail),
        "singular or excessively ill-scaled matrix role=%s command=%d node=%llu det=%.17g "
        "matrix=[%.9g,%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g;"
        "%.9g,%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g]",
        role, commandIndex, static_cast<unsigned long long>(nodeId), determinant,
        double(value[0][0]), double(value[0][1]), double(value[0][2]), double(value[0][3]),
        double(value[1][0]), double(value[1][1]), double(value[1][2]), double(value[1][3]),
        double(value[2][0]), double(value[2][1]), double(value[2][2]), double(value[2][3]),
        double(value[3][0]), double(value[3][1]), double(value[3][2]), double(value[3][3]));
      detail[sizeof(detail) - 1] = '\0';
      throw Failure{Code::INVALID_TRANSFORM, detail};
    }
    const SbMatrix inverse = value.inverse();
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
      number(inverse[r][c], Code::INVALID_TRANSFORM);
    return result;
  }
};

bool sameMatrix(const SbMatrix & a, const SbMatrix & b) {
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
    const double scale = std::max(1.0, std::max(std::fabs(double(a[r][c])), std::fabs(double(b[r][c]))));
    if (!std::isfinite(a[r][c]) || !std::isfinite(b[r][c]) ||
        std::fabs(double(a[r][c]) - b[r][c]) > 1.0e-5 * scale) return false;
  }
  return true;
}

bool label(const std::string & value, std::size_t maximum) {
  if (value.empty() || value.size() > maximum) return false;
  for (unsigned char character : value) if (character < 32 || character > 126) return false;
  return true;
}

void validateEnvelope(const Envelope & envelope, Guard & guard) {
  require(envelope.contractVersion == 1 && envelope.captureSerial > 0 &&
    envelope.sourceCommit == "58d328fe777a01f354160d283279f0a9148a540a" &&
    label(envelope.sceneUnits, 64) && label(envelope.coordinateFrame, 128),
    Code::INVALID_ENVELOPE, "invalid source/contract/units/frame provenance");
  require(std::isfinite(envelope.devicePixelRatio) && envelope.devicePixelRatio > 0 &&
    envelope.devicePixelRatio <= 16, Code::INVALID_ENVELOPE, "invalid physical viewport DPR");
  const SbVec2s size = envelope.viewport.getViewportSizePixels();
  const SbVec2s origin = envelope.viewport.getViewportOriginPixels();
  require(size[0] > 0 && size[1] > 0 && size[0] <= guard.limits.maxWidth && size[1] <= guard.limits.maxHeight &&
    std::size_t(size[0]) * std::size_t(size[1]) <= guard.limits.maxPixels && origin[0] >= 0 && origin[1] >= 0,
    Code::INVALID_ENVELOPE, "viewport exceeds diagnostic camera envelope");
  const SbViewVolume & volume = envelope.cameraVolume;
  const bool orthographic = volume.getProjectionType() == SbViewVolume::ORTHOGRAPHIC;
  const double nearDistance = guard.number(volume.getNearDist(), Code::INVALID_ENVELOPE);
  // An orthographic clip box may start behind its eye. Coin's ray segment
  // already starts on the supplied near plane and uses the live projection.
  require((orthographic ||
    volume.getProjectionType() == SbViewVolume::PERSPECTIVE) &&
    (orthographic || nearDistance > 0) &&
    guard.number(volume.getDepth(), Code::INVALID_ENVELOPE) > 0 &&
    guard.number(volume.getWidth(), Code::INVALID_ENVELOPE) > 0 &&
    guard.number(volume.getHeight(), Code::INVALID_ENVELOPE) > 0,
    Code::INVALID_ENVELOPE, "invalid camera volume");
  guard.vector(volume.getProjectionPoint(), Code::INVALID_ENVELOPE);
  const Vec3 direction = guard.vector(volume.getProjectionDirection(), Code::INVALID_ENVELOPE);
  require(direction[0] * direction[0] + direction[1] * direction[1] +
    direction[2] * direction[2] > 0, Code::INVALID_ENVELOPE, "zero camera direction");
  guard.matrix(envelope.viewMatrix, false, "envelope_view");
  guard.matrix(envelope.projectionMatrix, false, "envelope_projection");
  SbMatrix volumeView, volumeProjection;
  volume.getMatrices(volumeView, volumeProjection);
  require(sameMatrix(volumeView, envelope.viewMatrix) &&
    sameMatrix(volumeProjection, envelope.projectionMatrix),
    Code::CAMERA_MISMATCH, "camera volume and supplied matrices disagree");
  guard.charge(1, sizeof(OwnedFrame));
  guard.charge(envelope.sourceCommit.size() + envelope.sceneUnits.size() +
    envelope.coordinateFrame.size(), sizeof(char));
}

void validateState(const SoRenderCommand & command, const Envelope & envelope) {
  const SoRenderState & state = command.state;
  const SoRasterState & raster = state.raster;
  require(command.stage == SoRenderStage::Main && command.opacityClass == SO_OPACITY_OPAQUE &&
    !command.finalizationEnabledBlend && !command.pixelRaster.enabled,
    Code::UNSUPPORTED_STATE, "only opaque main triangles are admitted");
  require(state.depth.enabled && state.depth.writeEnabled &&
    state.depth.func == SO_DEPTH_LEQUAL && state.depth.range[0] == 0 && state.depth.range[1] == 1 &&
    !state.blend.enabled && state.blend.rgbEquation == SO_BLEND_EQUATION_ADD &&
    state.blend.alphaEquation == SO_BLEND_EQUATION_ADD &&
    state.alphaTest.policy == SO_ALPHA_TEST_POLICY_NONE && state.alphaTest.function == SO_ALPHA_TEST_NONE,
    Code::UNSUPPORTED_STATE, "unsupported depth/blend/alpha state");
  require(raster.visible && raster.fillMode == SO_RASTER_FILL && !raster.cullBackFaces &&
    raster.frontFaceCCW && !raster.scissorEnabled && !raster.viewportOverride &&
    (raster.lineWidth == 0 || raster.lineWidth == 1) &&
    (raster.pointSize == 0 || raster.pointSize == 1) && raster.linePattern == 0xFFFF &&
    raster.linePatternScale == 1 && raster.polygonOffsetFactor == 0 &&
    raster.polygonOffsetUnits == 0 && !raster.polygonOffsetFilled &&
    !raster.polygonOffsetLines && !raster.polygonOffsetPoints,
    Code::UNSUPPORTED_STATE, "unsupported raster state");
  if (raster.viewportEnabled) {
    const SbVec2s size = envelope.viewport.getViewportSizePixels();
    const SbVec2s origin = envelope.viewport.getViewportOriginPixels();
    require(raster.viewportX == origin[0] && raster.viewportY == origin[1] &&
      raster.viewportWidth == size[0] && raster.viewportHeight == size[1],
      Code::CAMERA_MISMATCH, "command viewport differs from frame camera");
  }
  if (state.useCommandMatrices) {
    require(sameMatrix(command.viewMatrix, envelope.viewMatrix) &&
      sameMatrix(command.projMatrix, envelope.projectionMatrix),
      Code::CAMERA_MISMATCH, "command camera differs from diagnostic frame");
  }
  require(command.userData == nullptr || envelope.allowProducerUserDataOmission,
    Code::UNSUPPORTED_STATE, "opaque producer userData omission requires caller acknowledgement");
}

Vec4 color(const SbVec4f & value, Guard & guard) {
  Vec4 result{};
  for (int i = 0; i < 4; ++i) {
    result[i] = guard.number(value[i], Code::INVALID_MATERIAL);
    require(result[i] >= 0 && result[i] <= 1, Code::INVALID_MATERIAL, "material color outside unit interval");
  }
  return result;
}

Material copyMaterial(const SoMaterialData & source, Guard & guard) {
  require(!source.texture.pixels && source.texture.width == 0 && source.texture.height == 0 &&
    source.texture.numComponents == 0 && !source.texture.hasTransparency &&
    source.texture.cacheKey == 0 && source.texture.revision == 0,
    Code::UNSUPPORTED_STATE, "textures are outside the first adapter subset");
  require(source.shadingModel == SO_SHADING_UNLIT || source.shadingModel == SO_SHADING_LEGACY_GOURAUD,
    Code::UNSUPPORTED_STATE, "unknown material shading model");
  Material result;
  result.ambient = color(source.ambient, guard);
  result.diffuse = color(source.diffuse, guard);
  result.specular = color(source.specular, guard);
  result.emissive = color(source.emissive, guard);
  result.opacity = guard.number(source.opacity, Code::INVALID_MATERIAL);
  result.shininess = guard.number(source.shininess, Code::INVALID_MATERIAL);
  require(result.opacity == 1 && result.diffuse[3] == 1 && result.shininess >= 0 &&
    result.shininess <= 1, Code::INVALID_MATERIAL, "nonopaque or invalid material");
  result.shadingModel = int(source.shadingModel);
  result.twoSidedLighting = source.twoSidedLighting;
  result.textureAlphaIncludesOpacity = source.textureAlphaIncludesOpacity;
  result.vertexColorAlphaIncludesOpacity = source.vertexColorAlphaIncludesOpacity;
  return result;
}

void copyLighting(const SoDrawList & source, const SoRenderCommand & original,
                  Command & command, const Envelope & envelope, Guard & guard) {
  if (original.lightingHandle == 0) return;
  const SoLightingData * lighting = source.getLighting(original.lightingHandle);
  require(lighting != nullptr, Code::INVALID_METADATA, "invalid source lighting handle");
  require(envelope.diagnosticIgnoreRetainedLighting ||
    (original.material.shadingModel == SO_SHADING_UNLIT && lighting->lights.empty()),
    Code::UNSUPPORTED_STATE, "only ambient-only setups on unlit-source commands are admitted");
  command.ambientLighting = guard.vector(lighting->ambient, Code::INVALID_METADATA);
  for (double value : command.ambientLighting) require(value >= 0 && value <= 1,
    Code::INVALID_METADATA, "ambient lighting outside unit interval");
  require(lighting->lights.size() <= 8, Code::UNSUPPORTED_STATE, "diagnostic source light count limit");
  guard.charge(lighting->lights.size(), sizeof(SoLightData));
  for (const SoLightData & light : lighting->lights) {
    guard.poll();
    require(light.type >= SO_LIGHT_DIRECTIONAL && light.type <= SO_LIGHT_SPOT, Code::INVALID_METADATA, "unknown source light type");
    const Vec3 rgb = guard.vector(light.color, Code::INVALID_METADATA);
    for (double value : rgb) require(value >= 0 && value <= 1, Code::INVALID_METADATA, "source light color outside unit interval");
    guard.vector(light.direction, Code::INVALID_METADATA);
    guard.vector(light.position, Code::INVALID_METADATA);
    const Vec3 attenuation = guard.vector(light.attenuation, Code::INVALID_METADATA);
    for (double value : attenuation) require(value >= 0, Code::INVALID_METADATA, "negative source attenuation");
    require(std::isfinite(light.spotCutoffCos) && light.spotCutoffCos >= -1 && light.spotCutoffCos <= 1 &&
      std::isfinite(light.spotExponent) && light.spotExponent >= 0 && light.spotExponent <= 128,
      Code::INVALID_METADATA, "invalid source spotlight parameters");
    command.sourceLights.push_back(light);
  }
  command.hasLightingSetup = true;
  command.sourceLightingHandle = original.lightingHandle;
}

template <std::size_t Width>
std::array<double, Width> readFloats(const float * source, std::size_t index,
                                   std::size_t stride, Guard & guard) {
  std::array<double, Width> result{};
  const unsigned char * bytes = reinterpret_cast<const unsigned char *>(source) + index * stride;
  for (std::size_t axis = 0; axis < Width; ++axis) {
    float value;
    std::memcpy(&value, bytes + axis * sizeof(float), sizeof(float));
    result[axis] = guard.number(value);
  }
  return result;
}

std::size_t checkedStride(std::uint32_t supplied, std::size_t width, std::size_t count) {
  const std::size_t stride = supplied ? supplied : width * sizeof(float);
  require(stride >= width * sizeof(float) && stride % sizeof(float) == 0 && stride <= 4096,
    Code::INVALID_GEOMETRY, "invalid attribute byte stride");
  require(count == 0 || count - 1 <= (std::numeric_limits<std::size_t>::max() - width * sizeof(float)) / stride,
    Code::INVALID_GEOMETRY, "attribute address arithmetic overflow");
  return stride;
}

std::vector<ElementRange> copyRanges(const SbInlineVector<SoRenderElementRange, 1> & source,
                                    std::size_t drawLimit, Guard & guard) {
  require(source.size() <= guard.limits.maxElementRanges - guard.ranges,
    Code::VALUE_LIMIT, "element range limit");
  guard.ranges += source.size();
  guard.charge(source.size(), sizeof(ElementRange));
  std::vector<ElementRange> result;
  result.reserve(source.size());
  for (const auto & range : source) {
    guard.poll();
    require(range.type >= SO_PICK_OBJECT && range.type <= SO_PICK_VERTEX &&
      range.elementIndex >= -1 && range.drawCount > 0 && range.drawStart <= drawLimit &&
      range.drawCount <= drawLimit - range.drawStart,
      Code::INVALID_METADATA, "invalid source subelement range");
    result.push_back(ElementRange{int(range.type), range.elementIndex, range.drawStart, range.drawCount});
  }
  return result;
}

Resource copyResource(const SoGeometryDesc & geometry, const SoGeometryResource * source,
                      std::uint32_t handle, std::size_t resourceId, Guard & guard) {
  require(geometry.topology == SO_TOPOLOGY_TRIANGLES, Code::UNSUPPORTED_STATE, "only triangle topology is admitted");
  require(geometry.vertexCount > 0 && geometry.positions && geometry.normals &&
    geometry.normalCount == geometry.vertexCount &&
    (geometry.indexCount == 0 ? geometry.indices == nullptr : geometry.indices != nullptr),
    Code::INVALID_GEOMETRY, "missing/inconsistent complete triangle attributes");
  require(geometry.vertexCount <= guard.limits.maxVertices - guard.vertices &&
    geometry.indexCount <= guard.limits.maxIndices - guard.indices,
    Code::VALUE_LIMIT, "aggregate geometry value limit");
  guard.vertices += geometry.vertexCount;
  guard.indices += geometry.indexCount;
  const std::size_t drawCount = geometry.indexCount ? geometry.indexCount : geometry.vertexCount;
  require(drawCount % 3 == 0, Code::INVALID_GEOMETRY, "triangle count is not divisible by three");
  const std::size_t stride = checkedStride(geometry.vertexStride, 3, geometry.vertexCount);
  const std::size_t textureStride = checkedStride(geometry.texcoordStride, 4, geometry.vertexCount);
  guard.charge(1, sizeof(Resource));
  guard.charge(geometry.vertexCount, sizeof(Vec3) * 2);
  guard.charge(geometry.indexCount, sizeof(std::uint32_t));
  if (geometry.colors) guard.charge(geometry.vertexCount, sizeof(Vec4));
  if (geometry.texcoords) guard.charge(geometry.vertexCount, sizeof(Vec4));
  Resource result;
  result.resourceId = resourceId;
  result.sourceHandle = handle;
  result.sourceKey = source ? source->sourceKey : 0;
  result.revision = source ? source->revision : 0;
  result.cacheKey = geometry.cacheKey;
  result.geometryRevision = geometry.revision;
  result.sourceVertexStride = geometry.vertexStride;
  result.sourceTexcoordStride = geometry.texcoordStride;
  result.positions.reserve(geometry.vertexCount);
  result.normals.reserve(geometry.vertexCount);
  if (geometry.colors) result.colors.reserve(geometry.vertexCount);
  if (geometry.texcoords) result.texcoords.reserve(geometry.vertexCount);
  result.indices.reserve(geometry.indexCount);
  for (std::size_t i = 0; i < geometry.vertexCount; ++i) {
    guard.poll();
    result.positions.push_back(readFloats<3>(geometry.positions, i, stride, guard));
    const Vec3 normal = readFloats<3>(geometry.normals, i, stride, guard);
    require(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2] > 1.0e-24,
      Code::INVALID_GEOMETRY, "zero source normal");
    result.normals.push_back(normal);
    if (geometry.colors) {
      const Vec4 rgba = readFloats<4>(geometry.colors, i, sizeof(float) * 4, guard);
      for (double channel : rgba) require(channel >= 0 && channel <= 1,
        Code::INVALID_MATERIAL, "vertex color outside unit interval");
      require(rgba[3] == 1, Code::INVALID_MATERIAL, "nonopaque vertex color");
      result.colors.push_back(rgba);
    }
    if (geometry.texcoords) result.texcoords.push_back(readFloats<4>(geometry.texcoords, i, textureStride, guard));
  }
  for (std::size_t i = 0; i < geometry.indexCount; ++i) {
    guard.poll();
    const std::uint32_t index = geometry.indices[i];
    require(index < geometry.vertexCount, Code::INVALID_GEOMETRY, "triangle index outside vertices");
    result.indices.push_back(index);
  }
  if (source) result.elementRanges = copyRanges(source->elementRanges, drawCount, guard);
  return result;
}

void copySelection(const SoSelectionState & source, OwnedFrame & output, Guard & guard) {
  require(source.selected.size() <= guard.limits.maxSelectionTargets &&
    source.highlighted.size() <= guard.limits.maxSelectionTargets - source.selected.size(),
    Code::VALUE_LIMIT, "selection target limit");
  guard.charge(source.selected.size() + source.highlighted.size(), sizeof(SoSelectionTarget));
  for (const auto * targets : {&source.selected, &source.highlighted}) for (const auto & target : *targets) {
    guard.poll();
    require(target.commandIndex >= -1 &&
      (target.commandIndex == -1 || std::size_t(target.commandIndex) < output.commands.size()) &&
      target.type >= SO_PICK_OBJECT && target.type <= SO_PICK_VERTEX && target.elementIndex >= -1,
      Code::INVALID_METADATA, "invalid selection target");
    for (int i = 0; i < 4; ++i) require(std::isfinite(target.color[i]) &&
      target.color[i] >= 0 && target.color[i] <= 1, Code::INVALID_METADATA, "invalid selection color");
  }
  output.selection = source;
}

SbMatrix matrixFromOwned(const Matrix4 & source, Guard & guard) {
  SbMatrix result;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
    result[r][c] = static_cast<float>(guard.number(source[r][c], Code::INVALID_TRANSFORM));
  guard.matrix(result, true, "owned_model_conversion");
  return result;
}

coin_raytrace::Material legacyMaterial(const Material & source, const Vec4 * vertexColor) {
  coin_raytrace::Material result;
  for (int axis = 0; axis < 3; ++axis) {
    result.ambient[axis] = source.ambient[axis];
    result.diffuse[axis] = vertexColor ? (*vertexColor)[axis] : source.diffuse[axis];
    result.specular[axis] = source.specular[axis];
    result.emission[axis] = source.emissive[axis];
  }
  result.shininess = source.shininess;
  result.transparency = 0;
  return result;
}

} // namespace

const char * codeString(Code code) {
  switch (code) {
#define BRIDGE_CODE(name) case Code::name: return #name
    BRIDGE_CODE(OK); BRIDGE_CODE(INVALID_ENVELOPE); BRIDGE_CODE(STALE_FRAME);
    BRIDGE_CODE(INVALID_LIMIT); BRIDGE_CODE(COMMAND_LIMIT); BRIDGE_CODE(RESOURCE_LIMIT);
    BRIDGE_CODE(VALUE_LIMIT); BRIDGE_CODE(TRIANGLE_LIMIT); BRIDGE_CODE(BYTE_LIMIT);
    BRIDGE_CODE(TIME_LIMIT); BRIDGE_CODE(CANCELLED); BRIDGE_CODE(UNSUPPORTED_STATE);
    BRIDGE_CODE(INVALID_GEOMETRY); BRIDGE_CODE(INVALID_MATERIAL); BRIDGE_CODE(INVALID_TRANSFORM);
    BRIDGE_CODE(CAMERA_MISMATCH); BRIDGE_CODE(INVALID_METADATA); BRIDGE_CODE(ALLOCATION_FAILURE);
    BRIDGE_CODE(CAPTURE_FAILURE);
#undef BRIDGE_CODE
  }
  return "UNKNOWN";
}

bool capture(const SoDrawList & source, const Envelope & envelope,
             const Limits & limits, OwnedFrame & output, Status & status) {
  status.code = Code::OK;
  status.detail.clear();
  try {
    Guard guard(limits);
    validateEnvelope(envelope, guard);
    require(source.getGeneration() == envelope.expectedGeneration &&
      source.getContentRevision() == envelope.expectedContentRevision,
      Code::STALE_FRAME, "producer generation/content revision differs from request");
    require(source.getNumCommands() >= 0 && std::size_t(source.getNumCommands()) <= limits.maxCommands,
      Code::COMMAND_LIMIT, "command count limit");
    require(source.getNumGeometryResources() >= 0 &&
      std::size_t(source.getNumGeometryResources()) <= limits.maxResources,
      Code::RESOURCE_LIMIT, "producer resource count limit");
    require(source.getDepthClearEvents().empty(), Code::UNSUPPORTED_STATE, "depth-clear barriers require staged execution");
    OwnedFrame working;
    working.envelope = envelope;
    working.generation = source.getGeneration();
    working.contentRevision = source.getContentRevision();
    guard.charge(std::size_t(source.getNumCommands()), sizeof(Command));
    working.commands.reserve(source.getNumCommands());
    std::map<std::uint32_t, std::size_t> resourceMap;
    for (int index = 0; index < source.getNumCommands(); ++index) {
      guard.poll();
      const SoRenderCommand & original = source.getCommand(index);
      validateState(original, envelope);
      const SoGeometryResource * producerResource = nullptr;
      if (original.geometryHandle != SO_INVALID_GEOMETRY_HANDLE) {
        producerResource = source.getGeometryResource(original.geometryHandle);
        require(producerResource != nullptr, Code::INVALID_GEOMETRY, "invalid geometry resource handle");
      }
      std::size_t resourceId;
      const auto existing = resourceMap.find(original.geometryHandle);
      if (original.geometryHandle != SO_INVALID_GEOMETRY_HANDLE && existing != resourceMap.end()) {
        resourceId = existing->second;
      }
      else {
        require(working.resources.size() < limits.maxResources, Code::RESOURCE_LIMIT, "owned resource count limit");
        resourceId = working.resources.size();
        working.resources.push_back(copyResource(source.getCommandGeometry(original), producerResource,
          original.geometryHandle, resourceId, guard));
        if (original.geometryHandle != SO_INVALID_GEOMETRY_HANDLE) resourceMap.emplace(original.geometryHandle, resourceId);
      }
      const Resource & resource = working.resources[resourceId];
      const std::size_t drawCount = resource.indices.empty() ? resource.positions.size() : resource.indices.size();
      require(drawCount / 3 <= limits.maxTriangles - working.triangleCount,
        Code::TRIANGLE_LIMIT, "instanced triangle count limit");
      working.triangleCount += drawCount / 3;
      Command command;
      command.commandIndex = index;
      command.resourceId = resourceId;
      command.nodeId = original.nodeId;
      command.instanceId = original.instanceId;
      command.objectId = original.objectId;
      command.model = guard.matrix(original.modelMatrix, true, "command_model", index, original.nodeId);
      // Inactive command camera fields need not be initialized by a manual
      // producer. Copy effective frame matrices unless the policy uses them.
      command.view = guard.matrix(original.state.useCommandMatrices ? original.viewMatrix : envelope.viewMatrix, false, "command_view", index, original.nodeId);
      command.projection = guard.matrix(original.state.useCommandMatrices ? original.projMatrix : envelope.projectionMatrix, false, "command_projection", index, original.nodeId);
      command.material = copyMaterial(original.material, guard);
      copyLighting(source, original, command, envelope, guard);
      require(original.materialIndex >= 0, Code::INVALID_MATERIAL, "negative material index");
      command.materialIndex = original.materialIndex;
      command.stage = int(original.stage);
      command.usesCommandMatrices = original.state.useCommandMatrices != FALSE;
      command.pickable = original.pick.pickable;
      command.hadProducerUserData = original.userData != nullptr;
      command.state = original.state;
      command.elementRanges = copyRanges(source.getCommandElementRanges(original), drawCount, guard);
      working.commands.push_back(std::move(command));
    }
    copySelection(source.getSelectionState(), working, guard);
    guard.poll();
    require(source.getGeneration() == working.generation && source.getContentRevision() == working.contentRevision,
      Code::STALE_FRAME, "producer changed during synchronous capture");
    working.bytesCopied = guard.bytes;
    output = std::move(working);
    return true;
  }
  catch (const Failure & error) { setFailure(status, error.code, error.detail); }
  catch (const std::bad_alloc &) { setFailure(status, Code::ALLOCATION_FAILURE, "owned snapshot allocation failure"); }
  catch (const std::length_error &) { setFailure(status, Code::ALLOCATION_FAILURE, "owned snapshot allocation length failure"); }
  catch (...) { setFailure(status, Code::CAPTURE_FAILURE, "unexpected snapshot capture exception"); }
  clearOutput(output);
  return false;
}

bool toLegacySnapshot(const OwnedFrame & source, const Limits & limits,
                      coin_raytrace::Snapshot & output, Status & status) {
  // The unchanged reference DTO leaves Camera math members uninitialized.
  // Initialize every camera member before moving the cleared value to output.
  coin_raytrace::Snapshot cleared;
  cleared.camera.volume.ortho(-1, 1, -1, 1, 0, 1);
  cleared.camera.viewing.makeIdentity();
  cleared.camera.projection.makeIdentity();
  cleared.camera.viewport = SbViewportRegion(1, 1);
  output = std::move(cleared);
  status = Status();
  try {
    Guard guard(limits);
    validateEnvelope(source.envelope, guard);
    require(source.commands.size() <= limits.maxCommands && source.resources.size() <= limits.maxResources &&
      source.triangleCount <= limits.maxTriangles && source.bytesCopied <= limits.maxBytes,
      Code::VALUE_LIMIT, "owned frame exceeds conversion bounds");
    coin_raytrace::Snapshot working;
    working.camera.volume = source.envelope.cameraVolume;
    working.camera.viewing = source.envelope.viewMatrix;
    working.camera.projection = source.envelope.projectionMatrix;
    working.camera.viewport = source.envelope.viewport;
    working.cameraValid = true;
    guard.charge(source.commands.size(), sizeof(coin_raytrace::Instance));
    guard.charge(source.commands.size(), sizeof(int)); // One command index per instance path.
    working.instances.reserve(source.commands.size());
    for (const Command & command : source.commands) {
      guard.poll();
      require(command.resourceId < source.resources.size(), Code::INVALID_GEOMETRY, "owned command resource reference");
      const Resource & resource = source.resources[command.resourceId];
      require(resource.positions.size() == resource.normals.size() &&
        (resource.colors.empty() || resource.colors.size() == resource.positions.size()),
        Code::INVALID_GEOMETRY, "owned attribute cardinality");
      const std::size_t drawCount = resource.indices.empty() ? resource.positions.size() : resource.indices.size();
      require(drawCount % 3 == 0 && drawCount / 3 <= limits.maxTriangles - working.triangleCount,
        Code::TRIANGLE_LIMIT, "owned conversion triangle bound");
      guard.charge(drawCount / 3, sizeof(coin_raytrace::Triangle));
      const SbMatrix model = matrixFromOwned(command.model, guard);
      const SbMatrix normalMatrix = model.inverse().transpose();
      coin_raytrace::Instance instance;
      instance.sourceShapeId = command.nodeId;
      instance.shapeRevision = source.contentRevision;
      instance.geometryId = resource.resourceId;
      instance.model = command.model;
      // This synthetic index is provenance for the diagnostic renderer only;
      // it is not a copied scene path or a persistent CAD selection reference.
      instance.path.push_back(command.commandIndex);
      instance.triangles.reserve(drawCount / 3);
      for (std::size_t start = 0; start < drawCount; start += 3) {
        guard.poll();
        coin_raytrace::Triangle triangle;
        for (int corner = 0; corner < 3; ++corner) {
          const std::size_t vertex = resource.indices.empty() ? start + corner : resource.indices[start + corner];
          require(vertex < resource.positions.size(), Code::INVALID_GEOMETRY, "owned triangle index");
          const Vec3 & position = resource.positions[vertex];
          const Vec3 & normal = resource.normals[vertex];
          for (int axis = 0; axis < 3; ++axis) {
            guard.number(position[axis]); guard.number(normal[axis]);
          }
          SbVec3f localPosition{static_cast<float>(position[0]), static_cast<float>(position[1]), static_cast<float>(position[2])};
          SbVec3f localNormal{static_cast<float>(normal[0]), static_cast<float>(normal[1]), static_cast<float>(normal[2])};
          require(localNormal.normalize() > 0, Code::INVALID_GEOMETRY, "owned zero normal");
          SbVec3f transformedPosition, transformedNormal;
          model.multVecMatrix(localPosition, transformedPosition);
          normalMatrix.multDirMatrix(localNormal, transformedNormal);
          require(transformedNormal.normalize() > 0, Code::INVALID_GEOMETRY, "zero transformed normal");
          triangle.positionsObject[corner] = position;
          triangle.positionsWorld[corner] = guard.vector(transformedPosition);
          triangle.normalsObject[corner] = guard.vector(localNormal);
          triangle.normalsWorld[corner] = guard.vector(transformedNormal);
          triangle.materialIndices[corner] = command.materialIndex;
          triangle.materials[corner] = legacyMaterial(command.material,
            resource.colors.empty() ? nullptr : &resource.colors[vertex]);
        }
        instance.triangles.push_back(std::move(triangle));
        ++working.triangleCount;
      }
      working.instances.push_back(std::move(instance));
    }
    require(working.triangleCount == source.triangleCount, Code::INVALID_METADATA, "owned triangle summary mismatch");
    guard.poll();
    output = std::move(working);
    return true;
  }
  catch (const Failure & error) { setFailure(status, error.code, error.detail); }
  catch (const std::bad_alloc &) { setFailure(status, Code::ALLOCATION_FAILURE, "diagnostic expansion allocation failure"); }
  catch (const std::length_error &) { setFailure(status, Code::ALLOCATION_FAILURE, "diagnostic expansion allocation length failure"); }
  catch (...) { setFailure(status, Code::CAPTURE_FAILURE, "unexpected diagnostic conversion exception"); }
  return false;
}

} // namespace retained_bridge
