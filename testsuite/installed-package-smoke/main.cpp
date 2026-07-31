#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoDB.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/system/gl.h>

int
main()
{
  SoDB::init();
  int result;
  {
    SoGLRenderAction action(SbViewportRegion(1, 1));
    result = action.getClassTypeId().isDerivedFrom(SoGLRenderAction::getClassTypeId()) ? 0 : 1;
  }
  SoDB::finish();
  return result;
}
