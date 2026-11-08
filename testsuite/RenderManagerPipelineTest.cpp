#include <Inventor/SoDB.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/actions/SoIRRenderAction.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/rendering/SoRenderIR.h>

#include <iostream>

#include "rendering/SoRenderAdmissionP.h"

static int
verifyRetainedFrameAdmission()
{
  SoSeparator * root = new SoSeparator;
  root->ref();
  root->addChild(new SoCube);

  SoIRRenderAction action(SbViewportRegion(64, 64));
  action.apply(root);
  if (coin_evaluate_retained_frame_admission(action) !=
      SoRetainedFrameAdmission::ADMITTED) {
    std::cerr << "FAIL: current retained frame was not admitted" << std::endl;
    root->unref();
    return 1;
  }

  action.getMutableDrawList().addCommand(SoRenderCommand());
  const SoRetainedFrameAdmission staleVerdict =
    coin_evaluate_retained_frame_admission(action);
  if (staleVerdict !=
        SoRetainedFrameAdmission::APPLICATION_COVERAGE_INVALID ||
      !coin_retained_frame_admission_reason(staleVerdict)) {
    std::cerr << "FAIL: stale receipt did not require manager fallback"
              << std::endl;
    root->unref();
    return 1;
  }

  action.apply(root);
  action.markUnsupported(root, "test unsupported traversal");
  if (coin_evaluate_retained_frame_admission(action) !=
      SoRetainedFrameAdmission::TRAVERSAL_UNSUPPORTED) {
    std::cerr << "FAIL: unsupported traversal did not require manager fallback"
              << std::endl;
    root->unref();
    return 1;
  }

  root->unref();
  return 0;
}

int
main()
{
  SoDB::init();

  int result = verifyRetainedFrameAdmission();
  {
    SoRenderManager manager;
    manager.setRenderPipeline(SoRenderManager::RenderPipeline::LEGACY_GL);

#if COIN_HAVE_LEGACY_GL_RENDERER
    if (manager.getRenderPipeline() != SoRenderManager::RenderPipeline::LEGACY_GL) {
      std::cerr << "FAIL: LegacyGL build rejected its available pipeline" << std::endl;
      result = 1;
    }
#else
    if (manager.getRenderPipeline() != SoRenderManager::RenderPipeline::DRAW_LIST) {
      std::cerr << "FAIL: core-only build accepted LEGACY_GL" << std::endl;
      result = 1;
    }
#endif

    manager.setRenderPipeline(SoRenderManager::RenderPipeline::DRAW_LIST);
    if (manager.getRenderPipeline() != SoRenderManager::RenderPipeline::DRAW_LIST) {
      result = 1;
    }
  }
  SoDB::finish();
  return result;
}
