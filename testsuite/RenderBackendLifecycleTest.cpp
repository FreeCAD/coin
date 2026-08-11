#include "support/GLTestContext.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>

#include <iostream>

namespace {

int skip(const char * reason)
{
  std::cout << "SKIP: " << reason << std::endl;
  return 77;
}

bool rendered(SoRenderManager & manager)
{
  const SoRenderManager::RenderResult & result = manager.getLastRenderResult();
  return result.rendered &&
    result.usedPipeline == SoRenderManager::RenderPipeline::DRAW_LIST;
}

} // namespace

int main()
{
  SoDB::init();
  GLTestContextConfig contextConfig;
  contextConfig.width = 16;
  contextConfig.height = 16;
  GLTestContext context;
  if (!context.initialize(contextConfig)) {
    SoDB::finish();
    return skip("core OpenGL test context is unavailable");
  }

  SoSeparator * scene = new SoSeparator;
  scene->addChild(new SoCube);
  scene->ref();

  int result = 0;
  {
    SoRenderManager manager;
    manager.setViewportRegion(SbViewportRegion(SbVec2s(16, 16)));
    manager.setRenderPipeline(SoRenderManager::RenderPipeline::DRAW_LIST);
    manager.setSceneGraph(scene);
    manager.render(TRUE, TRUE);
    if (!rendered(manager)) result = 1;

    manager.releaseRenderBackendResources();
    manager.render(TRUE, TRUE);
    if (!rendered(manager)) {
      std::cerr << "FAIL: backend did not recover after a normal release" << std::endl;
      result = 1;
    }

    context.shutdown();
    manager.discardRenderBackendResources();

    if (!context.initialize(contextConfig)) {
      std::cerr << "FAIL: could not recreate EGL context" << std::endl;
      result = 1;
    }
    else {
      manager.render(TRUE, TRUE);
      if (!rendered(manager)) {
        std::cerr << "FAIL: backend did not recover after context loss" << std::endl;
        result = 1;
      }
    }
  }

  context.shutdown();
  scene->unref();
  SoDB::finish();
  return result;
}
