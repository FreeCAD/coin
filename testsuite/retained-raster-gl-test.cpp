#include "rendering/CoinOffscreenGLCanvas.h"
#include "rendering/SoGLRenderBackend.h"

#include <Inventor/SoDB.h>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int skip(const char * reason)
{
  std::cout << "SKIP: " << reason << std::endl;
  return 77;
}

void setEnvironment(const char * name, const char * value)
{
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

struct Fixture {
  CoinOffscreenGLCanvas canvas;
  SoGLRenderBackend backend;

  bool initialize()
  {
    canvas.setWantedSize(SbVec2s(64, 64));
    if (canvas.activateGLContext() == 0) return false;
    SoRenderBackendInitParams init = {};
    if (backend.initialize(init)) return true;
    canvas.deactivateGLContext();
    return false;
  }

  std::vector<uint8_t> render(SoDrawList & drawlist,
                              const SbVec4f & clearColor,
                              float dpr = 1.0f,
                              const SbVec2s & viewportOrigin = SbVec2s(0, 0),
                              const SbVec2s & viewportSize = SbVec2s(64, 64))
  {
    SoRenderParams params = {};
    params.viewport = SbViewportRegion(64, 64);
    params.viewport.setViewportPixels(viewportOrigin, viewportSize);
    params.viewMatrix.makeIdentity();
    params.projMatrix.makeIdentity();
    params.devicePixelRatio = dpr;
    params.clearColor = clearColor;
    params.clearDepth = 1.0f;
    params.flags = SO_PARAM_CLEAR_WINDOW | SO_PARAM_CLEAR_DEPTH;
    backend.render(drawlist, params);
    glFinish();
    std::vector<uint8_t> pixels(64 * 64 * 4, 0);
    canvas.readPixels(pixels.data(), SbVec2s(64, 64), 64, 4);
    return pixels;
  }

  void shutdown()
  {
    backend.shutdown();
    canvas.deactivateGLContext();
  }
};

const uint8_t * pixelAt(const std::vector<uint8_t> & pixels, int x, int y)
{
  return &pixels[static_cast<size_t>(y * 64 + x) * 4];
}

bool check(bool condition, const char * message)
{
  if (!condition) std::cerr << "FAIL: " << message << std::endl;
  return condition;
}

SoRenderCommand coloredCommand(SoPrimitiveTopology topology,
                               const float * positions,
                               uint32_t vertexCount,
                               const SbVec4f & color)
{
  SoRenderCommand command;
  command.modelMatrix.makeIdentity();
  command.geometry.topology = topology;
  command.geometry.positions = positions;
  command.geometry.vertexCount = vertexCount;
  command.geometry.vertexStride = sizeof(float) * 3;
  command.material.diffuse = color;
  command.material.shadingModel = SO_SHADING_UNLIT;
  return command;
}

bool testWideLine(Fixture & fixture)
{
  const float positions[] = { -0.8f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f };
  SoDrawList drawlist;
  SoRenderCommand command = coloredCommand(SO_TOPOLOGY_LINES, positions, 2,
                                           SbVec4f(1, 0, 0, 1));
  command.state.raster.lineWidth = 4.0f;
  drawlist.addCommand(command);
  const std::vector<uint8_t> pixels = fixture.render(drawlist,
                                                      SbVec4f(0, 0, 0, 1), 2.0f);
  const uint8_t * center = pixelAt(pixels, 32, 32);
  const uint8_t * edge = pixelAt(pixels, 32, 35);
  return check(center[0] > 200 && center[1] < 50 &&
               edge[0] > 150 && edge[1] < 80,
               "wide-line geometry shader did not apply DPR-scaled width");
}

bool testPointSize(Fixture & fixture)
{
  const float position[] = { 0.0f, 0.0f, 0.0f };
  SoDrawList drawlist;
  SoRenderCommand command = coloredCommand(SO_TOPOLOGY_POINTS, position, 1,
                                           SbVec4f(0, 1, 0, 1));
  command.state.raster.pointSize = 12.0f;
  drawlist.addCommand(command);
  const std::vector<uint8_t> pixels = fixture.render(drawlist,
                                                      SbVec4f(0, 0, 0, 1));
  const uint8_t * center = pixelAt(pixels, 32, 32);
  return check(center[1] > 200 && center[0] < 50,
               "point-size pipeline did not render the point");
}

bool testFullLinePattern(Fixture & fixture)
{
  const float positions[] = { -0.8f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f };
  SoDrawList drawlist;
  SoRenderCommand command = coloredCommand(SO_TOPOLOGY_LINES, positions, 2,
                                           SbVec4f(1, 0, 0, 1));
  command.state.raster.lineWidth = 2.0f;
  command.state.raster.linePattern = 0x0001;
  command.state.raster.linePatternScale = 4;
  drawlist.addCommand(command);
  const std::vector<uint8_t> pixels = fixture.render(drawlist,
                                                      SbVec4f(0, 0, 0, 1));
  int redPixels = 0;
  for (int x = 8; x < 56; ++x) {
    const uint8_t * pixel = pixelAt(pixels, x, 32);
    if (pixel[0] > 180 && pixel[1] < 60) ++redPixels;
  }
  return check(redPixels > 0 && redPixels < 24,
               "wide-line shader did not apply the complete 16-bit pattern");
}

bool testEmptyLinePattern(Fixture & fixture)
{
  const float positions[] = { -0.8f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f };
  SoDrawList drawlist;
  SoRenderCommand command = coloredCommand(SO_TOPOLOGY_LINES, positions, 2,
                                           SbVec4f(1, 0, 0, 1));
  command.state.raster.lineWidth = 4.0f;
  command.state.raster.linePattern = 0x0000;
  drawlist.addCommand(command);
  const std::vector<uint8_t> pixels = fixture.render(drawlist,
                                                      SbVec4f(0, 0, 0, 1));
  const uint8_t * center = pixelAt(pixels, 32, 32);
  return check(center[0] < 20 && center[1] < 20 && center[2] < 20,
               "zero line pattern did not discard the complete line");
}

bool testTriangleFallbacks(Fixture & fixture)
{
  const float positions[] = {
    0.0f, 0.65f, 0.0f,
    -0.65f, -0.65f, 0.0f,
    0.65f, -0.65f, 0.0f
  };
  SoDrawList drawlist;
  SoRenderCommand line = coloredCommand(SO_TOPOLOGY_TRIANGLES, positions, 3,
                                        SbVec4f(1, 0, 0, 1));
  line.state.raster.fillMode = 1;
  line.state.raster.lineWidth = 4.0f;
  drawlist.addCommand(line);
  SoRenderCommand point = line;
  point.state.raster.fillMode = 2;
  point.state.raster.pointSize = 12.0f;
  drawlist.addCommand(point);
  const std::vector<uint8_t> pixels = fixture.render(drawlist,
                                                      SbVec4f(0, 0, 0, 1));
  const uint8_t * top = pixelAt(pixels, 32, 52);
  const uint8_t * center = pixelAt(pixels, 32, 11);
  return check((top[0] > 150 && top[1] < 80) &&
               (center[0] > 150 && center[1] < 80),
               "triangle line/point fallbacks did not emit raster geometry");
}

int countRedPixels(const std::vector<uint8_t> & pixels)
{
  int count = 0;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      const uint8_t * pixel = pixelAt(pixels, x, y);
      if (pixel[0] > 150 && pixel[1] < 80 && pixel[2] < 80) ++count;
    }
  }
  return count;
}

int countGreenPixels(const std::vector<uint8_t> & pixels)
{
  int count = 0;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      const uint8_t * pixel = pixelAt(pixels, x, y);
      if (pixel[1] > 150 && pixel[0] < 80 && pixel[2] < 80) ++count;
    }
  }
  return count;
}

bool testPatternedTriangleFallback(Fixture & fixture)
{
  const float positions[] = {
    0.0f, 0.65f, 0.0f,
    -0.65f, -0.65f, 0.0f,
    0.65f, -0.65f, 0.0f
  };
  auto renderPattern = [&](uint16_t pattern) {
    SoDrawList drawlist;
    SoRenderCommand command = coloredCommand(
      SO_TOPOLOGY_TRIANGLES, positions, 3, SbVec4f(1, 0, 0, 1));
    command.state.raster.fillMode = 1;
    command.state.raster.lineWidth = 4.0f;
    command.state.raster.linePattern = pattern;
    command.state.raster.linePatternScale = 4;
    drawlist.addCommand(command);
    return fixture.render(drawlist, SbVec4f(0, 0, 0, 1));
  };

  const int patterned = countRedPixels(renderPattern(0x0001));
  const int solid = countRedPixels(renderPattern(0xFFFF));
  return check(patterned > 0 && patterned < solid / 2,
               "triangle wireframe fallback did not vary stipple along edges");
}

bool testTriangleFallbackCulling(Fixture & fixture)
{
  const float frontPositions[] = {
    -0.85f, -0.55f, 0.0f,
    -0.25f, -0.55f, 0.0f,
    -0.55f,  0.55f, 0.0f
  };
  const float backPositions[] = {
     0.85f, -0.55f, 0.0f,
     0.25f, -0.55f, 0.0f,
     0.55f,  0.55f, 0.0f
  };

  auto render = [&](bool frontFaceCCW, bool points) {
    SoDrawList drawlist;
    SoRenderCommand front = coloredCommand(
      SO_TOPOLOGY_TRIANGLES, frontPositions, 3, SbVec4f(1, 0, 0, 1));
    SoRenderCommand back = coloredCommand(
      SO_TOPOLOGY_TRIANGLES, backPositions, 3, SbVec4f(0, 1, 0, 1));
    for (SoRenderCommand * command : {&front, &back}) {
      command->state.raster.cullMode = TRUE;
      command->state.raster.frontFaceCCW = frontFaceCCW ? TRUE : FALSE;
      if (points) {
        command->state.raster.fillMode = 2;
        command->state.raster.pointSize = 512.0f;
      }
      else {
        command->state.raster.fillMode = 1;
        command->state.raster.lineWidth = 4.0f;
        // A nearly solid pattern forces the triangle line fallback without
        // requiring a test-specific assumption about the native line range.
        command->state.raster.linePattern = 0xFFFE;
      }
    }
    drawlist.addCommand(front);
    drawlist.addCommand(back);
    return fixture.render(drawlist, SbVec4f(0, 0, 0, 1));
  };

  const std::vector<uint8_t> ccwLines = render(true, false);
  if (!check(countRedPixels(ccwLines) > 0 && countGreenPixels(ccwLines) == 0,
             "triangle line fallback did not cull a back-facing source triangle")) {
    return false;
  }
  const std::vector<uint8_t> cwLines = render(false, false);
  if (!check(countRedPixels(cwLines) == 0 && countGreenPixels(cwLines) > 0,
             "triangle line fallback did not honor clockwise front faces")) {
    return false;
  }

  const std::vector<uint8_t> ccwPoints = render(true, true);
  if (!check(countRedPixels(ccwPoints) > 0 && countGreenPixels(ccwPoints) == 0,
             "triangle point fallback did not cull a back-facing source triangle")) {
    return false;
  }
  const std::vector<uint8_t> cwPoints = render(false, true);
  return check(countRedPixels(cwPoints) == 0 && countGreenPixels(cwPoints) > 0,
               "triangle point fallback did not honor clockwise front faces");
}

bool testPixelDraw(Fixture & fixture)
{
  const float positions[] = {
    -0.1f, -0.1f, 0.0f,  0.1f, -0.1f, 0.0f,
     0.1f,  0.1f, 0.0f, -0.1f,  0.1f, 0.0f
  };
  const float texcoords[] = {
    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f
  };
  const uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };
  const unsigned char image[] = {
    255, 0, 0, 255, 255, 0, 0, 255,
    255, 0, 0, 255, 255, 0, 0, 255
  };
  SoRenderCommand command;
  command.modelMatrix.makeIdentity();
  command.geometry.topology = SO_TOPOLOGY_TRIANGLES;
  command.geometry.vertexCount = 4;
  command.geometry.indexCount = 6;
  command.geometry.positions = positions;
  command.geometry.indices = indices;
  command.geometry.vertexStride = sizeof(float) * 3;
  command.geometry.texcoords = texcoords;
  command.geometry.texcoordStride = sizeof(float) * 4;
  command.material.flags = SO_MAT_HAS_TEXTURE;
  command.material.texture.pixels = image;
  command.material.texture.width = 2;
  command.material.texture.height = 2;
  command.material.texture.numComponents = 4;
  command.pixelRaster.kind = SO_PIXEL_RASTER_IMAGE;
  command.pixelRaster.originX = 20;
  command.pixelRaster.originY = 20;
  command.material.shadingModel = SO_SHADING_UNLIT;
  SoDrawList drawlist;
  drawlist.addCommand(command);
  const std::vector<uint8_t> pixels = fixture.render(
    drawlist, SbVec4f(0, 0, 1, 1), 1.0f, SbVec2s(8, 8), SbVec2s(48, 48));
  const uint8_t * pixel = pixelAt(pixels, 28, 28);
  return check(pixel[0] > 200 && pixel[1] < 50 && pixel[2] < 50,
               "pixel pipeline did not sample the retained image at its origin");
}

} // namespace

int main()
{
  setEnvironment("COIN_EGL", "1");
  setEnvironment("EGL_PLATFORM", "surfaceless");
  setEnvironment("COIN_EGL_CORE_PROFILE", "1");
  SoDB::init();
  Fixture fixture;
  if (!fixture.initialize()) {
    SoDB::finish();
    return skip("core EGL raster context is unavailable");
  }

  int result = 0;
  if (!testWideLine(fixture)) result = 1;
  if (!testPointSize(fixture)) result = 1;
  if (!testFullLinePattern(fixture)) result = 1;
  if (!testEmptyLinePattern(fixture)) result = 1;
  if (!testTriangleFallbacks(fixture)) result = 1;
  if (!testPatternedTriangleFallback(fixture)) result = 1;
  if (!testTriangleFallbackCulling(fixture)) result = 1;
  if (!testPixelDraw(fixture)) result = 1;
  fixture.shutdown();
  SoDB::finish();
  return result;
}
