#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoPath.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/actions/SoIRRenderAction.h>
#include <Inventor/lists/SoPathList.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSubNode.h>

#include <iostream>

#include "actions/SoIRCoverageP.h"

class PathProbe : public SoNode {
  SO_NODE_HEADER(PathProbe);

public:
  static void initClass(void);
  PathProbe(void);

  bool sawBelowPath = false;
  int nonDrawingMarkers = 0;

  void IRRender(SoIRRenderAction * action) override
  {
    this->sawBelowPath = action->getCurPathCode() == SoAction::BELOW_PATH;
    for (int i = 0; i < this->nonDrawingMarkers; ++i) {
      action->markApplicationNodeNonDrawing(this);
    }
  }

protected:
  ~PathProbe() override {}
};

SO_NODE_SOURCE(PathProbe);

void
PathProbe::initClass(void)
{
  SO_NODE_INIT_CLASS(PathProbe, SoNode, "SoNode");
}

PathProbe::PathProbe(void)
{
  SO_NODE_CONSTRUCTOR(PathProbe);
}

static void
check(bool condition, const char * message, int & result)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    result = 1;
  }
}

static int
runTest()
{
  PathProbe::initClass();

  SoSeparator * root = new SoSeparator;
  root->ref();
  SoCube * firstCube = new SoCube;
  SoCube * secondCube = new SoCube;
  root->addChild(firstCube);
  root->addChild(secondCube);

  PathProbe * probe = new PathProbe;
  probe->ref();

  SoIRRenderAction action(SbViewportRegion(64, 64));
  int result = 0;

  action.apply(static_cast<SoNode *>(root));
  check(action.getDrawList().getNumCommands() == 2,
        "apply(SoNode*) did not traverse both cubes", result);
  const SoIRApplicationCoverageReceipt & builtInReceipt =
    action.getApplicationCoverageReceipt();
  check(action.isApplicationCoverageReceiptCurrent() &&
        builtInReceipt.records.size() == 2 &&
        builtInReceipt.records[0].producerKind ==
          SoIRCoverageProducerKind::BUILT_IN_SHAPE &&
        builtInReceipt.records[0].classification ==
          SoIRApplicationCoverageClass::ADMITTED_COMMANDS &&
        builtInReceipt.records[0].firstCommand == 0 &&
        builtInReceipt.records[0].commandCount == 1 &&
        builtInReceipt.records[1].producerKind ==
          SoIRCoverageProducerKind::BUILT_IN_SHAPE &&
        builtInReceipt.records[1].classification ==
          SoIRApplicationCoverageClass::ADMITTED_COMMANDS &&
        builtInReceipt.records[1].firstCommand == 1 &&
        builtInReceipt.records[1].commandCount == 1,
        "built-in shape commands lacked terminal producer records", result);
  SoIRApplicationCoverageReceipt & corruptedBuiltInReceipt =
    const_cast<SoIRApplicationCoverageReceipt &>(builtInReceipt);
  corruptedBuiltInReceipt.records[0].producerKind =
    static_cast<SoIRCoverageProducerKind>(255);
  check(!action.isApplicationCoverageReceiptCurrent(),
        "unknown producer kind did not invalidate coverage", result);
  action.apply(root);
  check(action.isApplicationCoverageReceiptCurrent(),
        "built-in coverage did not recover after producer-kind corruption",
        result);
  check(action.getCommandPath(0) != NULL &&
        action.getCommandPath(0)->getTail() == firstCube &&
        action.getCommandPath(1) != NULL &&
        action.getCommandPath(1)->getTail() == secondCube,
        "retained commands did not preserve their producing scene paths", result);
  root->removeChild(firstCube);
  check(action.getCommandPath(0) != NULL &&
        action.getCommandPath(0)->getTail() == firstCube,
        "scene mutation changed a retained frame's path snapshot", result);
  root->insertChild(firstCube, 0);

  SoSeparator * emptyRoot = new SoSeparator;
  emptyRoot->ref();
  SoFaceSet * emptyFaceSet = new SoFaceSet;
  emptyRoot->addChild(emptyFaceSet);
  action.apply(emptyRoot);
  check(action.hasUnsupportedRendering() &&
        action.getUnsupportedNode() == emptyFaceSet &&
        action.getApplicationCoverageReceipt().records.size() == 1 &&
        action.getApplicationCoverageReceipt().records[0].producerKind ==
          SoIRCoverageProducerKind::BUILT_IN_SHAPE &&
        action.getApplicationCoverageReceipt().records[0].classification ==
          SoIRApplicationCoverageClass::FALLBACK_REQUIRED &&
        !action.isApplicationCoverageReceiptCurrent(),
        "zero-command built-in shape was silently admitted", result);
  emptyRoot->unref();
  action.apply(root);
  check(action.isApplicationCoverageReceiptCurrent(),
        "ordinary built-in shapes did not recover after empty-shape fallback",
        result);

  // Path nodes must remain alive even before a consumer asks the action to
  // materialize a complete SoPath from its compact frame storage.
  SoSeparator * compactRoot = new SoSeparator;
  compactRoot->ref();
  SoCube * compactCube = new SoCube;
  compactRoot->addChild(compactCube);
  SoIRRenderAction compactAction(SbViewportRegion(64, 64));
  compactAction.apply(compactRoot);
  compactRoot->unref();
  check(compactAction.getCommandPath(0) != NULL &&
        compactAction.getCommandPath(0)->getTail() == compactCube,
        "compact retained path did not preserve its nodes", result);

  action.getMutableDrawList().buildPickLUT();
  check(action.getDrawList().getPickLUT().size() >= 2,
        "pick lookup table did not assign object IDs to retained commands", result);
  const SoPickLUTEntry * firstPick = action.getDrawList().resolvePickId(1);
  check(firstPick != NULL && firstPick->commandIndex == 0,
        "pick lookup table did not resolve the first command", result);

  SoPath * firstPath = new SoPath(root);
  firstPath->append(firstCube);
  firstPath->ref();
  action.apply(firstPath);
  check(action.getDrawList().getNumCommands() == 1 &&
        action.getDrawList().getCommand(0).userData == firstCube,
        "apply(SoPath*) did not traverse the selected cube", result);
  check(action.getCommandPath(0) != NULL &&
        action.getCommandPath(0)->getTail() == firstCube,
        "path traversal did not retain the selected scene path", result);
  firstPath->unref();

  SoPath * oneNodePath = new SoPath(probe);
  oneNodePath->ref();
  action.apply(oneNodePath);
  check(probe->sawBelowPath,
        "one-node SoPath did not use BELOW_PATH traversal", result);
  check(action.getDrawList().getNumCommands() == 0,
        "one-node probe left commands in the retained frame", result);
  check(action.hasUnsupportedRendering() &&
        action.getUnsupportedNode() == probe,
        "application node with no retained output was silently accepted",
        result);
  const SoIRApplicationCoverageReceipt & rejectedReceipt =
    action.getApplicationCoverageReceipt();
  check(rejectedReceipt.records.size() == 1 &&
        rejectedReceipt.records[0].classification ==
          SoIRApplicationCoverageClass::FALLBACK_REQUIRED &&
        !action.isApplicationCoverageReceiptCurrent(),
        "silent application-node receipt was not rejected", result);
  oneNodePath->unref();

  probe->nonDrawingMarkers = 1;
  action.apply(static_cast<SoNode *>(probe));
  const SoIRApplicationCoverageReceipt & noOpReceipt =
    action.getApplicationCoverageReceipt();
  check(!action.hasUnsupportedRendering() &&
        action.isApplicationCoverageReceiptCurrent() &&
        noOpReceipt.records.size() == 1 &&
        noOpReceipt.records[0].producerKind ==
          SoIRCoverageProducerKind::APPLICATION_NODE &&
        noOpReceipt.records[0].classification ==
          SoIRApplicationCoverageClass::EXPLICIT_NON_DRAWING &&
        noOpReceipt.records[0].commandCount == 0,
        "explicit application-node no-op was not admitted", result);

  probe->nonDrawingMarkers = 2;
  action.apply(static_cast<SoNode *>(probe));
  check(action.hasUnsupportedRendering() &&
        action.getApplicationCoverageReceipt().invalid &&
        action.getApplicationCoverageReceipt().records.size() == 1 &&
        action.getApplicationCoverageReceipt().records[0].classification ==
          SoIRApplicationCoverageClass::FALLBACK_REQUIRED &&
        !action.isApplicationCoverageReceiptCurrent(),
        "duplicate application-node classification was accepted", result);
  probe->nonDrawingMarkers = 0;

  SoPath * firstListPath = new SoPath(root);
  firstListPath->append(firstCube);
  firstListPath->ref();
  SoPath * secondListPath = new SoPath(root);
  secondListPath->append(secondCube);
  secondListPath->ref();
  {
    SoPathList pathList;
    pathList.append(firstListPath);
    pathList.append(secondListPath);
    action.apply(pathList);
    check(action.getDrawList().getNumCommands() == 2,
          "apply(SoPathList) did not traverse both paths", result);
  }
  firstListPath->unref();
  secondListPath->unref();

  action.getMutableDrawList().addCommand(SoRenderCommand());
  action.apply(static_cast<SoNode *>(probe));
  check(action.getDrawList().getNumCommands() == 0,
        "repeated apply() did not clear the previous frame", result);
  check(action.hasUnsupportedRendering() &&
        action.getUnsupportedNode() == probe,
        "repeated application-node traversal lost fallback status", result);

  SoSeparator * coverageRoot = new SoSeparator;
  coverageRoot->ref();
  PathProbe * noOpA = new PathProbe;
  PathProbe * noOpB = new PathProbe;
  noOpA->nonDrawingMarkers = 1;
  noOpB->nonDrawingMarkers = 1;
  coverageRoot->addChild(noOpA);
  coverageRoot->addChild(noOpB);
  action.setApplicationCoverageRecordLimit(4);
  action.apply(static_cast<SoNode *>(coverageRoot));
  check(!action.hasUnsupportedRendering() &&
        action.isApplicationCoverageReceiptCurrent() &&
        action.getApplicationCoverageReceipt().records.size() == 2,
        "multi-entry application coverage receipt was incomplete", result);

  action.setApplicationCoverageRecordLimit(1);
  check(!action.isApplicationCoverageReceiptCurrent(),
        "coverage limit change did not stale the existing receipt", result);
  action.apply(static_cast<SoNode *>(coverageRoot));
  check(action.hasUnsupportedRendering() &&
        action.getApplicationCoverageReceipt().overflow &&
        action.getApplicationCoverageReceipt().records.size() == 1 &&
        !action.isApplicationCoverageReceiptCurrent(),
        "application coverage limit did not fail closed", result);

  action.setApplicationCoverageRecordLimit(4);
  action.apply(static_cast<SoNode *>(coverageRoot));
  check(!action.hasUnsupportedRendering() &&
        action.isApplicationCoverageReceiptCurrent() &&
        action.getApplicationCoverageReceipt().records.size() == 2,
        "coverage did not recover after a bounded-record failure", result);
  action.getMutableDrawList().addCommand(SoRenderCommand());
  check(!action.isApplicationCoverageReceiptCurrent(),
        "DrawList mutation did not stale the application coverage receipt",
        result);
  coverageRoot->unref();

  SoSeparator * unpickableRoot = new SoSeparator;
  unpickableRoot->ref();
  SoPickStyle * unpickableStyle = new SoPickStyle;
  unpickableStyle->style = SoPickStyle::UNPICKABLE;
  unpickableRoot->addChild(unpickableStyle);
  unpickableRoot->addChild(new SoCube);
  action.apply(static_cast<SoNode *>(unpickableRoot));
  bool allUnpickable = action.getDrawList().getNumCommands() > 0;
  for (int i = 0; i < action.getDrawList().getNumCommands(); ++i) {
    allUnpickable = allUnpickable &&
      !action.getDrawList().getCommand(i).pick.pickable;
  }
  action.getMutableDrawList().buildPickLUT();
  check(allUnpickable && action.getDrawList().getPickLUT().empty(),
        "SoPickStyle::UNPICKABLE was not retained by DrawList commands",
        result);
  unpickableRoot->unref();

  probe->unref();
  root->unref();
  return result;
}

int
main()
{
  SoDB::init();
  const int result = runTest();
  SoDB::finish();
  return result;
}
