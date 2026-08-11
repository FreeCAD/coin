#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShaderObject.h>
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoVertexShader.h>

#include "support/GLTestContext.h"
#include "support/GLTestUtils.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

using coin_test::check;
using coin_test::skip;

struct DiagnosticCapture {
  int warnings = 0;
  int infos = 0;
  std::vector<std::string> messages;
};

void capture_diagnostic(const SoError * error, void * data)
{
  DiagnosticCapture * capture = static_cast<DiagnosticCapture *>(data);
  if (!error->isOfType(SoDebugError::getClassTypeId())) return;

  const SoDebugError * debug = static_cast<const SoDebugError *>(error);
  if (debug->getSeverity() == SoDebugError::WARNING) ++capture->warnings;
  if (debug->getSeverity() == SoDebugError::INFO) ++capture->infos;
  capture->messages.push_back(debug->getDebugString().getString());
}

std::string messages_as_text(const DiagnosticCapture & capture)
{
  std::string result;
  for (std::vector<std::string>::const_iterator it = capture.messages.begin();
       it != capture.messages.end(); ++it) {
    if (!result.empty()) result += "\n";
    result += *it;
  }
  return result;
}

bool contains(const DiagnosticCapture & capture, const char * text)
{
  return messages_as_text(capture).find(text) != std::string::npos;
}

class ShaderFile {
public:
  ShaderFile(const char * path, const char * source)
    : path(path)
  {
    std::ofstream output(this->path.c_str(), std::ios::binary);
    if (!output) {
      this->path.clear();
      return;
    }
    output << source;
    if (!output) this->path.clear();
  }

  ~ShaderFile()
  {
    if (!this->path.empty()) std::remove(this->path.c_str());
  }

  bool valid() const { return !this->path.empty(); }

  std::string path;
};

void render_program(GLTestContext & context, SoShaderProgram * program)
{
  SoSeparator * root = new SoSeparator;
  root->ref();
  root->addChild(program);

  context.bindFramebuffer();
  SoGLRenderAction action(SbViewportRegion(16, 16));
  action.setCacheContext(context.contextId());
  action.apply(root);

  root->unref();
}

SoShaderProgram * make_program(SoShaderObject * first,
                               SoShaderObject * second = NULL)
{
  SoShaderProgram * program = new SoShaderProgram;
  program->shaderObject.set1Value(0, first);
  if (second != NULL) program->shaderObject.set1Value(1, second);
  return program;
}

} // namespace

int main()
{
  SoDB::init();

  GLTestContextConfig config;
  config.profile = GLTestProfile::Compatibility;
  config.major = 3;
  config.minor = 3;
  config.width = 16;
  config.height = 16;
  GLTestContext context;
  if (!context.initialize(config)) {
    SoDB::finish();
    return skip("compatibility OpenGL test context is unavailable");
  }
  if (!context.makeCurrent()) {
    SoDB::finish();
    return skip("compatibility OpenGL test context could not be made current");
  }

  SoErrorCB * previousCallback = SoDebugError::getHandlerCallback();
  void * previousData = SoDebugError::getHandlerData();
  int result = 1;

  do {
    ShaderFile brokenFile(
      "coin-glsl-broken.vert",
      "#version 330 core\n"
      "void main() { this is not valid GLSL; }\n");
    if (!check(brokenFile.valid(), "could not create broken shader file")) break;

    SoVertexShader * brokenShader = new SoVertexShader;
    brokenShader->sourceType = SoShaderObject::FILENAME;
    brokenShader->sourceProgram = brokenFile.path.c_str();

    DiagnosticCapture compileCapture;
    SoDebugError::setHandlerCallback(capture_diagnostic, &compileCapture);
    render_program(context, make_program(brokenShader));

    if (!check(compileCapture.warnings > 0,
               "shader compilation failure was not reported as a warning")) break;
    if (!check(contains(compileCapture, "vertex shader"),
               "shader diagnostic omitted its stage")) break;
    if (!check(contains(compileCapture, "-broken.vert"),
               "shader diagnostic omitted its source identity")) break;
    if (!check(contains(compileCapture, "failed to compile"),
               "shader diagnostic omitted its failure description")) break;

    ShaderFile linkVertexFile(
      "coin-glsl-link.vert",
      "#version 330 core\n"
      "out vec3 varying_color;\n"
      "void main() { varying_color = vec3(1.0); "
      "gl_Position = vec4(0.0); }\n");
    ShaderFile linkFragmentFile(
      "coin-glsl-link.frag",
      "#version 330 core\n"
      "in vec4 varying_color;\n"
      "out vec4 color;\n"
      "void main() { color = varying_color; }\n");
    if (!check(linkVertexFile.valid() && linkFragmentFile.valid(),
               "could not create shader link test files")) break;

    SoVertexShader * vertexShader = new SoVertexShader;
    vertexShader->sourceType = SoShaderObject::FILENAME;
    vertexShader->sourceProgram = linkVertexFile.path.c_str();
    SoFragmentShader * fragmentShader = new SoFragmentShader;
    fragmentShader->sourceType = SoShaderObject::FILENAME;
    fragmentShader->sourceProgram = linkFragmentFile.path.c_str();

    DiagnosticCapture linkCapture;
    SoDebugError::setHandlerCallback(capture_diagnostic, &linkCapture);
    render_program(context, make_program(vertexShader, fragmentShader));

    if (!check(linkCapture.warnings > 0,
               "program link failure was not reported as a warning")) break;
    if (!check(contains(linkCapture, "failed to link"),
               "program diagnostic omitted its failure description")) break;
    if (!check(contains(linkCapture, "vertex shader=") &&
               contains(linkCapture, "-link.vert"),
               "program diagnostic omitted the vertex source stage")) break;
    if (!check(contains(linkCapture, "fragment shader=") &&
               contains(linkCapture, "-link.frag"),
               "program diagnostic omitted the fragment source stage")) break;

    result = 0;
  } while (false);

  SoDebugError::setHandlerCallback(previousCallback, previousData);
  SoDB::finish();
  return result;
}
