#include <windows.h>
#include <GL/glcorearb.h>

#include <Inventor/C/glue/gl.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/misc/SoContextHandler.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>

#include <array>
#include <chrono>
#include <thread>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace {
constexpr int side = 32;
constexpr size_t pixels = side * side;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

void need(bool value, const char * message) {
  if (!value) throw std::runtime_error(message);
}

GLint integer(GLenum token) {
  GLint value = 0;
  glGetIntegerv(token, &value);
  return value;
}

std::vector<GLenum> errors() {
  std::vector<GLenum> observed;
  for (int i = 0; i < 64; ++i) {
    const GLenum value = glGetError();
    if (value == GL_NO_ERROR) break;
    observed.push_back(value);
  }
  return observed;
}

void jsonString(std::ostream & out, std::string_view value) {
  constexpr char hex[] = "0123456789abcdef";
  out << '"';
  for (const unsigned char ch : value) {
    if (ch == '\\' || ch == '"') out << '\\' << char(ch);
    else if (ch < 0x20) out << "\\u00" << hex[ch >> 4] << hex[ch & 15];
    else out << char(ch);
  }
  out << '"';
}

void jsonErrors(std::ostream & out, const std::vector<GLenum> & values) {
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  out << ']';
}

std::string glString(GLenum token) {
  const auto * value = glGetString(token);
  need(value != nullptr, "GL identity unavailable");
  return reinterpret_cast<const char *>(value);
}

// Mirrors the recovery probe's private hidden WGL ownership and partial-init cleanup.
struct Window {
  HWND window = nullptr;
  HDC dc = nullptr;
  HGLRC context = nullptr;
  const HGLRC previous = wglGetCurrentContext();
  const HDC previousDc = wglGetCurrentDC();
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  static constexpr wchar_t name[] = L"CoinDepthStateProbe";
  bool registered = false;
  bool restored = false, deleted = false, released = false, gone = false;
  bool unregistered = false;

  Window() {
    try {
      WNDCLASSW wc{};
      wc.style = CS_OWNDC;
      wc.lpfnWndProc = DefWindowProcW;
      wc.hInstance = instance;
      wc.lpszClassName = name;
      need(RegisterClassW(&wc) != 0, "register owned window class");
      registered = true;
      window = CreateWindowExW(0, name, L"Coin depth state probe", WS_OVERLAPPEDWINDOW,
                               0, 0, 64, 64, nullptr, nullptr, instance, nullptr);
      need(window != nullptr, "create owned hidden window");
      dc = GetDC(window);
      need(dc != nullptr, "acquire owned DC");
      PIXELFORMATDESCRIPTOR p{};
      p.nSize = sizeof(p);
      p.nVersion = 1;
      p.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
      p.iPixelType = PFD_TYPE_RGBA;
      p.cColorBits = 32;
      p.cDepthBits = 24;
      p.cAlphaBits = 8;
      const int format = ChoosePixelFormat(dc, &p);
      need(format && SetPixelFormat(dc, format, &p), "set owned pixel format");
      context = wglCreateContext(dc);
      need(context && wglMakeCurrent(dc, context), "activate owned WGL context");
    }
    catch (...) { close(); throw; }
  }
  Window(const Window &) = delete;
  Window & operator=(const Window &) = delete;
  void close() {
    if (context) {
      restored = wglMakeCurrent(previousDc, previous) != FALSE &&
        wglGetCurrentContext() == previous && wglGetCurrentDC() == previousDc;
      deleted = wglDeleteContext(context) != FALSE;
      context = nullptr;
    }
    if (dc) { released = ReleaseDC(window, dc) != 0; dc = nullptr; }
    if (window) {
      const HWND owned = window;
      const BOOL destroyed = DestroyWindow(owned);
      gone = destroyed && !IsWindow(owned);
      window = nullptr;
    }
    if (registered) {
      unregistered = UnregisterClassW(name, instance) != FALSE;
      registered = false;
    }
  }
  ~Window() { close(); }
};

struct CoinContext {
  uint32_t cache;
  const cc_glglue * glue;
  CoinContext() {
    SoDB::init();
    cache = SoGLCacheContextElement::getUniqueCacheContext();
    glue = cc_glglue_instance(cache);
  }
  ~CoinContext() {
    SoContextHandler::destructingContext(cache);
    SoDB::finish();
  }
};

struct Target {
  const cc_glglue * glue;
  GLuint fbo = 0, color = 0, depth = 0;

  explicit Target(const cc_glglue * g) : glue(g) {
    need(glue && cc_glglue_has_framebuffer_objects(glue), "owned FBO API unavailable");
    try {
      cc_glglue_glGenFramebuffers(glue, 1, &fbo);
      need(fbo != 0, "allocate owned framebuffer");
      bind();
      glGenTextures(1, &color);
      glBindTexture(GL_TEXTURE_2D, color);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, side, side, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      cc_glglue_glFramebufferTexture2D(glue, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, color, 0);
      glGenTextures(1, &depth);
      glBindTexture(GL_TEXTURE_2D, depth);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, side, side, 0,
                   GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      cc_glglue_glFramebufferTexture2D(glue, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                     GL_TEXTURE_2D, depth, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
      need(cc_glglue_glCheckFramebufferStatus(glue, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
           "owned FBO is incomplete");
      glDisable(GL_DITHER);
      glDisable(GL_FRAMEBUFFER_SRGB);
      need(errors().empty(), "owned FBO setup GL error");
    }
    catch (...) { close(); throw; }
  }
  void bind() const {
    cc_glglue_glBindFramebuffer(glue, GL_FRAMEBUFFER, fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, side, side);
  }
  void reset(bool preserved) const {
    bind();
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (preserved) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(0, 0, side / 2, side);
      glClearColor(0, 0, 1, 1);
      glClearDepth(.25);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glDisable(GL_SCISSOR_TEST);
    }
  }
  void close() {
    cc_glglue_glBindFramebuffer(glue, GL_FRAMEBUFFER, 0);
    if (fbo) cc_glglue_glDeleteFramebuffers(glue, 1, &fbo);
    if (color) glDeleteTextures(1, &color);
    if (depth) glDeleteTextures(1, &depth);
  }
  ~Target() { close(); }
};

// Read the actual target independently of Coin's diagnostic readback. The
// oracle neutralizes PACK and records its value during the driver call.
struct PixelPack {
  static constexpr std::array<GLenum, 5> keys{
    GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS,
    GL_PACK_SWAP_BYTES
  };
  std::array<GLint, keys.size()> saved{};
  const cc_glglue * glue;
  const GLint pbo;
  explicit PixelPack(const cc_glglue * g)
    : glue(g), pbo(integer(GL_PIXEL_PACK_BUFFER_BINDING)) {
    cc_glglue_glBindBuffer(glue, GL_PIXEL_PACK_BUFFER, 0);
    for (size_t i = 0; i < keys.size(); ++i) {
      saved[i] = integer(keys[i]);
      glPixelStorei(keys[i], i == 0 ? 1 : 0);
    }
  }
  ~PixelPack() {
    for (size_t i = 0; i < keys.size(); ++i) glPixelStorei(keys[i], saved[i]);
    cc_glglue_glBindBuffer(glue, GL_PIXEL_PACK_BUFFER, GLuint(pbo));
  }
};

template<class T>
void writeBuffer(const fs::path & path, const std::vector<T> & data, size_t count) {
  need(data.size() == count, "wrong frame array size");
  std::ofstream out(path, std::ios::binary);
  need(bool(out), "open new frame array");
  out.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size() * sizeof(T)));
  out.close();
  need(bool(out), "close frame array");
}

bool readTarget(const Target & target, const fs::path & prefix) {
  need(integer(GL_DRAW_FRAMEBUFFER_BINDING) == GLint(target.fbo) &&
       integer(GL_READ_FRAMEBUFFER_BINDING) == GLint(target.fbo) &&
       integer(GL_READ_BUFFER) == GL_COLOR_ATTACHMENT0, "unexpected framebuffer or read target");
  std::vector<unsigned char> rgb(pixels * 3);
  std::vector<float> depth(pixels);
  bool swap = true;
  {
    PixelPack state(target.glue);
    swap = integer(GL_PACK_SWAP_BYTES) != GL_FALSE;
    glReadPixels(0, 0, side, side, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    glReadPixels(0, 0, side, side, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
  }
  need(errors().empty(), "independent target readback GL error");
  writeBuffer(prefix.wstring() + L".rgb", rgb, pixels * 3);
  writeBuffer(prefix.wstring() + L".depth.f32", depth, pixels);
  return swap;
}

void addCube(SoSeparator * root, float x, float z, float size, float red, float green) {
  auto * part = new SoSeparator;
  root->addChild(part);
  auto * transform = new SoTranslation;
  transform->translation.setValue(x, 0, z);
  part->addChild(transform);
  auto * material = new SoMaterial;
  material->diffuseColor.setValue(red, green, 0);
  material->emissiveColor.setValue(red, green, 0);
  material->ambientColor.setValue(0, 0, 0);
  material->specularColor.setValue(0, 0, 0);
  part->addChild(material);
  auto * cube = new SoCube;
  cube->width = size;
  cube->height = size;
  cube->depth = 1;
  part->addChild(cube);
}

struct Scene {
  struct RootRef {
    SoSeparator * node = new SoSeparator;
    RootRef() { node->ref(); }
    ~RootRef() { node->unref(); }
  } root;
  SoRenderManager manager;

  Scene(uint32_t cache, bool overlay) {
    auto * camera = new SoOrthographicCamera;
    camera->position.setValue(0, 0, 5);
    camera->height = 4;
    camera->nearDistance = 1;
    camera->farDistance = 9;
    camera->viewportMapping = SoCamera::LEAVE_ALONE;
    root.node->addChild(camera);
    auto * light = new SoLightModel;
    light->model = SoLightModel::BASE_COLOR;
    root.node->addChild(light);
    if (overlay) {
      addCube(root.node, -.5f, -1.5f, .75f, 0, 1);
      addCube(root.node, .5f, .5f, .75f, 0, 1);
    }
    else addCube(root.node, 0, -.5f, 2, 1, 0);
    manager.setViewportRegion(SbViewportRegion(side, side));
    manager.setCamera(camera);
    manager.setCameraInSceneGraph(TRUE);
    manager.setAutoClipping(SoRenderManager::NO_AUTO_CLIPPING);
    manager.setSceneGraph(root.node);
    manager.setBackgroundColor(SbColor4f(0, 0, 0, 1));
    manager.getGLRenderAction()->setCacheContext(cache);
    manager.setRenderPipeline(SoRenderManager::RenderPipeline::DRAW_LIST);
    manager.setCpuRayTracingReadback(overlay ? FALSE : TRUE);
  }
  ~Scene() {
    manager.setSceneGraph(nullptr);
    manager.releaseRenderBackendResources();
    manager.setCamera(nullptr);
  }
};

struct Case { const char * name; bool cpu; bool swap; };

void observe(std::ostream & out, const fs::path & dir, const Target & target,
             Scene & scene, Scene & overlay, std::wstring_view mode, Case row) {
  const bool preserved = mode == L"preserved";
  target.reset(preserved);
  if (preserved) readTarget(target, dir / (std::string(row.name) + ".prefill"));
  scene.manager.setCpuRayTracingEnabled(row.cpu ? TRUE : FALSE);
  glPixelStorei(GL_PACK_SWAP_BYTES, row.swap && mode == L"pack" ? GL_TRUE : GL_FALSE);
  glPixelStorei(GL_UNPACK_SWAP_BYTES, row.swap && mode == L"unpack" ? GL_TRUE : GL_FALSE);
  const bool packBefore = integer(GL_PACK_SWAP_BYTES) != GL_FALSE;
  const bool unpackBefore = integer(GL_UNPACK_SWAP_BYTES) != GL_FALSE;
  need(errors().empty(), "case setup GL error");
  scene.manager.render(!preserved, !preserved);
  const bool packAfter = integer(GL_PACK_SWAP_BYTES) != GL_FALSE;
  const bool unpackAfter = integer(GL_UNPACK_SWAP_BYTES) != GL_FALSE;
  auto observedErrors = errors();
  const auto & cpu = scene.manager.getLastCpuRayTracingResult();
  const auto & route = scene.manager.getLastRenderResult();
  const std::string name(row.name);
  const bool oraclePackSwap = readTarget(target, dir / (name + ".actual"));
  if (cpu.status == SoCpuRayTracingResult::RENDERED) {
    writeBuffer(dir / (name + ".owned.rgb"), cpu.rgb, pixels * 3);
    writeBuffer(dir / (name + ".owned.depth.f32"), cpu.depth, pixels);
    writeBuffer(dir / (name + ".owned.coverage.u8"), cpu.coverage, pixels);
  }
  if (cpu.readbackValid) {
    writeBuffer(dir / (name + ".reported.rgb"), cpu.readbackRgb, pixels * 3);
    writeBuffer(dir / (name + ".reported.depth.f32"), cpu.readbackDepth, pixels);
  }
  if (!preserved) {
    overlay.manager.render(FALSE, FALSE);
    const auto overlayErrors = errors();
    observedErrors.insert(observedErrors.end(), overlayErrors.begin(), overlayErrors.end());
    readTarget(target, dir / (name + ".overlay"));
  }
  out << std::boolalpha << "{\"case\":";
  jsonString(out, row.name);
  out << ",\"cpu_requested\":" << row.cpu << ",\"clear_depth\":" << !preserved
      << ",\"clear_color\":" << !preserved << ",\"cpu_status\":" << int(cpu.status)
      << ",\"cpu_detail\":";
  jsonString(out, cpu.detail.getString());
  out << ",\"frame_serial\":" << cpu.frameSerial << ",\"cpu_framebuffer\":" << cpu.framebuffer
      << ",\"cpu_width\":" << cpu.width << ",\"cpu_height\":" << cpu.height
      << ",\"triangle_count\":" << cpu.triangleCount << ",\"hit_count\":" << cpu.hitCount
      << ",\"readback_valid\":" << cpu.readbackValid
      << ",\"requested_pipeline\":" << int(route.requestedPipeline)
      << ",\"used_pipeline\":" << int(route.usedPipeline)
      << ",\"fallback_reason\":" << int(route.fallbackReason)
      << ",\"rendered\":" << (route.rendered != FALSE)
      << ",\"pack_before\":" << packBefore << ",\"pack_after\":" << packAfter
      << ",\"unpack_before\":" << unpackBefore << ",\"unpack_after\":" << unpackAfter
      << ",\"oracle_pack_swap\":" << oraclePackSwap << ",\"gl_errors\":";
  jsonErrors(out, observedErrors);
  if (!preserved) {
    const auto & control = overlay.manager.getLastRenderResult();
    out << ",\"overlay_used_pipeline\":" << int(control.usedPipeline)
        << ",\"overlay_fallback_reason\":" << int(control.fallbackReason)
        << ",\"overlay_rendered\":" << (control.rendered != FALSE);
  }
  out << '}';
  out.flush();
  need(bool(out), "write frame observation");
}
// Two interior pixels exchange foreground/background after a one-unit camera move.
void checkReplacement(Scene & scene, const Target & target) {
  auto & manager = scene.manager;
  manager.setCpuRayTracingEnabled(TRUE);
  for (int pose = 0; pose != 2; ++pose) {
    if (pose) manager.getCamera()->position.setValue(1, 0, 5);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool fallback = false;
    while (true) {
      target.reset(false);
      manager.render(TRUE, TRUE);
      const auto & frame = manager.getLastCpuRayTracingResult();
      if (frame.status == SoCpuRayTracingResult::RENDERED) {
        need(fallback, "expected native fallback before publication");
        need(frame.readbackValid && frame.readbackDepth.size() == pixels, "missing presented depth");
        const bool left = frame.readbackDepth[16 * side + 6] < .99f;
        const bool right = frame.readbackDepth[16 * side + 18] < .99f;
        need(left == bool(pose) && right != bool(pose), "stale or incorrect presented camera image");
        break;
      }
      need(frame.status == SoCpuRayTracingResult::FALLBACK, "unexpected preview status");
      fallback = true;
      need(std::chrono::steady_clock::now() < deadline, "replacement timeout");
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
}
} // namespace

int wmain(int argc, wchar_t ** argv) {
  try {
    need(argc == 3, "usage: depth_state_probe unpack|pack|preserved NEW_OUTPUT_DIRECTORY");
    const std::wstring_view mode = argv[1];
    need(mode == L"unpack" || mode == L"pack" || mode == L"preserved" || mode == L"replacement", "unknown mode");
    const fs::path dir = fs::absolute(argv[2]);
    need(fs::create_directory(dir), "exclusive output directory");
    std::ofstream out(dir / "observations.json", std::ios::binary);
    need(bool(out), "open observations");
    wchar_t module[MAX_PATH * 4]{};
    const HMODULE coin = GetModuleHandleW(L"Coin4.dll");
    need(coin != nullptr, "loaded Coin4 module missing");
    const DWORD length = GetModuleFileNameW(coin, module, DWORD(std::size(module)));
    need(length && length < std::size(module), "loaded Coin4 module path unavailable");
    Window window;
    {
      CoinContext coinContext;
      Target target(coinContext.glue);
      Scene scene(coinContext.cache, false);
      Scene overlay(coinContext.cache, true);
      out << "{\"schema\":\"coin.depth-state.observations.v1\",\"mode\":";
      jsonString(out, fs::path(mode).u8string());
      out << ",\"width\":" << side << ",\"height\":" << side
          << ",\"framebuffer\":" << target.fbo << ",\"depth_bits\":" << integer(GL_DEPTH_BITS)
          << ",\"samples\":" << integer(GL_SAMPLES) << ",\"loaded_coin_module\":";
      jsonString(out, fs::path(module).u8string());
      out << ",\"gl_vendor\":";
      jsonString(out, glString(GL_VENDOR));
      out << ",\"gl_renderer\":";
      jsonString(out, glString(GL_RENDERER));
      out << ",\"gl_version\":";
      jsonString(out, glString(GL_VERSION));
      out << ",\"frames\":[";
      if (mode == L"replacement") checkReplacement(scene, target);
      else if (mode == L"preserved") {
        observe(out, dir, target, scene, overlay, mode, {"gl_prefilled", false, false});
        out << ',';
        observe(out, dir, target, scene, overlay, mode, {"cpu_prefilled", true, false});
      }
      else {
        const Case cases[] = {{"gl_default", false, false}, {"gl_swap", false, true},
                             {"cpu_default", true, false}, {"cpu_swap", true, true}};
        for (size_t i = 0; i < std::size(cases); ++i) {
          if (i) out << ',';
          observe(out, dir, target, scene, overlay, mode, cases[i]);
        }
      }
    }
    const auto cleanupErrors = errors();
    window.close();
    out << std::boolalpha << "],\"cleanup\":{\"restored_context\":" << window.restored
        << ",\"context_deleted\":" << window.deleted << ",\"dc_released\":" << window.released
        << ",\"window_gone\":" << window.gone << ",\"class_unregistered\":" << window.unregistered
        << ",\"gl_errors\":";
    jsonErrors(out, cleanupErrors);
    out << "}}\n";
    out.close();
    need(bool(out), "close observations");
    need(window.restored && window.deleted && window.released && window.gone &&
         window.unregistered && cleanupErrors.empty(), "owned context cleanup incomplete");
    std::cout << "Captured native depth-state observations\n";
    return 0;
  }
  catch (const std::exception & error) {
    std::cerr << error.what() << '\n';
    return 2;
  }
}
