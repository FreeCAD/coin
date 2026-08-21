#include <Inventor/SoDB.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/nodes/SoSeparator.h>

#include <iostream>

int main()
{
  SoDB::init();
  SoSeparator * background = new SoSeparator;
  SoSeparator * foreground = new SoSeparator;
  background->ref();
  foreground->ref();

  int result = 0;
  {
    SoRenderManager manager;
    manager.setRenderLayerRoot(SoRenderManager::RENDER_LAYER_BACKGROUND,
                               background);
    manager.setRenderLayerRoot(SoRenderManager::RENDER_LAYER_FOREGROUND,
                               foreground);
    if (manager.getRenderLayerRoot(
          SoRenderManager::RENDER_LAYER_BACKGROUND) != background ||
        manager.getRenderLayerRoot(
          SoRenderManager::RENDER_LAYER_FOREGROUND) != foreground) {
      std::cerr << "FAIL: render-layer roots were not retained" << std::endl;
      result = 1;
    }
    manager.setRenderLayerRoot(SoRenderManager::RENDER_LAYER_FOREGROUND, NULL);
    if (manager.getRenderLayerRoot(
          SoRenderManager::RENDER_LAYER_FOREGROUND) != NULL) {
      std::cerr << "FAIL: foreground root was not released" << std::endl;
      result = 1;
    }
  }

  foreground->unref();
  background->unref();
  SoDB::finish();
  return result;
}
