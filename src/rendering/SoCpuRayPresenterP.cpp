#include "rendering/SoCpuRayPresenterP.h"
#include "rendering/SoRayTraceRendererP.h"
#include <Inventor/C/glue/gl.h>
#include "glue/glp.h"
#include "glue/glslp.h"
#include <cmath>
#include <cstdint>
#include <sstream>

namespace {
const char * vertexSource = R"GLSL(#version 330 core
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";
const char * fragmentSource = R"GLSL(#version 330 core
uniform sampler2D cpuColor;
uniform sampler2D cpuDepth;
uniform usampler2D cpuCoverage;
uniform ivec2 viewportOrigin;
layout(location=0) out vec4 outputColor;
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy) - viewportOrigin;
  ivec2 size = textureSize(cpuCoverage, 0);
  if (any(lessThan(p, ivec2(0))) || any(greaterThanEqual(p, size))) discard;
  if (texelFetch(cpuCoverage, p, 0).r == 0u) discard;
  outputColor = vec4(texelFetch(cpuColor, p, 0).rgb, 1.0);
  gl_FragDepth = texelFetch(cpuDepth, p, 0).r;
}
)GLSL";
void errorDetail(std::string & detail, const char * operation, GLenum error) {
  std::ostringstream text; text << operation << " (GL 0x" << std::hex << error << ')';
  detail = text.str();
}
struct ShaderName {
  const cc_glglue * glue;
  GLuint name;
  ~ShaderName(){if(name)cc_glglue_glDeleteShader(glue,name);}
  GLuint release(){const GLuint result=name;name=0;return result;}
};
GLuint shader(const cc_glglue * glue, GLenum type, const char * source, std::string & detail) {
  ShaderName owner{glue,cc_glglue_glCreateShader(glue,type)};
  const GLuint name=owner.name;
  if (!name) { detail = "CPU presentation shader allocation failed"; return 0; }
  cc_glglue_glShaderSource(glue, name, 1, &source, nullptr);
  cc_glglue_glCompileShader(glue, name);
  GLint ok = 0; cc_glglue_glGetShaderiv(glue, name, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048] = {}; GLsizei written = 0;
    cc_glglue_glGetShaderInfoLog(glue, name, 2047, &written, log);
    detail = std::string("CPU presentation shader compile: ") + log;
    return 0;
  }
  return owner.release();
}
struct State {
  const cc_glglue * g;
  GLint program=0, vao=0, active=GL_TEXTURE0, unpack=0, alignment=4;
  GLint rowLength=0, imageHeight=0, skipRows=0, skipPixels=0, skipImages=0;
  GLint swapBytes=GL_FALSE;
  GLint textures[3]={}, samplers[3]={}, viewport[4]={}, depthFunction=GL_LESS;
  GLint polygon[2]={GL_FILL,GL_FILL}, drawFbo=0, readFbo=0;
  GLboolean depth=0, blend=0, cull=0, scissor=0, srgb=0, offset=0, stencil=0;
  GLboolean dither=0, rasterDiscard=0, alpha=0;
  bool compatibility=false;
  GLboolean mask[4]={}, depthWrite=0;
  GLdouble depthRange[2]={};
  bool restored=false;
  explicit State(const cc_glglue * glue):g(glue) {
    glGetIntegerv(GL_CURRENT_PROGRAM,&program); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active); glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment); glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength);
    glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT,&imageHeight); glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels); glGetIntegerv(GL_UNPACK_SKIP_IMAGES,&skipImages);
    glGetIntegerv(GL_UNPACK_SWAP_BYTES,&swapBytes);
    for(int i=0;i<3;++i) {
      cc_glglue_glActiveTexture(g,GL_TEXTURE0+i);
      glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]); glGetIntegerv(GL_SAMPLER_BINDING,&samplers[i]);
    }
    cc_glglue_glActiveTexture(g,static_cast<GLenum>(active));
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);
    glGetIntegerv(GL_VIEWPORT,viewport); glGetIntegerv(GL_DEPTH_FUNC,&depthFunction);
    glGetIntegerv(GL_POLYGON_MODE,polygon); glGetBooleanv(GL_COLOR_WRITEMASK,mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite); glGetDoublev(GL_DEPTH_RANGE,depthRange);
    depth=glIsEnabled(GL_DEPTH_TEST); blend=glIsEnabled(GL_BLEND); cull=glIsEnabled(GL_CULL_FACE);
    scissor=glIsEnabled(GL_SCISSOR_TEST); srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    offset=glIsEnabled(GL_POLYGON_OFFSET_FILL); stencil=glIsEnabled(GL_STENCIL_TEST);
    dither=glIsEnabled(GL_DITHER);rasterDiscard=glIsEnabled(GL_RASTERIZER_DISCARD);
    GLint profile=0;glGetIntegerv(GL_CONTEXT_PROFILE_MASK,&profile);
    compatibility=(profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)!=0;
    if(compatibility)alpha=glIsEnabled(GL_ALPHA_TEST);
  }
  static void enabled(GLenum capability,GLboolean flag) { if(flag)glEnable(capability);else glDisable(capability); }
  void restore() {
    if(restored)return;
    for(int i=0;i<3;++i) {
      cc_glglue_glActiveTexture(g,GL_TEXTURE0+i);
      cc_glglue_glBindTexture(g,GL_TEXTURE_2D,static_cast<GLuint>(textures[i]));
    }
    cc_glglue_glActiveTexture(g,static_cast<GLenum>(active));
    cc_glglue_glBindBuffer(g,GL_PIXEL_UNPACK_BUFFER,static_cast<GLuint>(unpack));
    glPixelStorei(GL_UNPACK_ALIGNMENT,alignment); glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,imageHeight); glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels); glPixelStorei(GL_UNPACK_SKIP_IMAGES,skipImages);
    glPixelStorei(GL_UNPACK_SWAP_BYTES,swapBytes);
    g->glBindVertexArray(static_cast<GLuint>(vao)); cc_glglue_glUseProgram(g,static_cast<GLuint>(program));
    cc_glglue_glBindFramebuffer(g,GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(drawFbo));
    cc_glglue_glBindFramebuffer(g,GL_READ_FRAMEBUFFER,static_cast<GLuint>(readFbo));
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glDepthFunc(static_cast<GLenum>(depthFunction)); glDepthMask(depthWrite);
    glDepthRange(depthRange[0],depthRange[1]); glColorMask(mask[0],mask[1],mask[2],mask[3]);
    if(polygon[0]==polygon[1])glPolygonMode(GL_FRONT_AND_BACK,static_cast<GLenum>(polygon[0]));
    else {glPolygonMode(GL_FRONT,static_cast<GLenum>(polygon[0]));glPolygonMode(GL_BACK,static_cast<GLenum>(polygon[1]));}
    enabled(GL_DEPTH_TEST,depth);enabled(GL_BLEND,blend);enabled(GL_CULL_FACE,cull);
    enabled(GL_SCISSOR_TEST,scissor);enabled(GL_FRAMEBUFFER_SRGB,srgb);
    enabled(GL_POLYGON_OFFSET_FILL,offset);enabled(GL_STENCIL_TEST,stencil);
    enabled(GL_DITHER,dither);enabled(GL_RASTERIZER_DISCARD,rasterDiscard);
    if(compatibility)enabled(GL_ALPHA_TEST,alpha);
    restored=true;
  }
  ~State(){restore();}
};
}
struct SoCpuRayPresenter::Impl {
  const cc_glglue * glue=nullptr; void * context=nullptr;
  GLuint program=0, vao=0, textures[3]={};
  GLint colorLocation=-1,depthLocation=-1,coverageLocation=-1,originLocation=-1;
};
SoCpuRayPresenter::SoCpuRayPresenter():p(new Impl){}
SoCpuRayPresenter::~SoCpuRayPresenter(){delete p;}
void SoCpuRayPresenter::discard(){*p=Impl();}
void SoCpuRayPresenter::shutdown(){
  if(p->glue && p->context && p->context==coin_gl_current_context()) {
    if(p->program)cc_glglue_glDeleteProgram(p->glue,p->program);
    if(p->vao && p->glue->glDeleteVertexArrays)p->glue->glDeleteVertexArrays(1,&p->vao);
    glDeleteTextures(3,p->textures);
  }
  discard();
}
bool SoCpuRayPresenter::prepare(const cc_glglue * glue,std::string & detail){
  try {
  detail.clear();void * current=coin_gl_current_context();
  if(!current || !glue || !cc_glglue_glversion_matches_at_least(glue,3,3,0) ||
      !glue->glGenVertexArrays || !glue->glBindVertexArray || !glue->glDeleteVertexArrays ||
      !glue->glUniform1i || !glue->glUniform2i || !glue->glActiveTexture) {
    detail="CPU presentation requires current OpenGL3.3 shader/VAO dispatch";return false;
  }
  if(p->context && (p->context!=current || p->glue!=glue)) {
    detail="CPU presenter context changed before owner shutdown/discard";return false;
  }
  GLenum previous=glGetError();
  if(previous!=GL_NO_ERROR){errorDetail(detail,"preexisting GL error before CPU prepare",previous);return false;}
  // These caller states can alter/clip a fullscreen shader whose outputs do
  // not implement the caller's logical operations or user clipping contract.
  const bool logicOp=glIsEnabled(GL_COLOR_LOGIC_OP)!=GL_FALSE;
  GLint maxClipDistances=0;glGetIntegerv(GL_MAX_CLIP_DISTANCES,&maxClipDistances);
  if(maxClipDistances<0 || maxClipDistances>64){detail="CPU presenter clip-distance query exceeds bound";return false;}
  bool clipping=false;
  for(GLint i=0;i<maxClipDistances;++i)clipping=clipping || glIsEnabled(GL_CLIP_DISTANCE0+i)!=GL_FALSE;
  const GLenum stateError=glGetError();
  if(stateError!=GL_NO_ERROR){errorDetail(detail,"CPU caller-state preflight failed",stateError);return false;}
  if(logicOp){detail="CPU presenter does not replace caller color logic operations";return false;}
  if(clipping){detail="CPU presenter does not implement caller clip distances";return false;}
  // Reject incompatible caller samplers before any framebuffer mutation.
  GLint active=GL_TEXTURE0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
  bool externalSampler=false;
  for(int i=0;i<3;++i){
    cc_glglue_glActiveTexture(glue,GL_TEXTURE0+i);
    GLint sampler=0;glGetIntegerv(GL_SAMPLER_BINDING,&sampler);
    externalSampler=externalSampler || sampler!=0;
  }
  cc_glglue_glActiveTexture(glue,static_cast<GLenum>(active));
  const GLenum queryError=glGetError();
  if(queryError!=GL_NO_ERROR){errorDetail(detail,"CPU sampler preflight failed",queryError);return false;}
  if(externalSampler){detail="CPU presenter does not replace caller sampler objects";return false;}
  if(p->program)return true;
  p->glue=glue;p->context=current;
  // Creating shaders/VAO/texture names does not alter a framebuffer or bindings.
  ShaderName vs{glue,shader(glue,GL_VERTEX_SHADER,vertexSource,detail)};
  if(!vs.name){shutdown();return false;}
  ShaderName fs{glue,shader(glue,GL_FRAGMENT_SHADER,fragmentSource,detail)};
  if(!fs.name){shutdown();return false;}
  p->program=cc_glglue_glCreateProgram(glue);
  if(p->program){
    cc_glglue_glAttachShader(glue,p->program,vs.name);cc_glglue_glAttachShader(glue,p->program,fs.name);
    cc_glglue_glLinkProgram(glue,p->program);
  }
  GLint linked=0;if(p->program)cc_glglue_glGetGLSLProgramiv(glue,p->program,GL_LINK_STATUS,&linked);
  if(!linked){char log[2048]={};GLsizei count=0;if(p->program)cc_glglue_glGetProgramInfoLog(glue,p->program,2047,&count,log);detail=std::string("CPU presentation link failed: ")+log;shutdown();return false;}
  p->colorLocation=cc_glglue_glGetUniformLocation(glue,p->program,"cpuColor");
  p->depthLocation=cc_glglue_glGetUniformLocation(glue,p->program,"cpuDepth");
  p->coverageLocation=cc_glglue_glGetUniformLocation(glue,p->program,"cpuCoverage");
  p->originLocation=cc_glglue_glGetUniformLocation(glue,p->program,"viewportOrigin");
  glue->glGenVertexArrays(1,&p->vao);glGenTextures(3,p->textures);
  GLenum error=glGetError();
  if(error!=GL_NO_ERROR || !p->vao || !p->textures[0] || !p->textures[1] || !p->textures[2] ||
      p->colorLocation<0 || p->depthLocation<0 || p->coverageLocation<0 || p->originLocation<0){
    errorDetail(detail,"CPU presenter resource preparation failed",error);shutdown();return false;
  }
  return true;
  }catch(...){shutdown();throw;}
}
bool SoCpuRayPresenter::present(const coin_raytrace::Frame & frame,std::string & detail){
  detail.clear();
  if(!p->program || !p->glue || p->context!=coin_gl_current_context()){
    detail="CPU presenter was not prepared in this current context";return false;
  }
  // Capture/render admission selects the profile. The presenter independently
  // bounds allocation at the largest private profile and validates every array.
  if(frame.width<=0 || frame.height<=0 || frame.width>4096 || frame.height>4096 ||
      static_cast<size_t>(frame.width)*frame.height>4194304){
    detail="CPU frame dimensions exceed presenter contract";return false;
  }
  const size_t pixels=static_cast<size_t>(frame.width)*frame.height;
  if(frame.rgb.size()!=pixels*3 || frame.depth.size()!=pixels || frame.covered.size()!=pixels){
    detail="CPU frame payload cardinality mismatch";return false;
  }
  for(size_t i=0;i<pixels;++i)if(!std::isfinite(frame.depth[i]) || frame.depth[i]<0 ||
      frame.depth[i]>1 || frame.covered[i]>1){detail="CPU frame depth/coverage invalid";return false;}
  GLenum previous=glGetError();
  if(previous!=GL_NO_ERROR){errorDetail(detail,"preexisting GL error before CPU upload",previous);return false;}
  const cc_glglue * g=p->glue;State state(g);
  for(int i=0;i<3;++i)if(state.samplers[i]!=0){detail="CPU presenter does not replace caller sampler objects";state.restore();return false;}
  cc_glglue_glBindBuffer(g,GL_PIXEL_UNPACK_BUFFER,0);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
  glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_FALSE); // Owned depth uses native float byte order.
  const GLint formats[3]={GL_RGB8,GL_R32F,GL_R8UI};
  const GLenum channels[3]={GL_RGB,GL_RED,GL_RED_INTEGER};
  const GLenum types[3]={GL_UNSIGNED_BYTE,GL_FLOAT,GL_UNSIGNED_BYTE};
  const void * data[3]={frame.rgb.data(),frame.depth.data(),frame.covered.data()};
  for(int i=0;i<3;++i){
    cc_glglue_glActiveTexture(g,GL_TEXTURE0+i);cc_glglue_glBindTexture(g,GL_TEXTURE_2D,p->textures[i]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
    glTexImage2D(GL_TEXTURE_2D,0,formats[i],frame.width,frame.height,0,channels[i],types[i],data[i]);
  }
  cc_glglue_glUseProgram(g,p->program);g->glUniform1i(p->colorLocation,0);
  g->glUniform1i(p->depthLocation,1);g->glUniform1i(p->coverageLocation,2);
  g->glUniform2i(p->originLocation,frame.origin[0],frame.origin[1]);g->glBindVertexArray(p->vao);
  glViewport(frame.origin[0],frame.origin[1],frame.width,frame.height);
  glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);
  glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_POLYGON_OFFSET_FILL);glDisable(GL_STENCIL_TEST);
  glDisable(GL_DITHER);glDisable(GL_RASTERIZER_DISCARD);
  if(state.compatibility)glDisable(GL_ALPHA_TEST);
  glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glDepthMask(GL_TRUE);glDepthRange(0,1);
  glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
  GLenum upload=glGetError();
  if(upload==GL_NO_ERROR)glDrawArrays(GL_TRIANGLES,0,3);
  GLenum draw=glGetError();state.restore();GLenum restore=glGetError();
  if(upload!=GL_NO_ERROR){errorDetail(detail,"CPU texture upload failed",upload);return false;}
  if(draw!=GL_NO_ERROR){errorDetail(detail,"CPU masked color/depth draw failed",draw);return false;}
  if(restore!=GL_NO_ERROR){errorDetail(detail,"CPU presenter state restoration failed",restore);return false;}
  return true;
}
