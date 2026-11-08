#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoPath.h>
#include <Inventor/actions/SoIRRenderAction.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoTranslation.h>

#include <iostream>

#include "actions/SoIRCoverageP.h"
#include "rendering/SoRenderAdmissionP.h"

class ApplicationFaceSet : public SoFaceSet {
  SO_NODE_HEADER(ApplicationFaceSet);

public:
  static void initClass(void);
  ApplicationFaceSet(void);

protected:
  ~ApplicationFaceSet() override {}
};

SO_NODE_SOURCE(ApplicationFaceSet);

void
ApplicationFaceSet::initClass(void)
{
  SO_NODE_INIT_CLASS(ApplicationFaceSet, SoFaceSet, "FaceSet");
}

ApplicationFaceSet::ApplicationFaceSet(void)
{
  SO_NODE_CONSTRUCTOR(ApplicationFaceSet);
}

static void
check(bool condition, const char * message, int & result)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    result = 1;
  }
}

static SoPath *
makePath(SoNode * root, SoNode * middle, SoNode * tail)
{
  SoPath * path = new SoPath(root);
  path->ref();
  path->append(middle);
  path->append(tail);
  return path;
}

static int
runStatePatchTests()
{
  int result = 0;
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoSeparator * branch = new SoSeparator;
  SoTranslation * translation = new SoTranslation;
  SoMaterial * material = new SoMaterial;
  branch->addChild(translation);
  branch->addChild(material);
  branch->addChild(new SoCube);
  root->addChild(branch);

  SoPath * translationPath = makePath(root, branch, translation);
  SoPath * materialPath = makePath(root, branch, material);
  SoIRRenderAction action(SbViewportRegion(64, 64));
  (void) action.getState();
  action.beginFrame();
  action.traverseAdditionalRoot(root);
  check(action.isApplicationCoverageReceiptCurrent(),
        "state patch scene did not start with a current receipt", result);

  translation->translation.setValue(0.25f, 0.0f, 0.0f);
  const int matrixUpdates =
    action.updateCommandMatricesForStatePath(translationPath);
  if (matrixUpdates != 1 || !action.isApplicationCoverageReceiptCurrent()) {
    std::cerr << "INFO: matrix updates=" << matrixUpdates
              << " current=" << action.isApplicationCoverageReceiptCurrent()
              << std::endl;
  }
  check(matrixUpdates == 1 &&
        action.isApplicationCoverageReceiptCurrent() &&
        coin_evaluate_retained_frame_admission(action) ==
          SoRetainedFrameAdmission::ADMITTED,
        "matrix patch did not commit a current coverage transaction", result);

  material->diffuseColor.setValue(0.2f, 0.4f, 0.6f);
  const int materialUpdates =
    action.updateCommandMaterialsForStatePath(materialPath, false);
  if (materialUpdates != 1 || !action.isApplicationCoverageReceiptCurrent()) {
    std::cerr << "INFO: material updates=" << materialUpdates
              << " current=" << action.isApplicationCoverageReceiptCurrent()
              << std::endl;
  }
  check(materialUpdates == 1 &&
        action.isApplicationCoverageReceiptCurrent() &&
        coin_evaluate_retained_frame_admission(action) ==
          SoRetainedFrameAdmission::ADMITTED,
        "material patch did not commit a current coverage transaction", result);

  action.getMutableDrawList().addCommand(SoRenderCommand());
  translation->translation.setValue(0.5f, 0.0f, 0.0f);
  check(action.updateCommandMatricesForStatePath(translationPath) == 0 &&
        !action.isApplicationCoverageReceiptCurrent() &&
        coin_evaluate_retained_frame_admission(action) ==
          SoRetainedFrameAdmission::APPLICATION_COVERAGE_INVALID,
        "structurally stale DrawList was rebound by a state patch", result);

  action.beginFrame();
  action.traverseAdditionalRoot(root);
  check(action.isApplicationCoverageReceiptCurrent(),
        "full traversal did not recover after rejected patch", result);

  translationPath->unref();
  materialPath->unref();
  root->unref();
  return result;
}

static int
runVisibilityPatchTest()
{
  int result = 0;
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoSwitch * visibleSwitch = new SoSwitch;
  visibleSwitch->whichChild = SO_SWITCH_ALL;
  visibleSwitch->addChild(new SoCube);
  root->addChild(visibleSwitch);

  SoPath * switchPath = new SoPath(root);
  switchPath->ref();
  switchPath->append(visibleSwitch);
  SoIRRenderAction action(SbViewportRegion(64, 64));
  (void) action.getState();
  action.beginFrame();
  action.traverseAdditionalRoot(root);
  check(action.isApplicationCoverageReceiptCurrent(),
        "visibility scene did not start with a current receipt", result);
  const int visibilityUpdates =
    action.updateCommandVisibilityForSwitchPath(switchPath, FALSE);
  if (visibilityUpdates != 1 ||
      !action.isApplicationCoverageReceiptCurrent()) {
    std::cerr << "INFO: visibility updates=" << visibilityUpdates
              << " current=" << action.isApplicationCoverageReceiptCurrent()
              << std::endl;
  }
  check(visibilityUpdates == 1 &&
        action.isApplicationCoverageReceiptCurrent() &&
        coin_evaluate_retained_frame_admission(action) ==
          SoRetainedFrameAdmission::ADMITTED,
        "visibility patch did not commit a current coverage transaction",
        result);

  switchPath->unref();
  root->unref();
  return result;
}

static int
runGeometryPatchTests()
{
  int result = 0;
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoSeparator * branch = new SoSeparator;
  SoCoordinate3 * coordinates = new SoCoordinate3;
  coordinates->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
  coordinates->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
  coordinates->point.set1Value(2, SbVec3f(0.0f, 1.0f, 0.0f));
  ApplicationFaceSet * faceSet = new ApplicationFaceSet;
  faceSet->numVertices.set1Value(0, 3);
  branch->addChild(coordinates);
  branch->addChild(faceSet);
  root->addChild(branch);

  SoPath * coordinatePath = makePath(root, branch, coordinates);
  SoIRRenderAction action(SbViewportRegion(64, 64));
  (void) action.getState();
  action.beginFrame();
  action.traverseAdditionalRoot(root);
  const SoIRApplicationCoverageReceipt & initialReceipt =
    action.getApplicationCoverageReceipt();
  check(initialReceipt.records.size() == 1 &&
        initialReceipt.records[0].classification ==
          SoIRApplicationCoverageClass::ADMITTED_COMMANDS &&
        action.isApplicationCoverageReceiptCurrent(),
        "application geometry did not start with one admitted record", result);

  coordinates->point.set1Value(0, SbVec3f(-0.75f, -1.0f, 0.0f));
  const int geometryUpdates =
    action.updateCommandGeometryForStatePath(coordinatePath);
  if (geometryUpdates != 1 || !action.isApplicationCoverageReceiptCurrent()) {
    std::cerr << "INFO: geometry updates=" << geometryUpdates
              << " current=" << action.isApplicationCoverageReceiptCurrent()
              << " records="
              << action.getApplicationCoverageReceipt().records.size()
              << std::endl;
  }
  check(geometryUpdates == 1 &&
        action.getApplicationCoverageReceipt().records.size() == 1 &&
        action.isApplicationCoverageReceiptCurrent() &&
        coin_evaluate_retained_frame_admission(action) ==
          SoRetainedFrameAdmission::ADMITTED,
        "geometry replay leaked transient coverage or lost currentness", result);

  SoIRApplicationCoverageReceipt & corruptedReceipt =
    const_cast<SoIRApplicationCoverageReceipt &>(
      action.getApplicationCoverageReceipt());
  corruptedReceipt.records[0].firstCommand =
    action.getDrawList().getNumCommands() + 1;
  coordinates->point.set1Value(0, SbVec3f(-0.5f, -1.0f, 0.0f));
  check(!action.isApplicationCoverageReceiptCurrent() &&
        action.updateCommandGeometryForStatePath(coordinatePath) == 0 &&
        coin_evaluate_retained_frame_admission(action) ==
          SoRetainedFrameAdmission::APPLICATION_COVERAGE_INVALID,
        "invalid command range was admitted or rebound", result);
  action.beginFrame();
  action.traverseAdditionalRoot(root);
  check(action.getApplicationCoverageReceipt().records.size() == 1 &&
        action.isApplicationCoverageReceiptCurrent(),
        "full traversal did not recover from invalid receipt structure", result);

  const int originalCommandCount = action.getDrawList().getNumCommands();
  const uint32_t originalVertexCount = action.getDrawList().getCommandGeometry(
    action.getDrawList().getCommand(0)).vertexCount;
  coordinates->point.set1Value(3, SbVec3f(0.0f, 0.0f, 1.0f));
  faceSet->numVertices.set1Value(0, 4);
  check(action.updateCommandGeometryForStatePath(coordinatePath) == 0 &&
        action.getDrawList().getNumCommands() == originalCommandCount &&
        action.getDrawList().getCommandGeometry(
          action.getDrawList().getCommand(0)).vertexCount ==
            originalVertexCount &&
        action.getApplicationCoverageReceipt().records.size() == 1 &&
        !action.isApplicationCoverageReceiptCurrent() &&
        coin_evaluate_retained_frame_admission(action) ==
          SoRetainedFrameAdmission::APPLICATION_COVERAGE_INVALID,
        "failed geometry patch did not roll back and fail closed", result);

  faceSet->numVertices.set1Value(0, 3);
  action.beginFrame();
  action.traverseAdditionalRoot(root);
  check(action.getApplicationCoverageReceipt().records.size() == 1 &&
        action.isApplicationCoverageReceiptCurrent(),
        "full traversal did not recover after failed geometry transaction",
        result);

  coordinatePath->unref();
  root->unref();
  return result;
}

int
main()
{
  SoDB::init();
  ApplicationFaceSet::initClass();
  const int result = runStatePatchTests() |
    runVisibilityPatchTest() |
    runGeometryPatchTests();
  SoDB::finish();
  return result;
}
