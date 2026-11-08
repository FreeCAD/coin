#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoIRRenderAction.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSwitch.h>

#include <iostream>

#include "actions/SoIRCoverageP.h"

static void
check(bool condition, const char * message, int & result)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    result = 1;
  }
}

int
main()
{
  SoDB::init();
  int result = 0;

  SoSeparator * root = new SoSeparator;
  root->ref();
  SoSwitch * branch = new SoSwitch;
  branch->whichChild = 0;
  branch->addChild(new SoCube);
  branch->addChild(new SoCube);
  root->addChild(branch);

  SoIRRenderAction action(SbViewportRegion(64, 64));
  action.apply(root);
  const SoIRApplicationCoverageReceipt & initial =
    action.getApplicationCoverageReceipt();
  check(action.isApplicationCoverageReceiptCurrent() &&
        initial.records.size() == 1 &&
        initial.controlDependencies.size() == 1 &&
        initial.controlDependencies[0].kind ==
          SoIRControlDependencyKind::SWITCH_DECISION &&
        initial.controlDependencies[0].rawDecision == 0 &&
        initial.controlDependencies[0].effectiveDecision == 0 &&
        initial.controlDependencies[0].childCount == 2,
        "initial switch decision was not receipt-bound", result);

  SoIRApplicationCoverageReceipt & corruptKind =
    const_cast<SoIRApplicationCoverageReceipt &>(
      action.getApplicationCoverageReceipt());
  corruptKind.controlDependencies[0].kind =
    static_cast<SoIRControlDependencyKind>(255);
  check(!action.isApplicationCoverageReceiptCurrent(),
        "unknown control dependency kind was accepted", result);
  action.apply(root);

  SoSeparator * overlayRoot = new SoSeparator;
  overlayRoot->ref();
  SoSwitch * overlaySwitch = new SoSwitch;
  overlaySwitch->whichChild = SO_SWITCH_NONE;
  overlaySwitch->addChild(new SoCube);
  overlayRoot->addChild(overlaySwitch);
  (void) action.getState();
  action.beginFrame();
  action.traverseAdditionalRoot(root);
  action.traverseAdditionalRoot(overlayRoot);
  check(action.isApplicationCoverageReceiptCurrent() &&
        action.getApplicationCoverageReceipt().controlDependencies.size() == 2,
        "sealed receipt rejected a later-root switch dependency", result);
  overlayRoot->unref();
  action.apply(root);

  const SbBool notify = branch->enableNotify(FALSE);
  branch->whichChild = 1;
  branch->enableNotify(notify);
  check(!action.isApplicationCoverageReceiptCurrent(),
        "suppressed switch edit escaped dependency validation", result);
  action.apply(root);
  check(action.isApplicationCoverageReceiptCurrent() &&
        action.getApplicationCoverageReceipt().controlDependencies.size() == 1 &&
        action.getApplicationCoverageReceipt().controlDependencies[0].
          effectiveDecision == 1,
        "switch dependency did not recover after rebuild", result);

  SoIRApplicationCoverageReceipt & corruptDecision =
    const_cast<SoIRApplicationCoverageReceipt &>(
      action.getApplicationCoverageReceipt());
  corruptDecision.controlDependencies[0].effectiveDecision = 0;
  check(!action.isApplicationCoverageReceiptCurrent(),
        "invalid switch decision relation was accepted", result);
  action.apply(root);

  const SbBool ignoredNotify = branch->enableNotify(FALSE);
  branch->whichChild.setIgnored(TRUE);
  branch->enableNotify(ignoredNotify);
  check(!action.isApplicationCoverageReceiptCurrent(),
        "suppressed switch ignored-state edit escaped validation", result);
  action.apply(root);
  check(action.isApplicationCoverageReceiptCurrent() &&
        action.getDrawList().getNumCommands() == 0 &&
        action.getApplicationCoverageReceipt().records.empty() &&
        action.getApplicationCoverageReceipt().controlDependencies.size() == 1 &&
        action.getApplicationCoverageReceipt().controlDependencies[0].
          fieldIgnored,
        "explicit no-branch switch decision was not admitted", result);

  const SbBool childNotify = branch->enableNotify(FALSE);
  branch->whichChild.setIgnored(FALSE);
  branch->whichChild = 1;
  branch->addChild(new SoCube);
  branch->enableNotify(childNotify);
  check(!action.isApplicationCoverageReceiptCurrent(),
        "suppressed switch child-count edit escaped validation", result);
  action.apply(root);
  check(action.isApplicationCoverageReceiptCurrent() &&
        action.getApplicationCoverageReceipt().controlDependencies[0].
          childCount == 3,
        "switch dependency did not recover after child-count rebuild", result);

  action.setApplicationCoverageRecordLimit(1);
  action.apply(root);
  const int firstOverflowRefCount = branch->getRefCount();
  check(action.hasUnsupportedRendering() &&
        !action.isApplicationCoverageReceiptCurrent(),
        "combined producer/dependency limit did not fail closed", result);
  action.apply(root);
  check(branch->getRefCount() == firstOverflowRefCount,
        "overflow recovery leaked a switch dependency reference", result);
  action.setApplicationCoverageRecordLimit(2);
  action.apply(root);
  check(!action.hasUnsupportedRendering() &&
        action.isApplicationCoverageReceiptCurrent(),
        "ordinary capture did not recover after dependency overflow", result);

  branch->whichChild = SO_SWITCH_NONE;
  action.apply(root);
  check(action.isApplicationCoverageReceiptCurrent() &&
        action.getDrawList().getNumCommands() == 0 &&
        action.getApplicationCoverageReceipt().records.empty() &&
        action.getApplicationCoverageReceipt().controlDependencies.size() == 1,
        "SO_SWITCH_NONE was not represented as a current control decision",
        result);

  branch->ref();
  root->unref();
  check(action.isApplicationCoverageReceiptCurrent(),
        "owned switch dependency did not survive source-root destruction",
        result);
  action.beginFrame();
  check(branch->getRefCount() == 1,
        "beginFrame did not release the owned switch dependency", result);
  branch->unref();

  SoDB::finish();
  return result;
}
