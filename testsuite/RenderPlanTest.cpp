#include "rendering/SoRenderPlan.h"

#include <iostream>
#include <vector>

namespace {

bool check(bool condition, const char * message)
{
  if (!condition) std::cerr << "FAIL: " << message << std::endl;
  return condition;
}

int drawOperationCount(const SoRenderPlan & plan)
{
  int count = 0;
  for (int i = 0; i < plan.getNumOperations(); ++i) {
    if (plan.getOperation(i).type == SoRenderOperationType::DRAW) ++count;
  }
  return count;
}

}

int
main()
{
  bool result = true;
#ifdef COIN_INTERNAL
  SoDrawList revisions;
  SoRenderCommand revisionCommand;
  revisions.addCommand(revisionCommand);
  uint64_t contentRevision = revisions.getContentRevision();
  uint64_t planRevision = revisions.getRenderPlanRevision();
  revisions.getCommandForStateUpdate(0).modelMatrix.makeIdentity();
  result = check(revisions.getContentRevision() > contentRevision &&
                 revisions.getRenderPlanRevision() == planRevision,
                 "dynamic state update invalidated derived layout") && result;

  contentRevision = revisions.getContentRevision();
  planRevision = revisions.getRenderPlanRevision();
  revisions.getCommandPreservingPickTopology(0).opacityClass =
    SO_OPACITY_TRANSPARENT;
  result = check(revisions.getContentRevision() > contentRevision &&
                 revisions.getRenderPlanRevision() > planRevision,
                 "ordering update did not isolate render-plan invalidation") &&
    result;

  contentRevision = revisions.getContentRevision();
  planRevision = revisions.getRenderPlanRevision();
  revisions.getCommand(0).geometry.revision++;
  result = check(revisions.getContentRevision() > contentRevision &&
                 revisions.getRenderPlanRevision() > planRevision,
                 "generic mutation did not invalidate render planning") &&
    result;
#endif // COIN_INTERNAL

  SoDrawList drawlist;
  SoRenderCommand first;
  SoRenderCommand second;
  SoRenderCommand third;
  first.userData = &first;
  second.userData = &second;
  third.userData = &third;
  drawlist.addCommand(first);
  drawlist.addCommand(second);
  drawlist.addCommand(third);

  const uint32_t generation = drawlist.getGeneration();
  const void * const firstData = drawlist.getCommand(0).userData;
  const void * const secondData = drawlist.getCommand(1).userData;
  const void * const thirdData = drawlist.getCommand(2).userData;

  SoRenderPlanner planner;
  SoRenderPlan plan;
  SoDrawList empty;
  planner.build(empty, plan);
  result = check(drawOperationCount(plan) == 0 &&
                 plan.getNumOperations() > 0,
                 "empty DrawList did not produce a barrier-only plan") &&
    result;

  planner.build(drawlist, plan);
  result = check(drawOperationCount(plan) == 3,
                 "planner did not retain every command") &&
    check(plan.getOperation(1).type == SoRenderOperationType::DRAW &&
          plan.getOperation(1).commandIndex == 0 &&
          plan.getOperation(2).type == SoRenderOperationType::DRAW &&
          plan.getOperation(2).commandIndex == 1 &&
          plan.getOperation(3).type == SoRenderOperationType::DRAW &&
          plan.getOperation(3).commandIndex == 2,
          "planner changed insertion order") &&
    check(drawlist.getGeneration() == generation &&
          drawlist.getCommand(0).userData == firstData &&
          drawlist.getCommand(1).userData == secondData &&
          drawlist.getCommand(2).userData == thirdData,
          "planning modified the retained DrawList");

  SoDrawList groupedDrawList;
  SoRenderCommand surfaceA1;
  SoRenderCommand edge;
  SoRenderCommand edge2;
  SoRenderCommand surfaceB;
  SoRenderCommand surfaceA2;
  surfaceA1.geometry.topology = SO_TOPOLOGY_TRIANGLES;
  surfaceA1.geometry.cacheKey = 10;
  surfaceA2.geometry = surfaceA1.geometry;
  surfaceB.geometry.topology = SO_TOPOLOGY_TRIANGLES;
  surfaceB.geometry.cacheKey = 20;
  edge.geometry.topology = SO_TOPOLOGY_LINES;
  edge.geometry.cacheKey = 5;
  edge2.geometry = edge.geometry;
  groupedDrawList.addCommand(surfaceA1);
  groupedDrawList.addCommand(edge);
  groupedDrawList.addCommand(surfaceB);
  groupedDrawList.addCommand(edge2);
  groupedDrawList.addCommand(surfaceA2);
  planner.build(groupedDrawList, plan);
  std::vector<uint32_t> groupedDraws;
  for (int i = 0; i < plan.getNumOperations(); ++i) {
    if (plan.getOperation(i).type == SoRenderOperationType::DRAW) {
      groupedDraws.push_back(plan.getOperation(i).commandIndex);
    }
  }
  result = check(groupedDraws.size() == 5 && groupedDraws[0] == 0 &&
                 groupedDraws[1] == 4 && groupedDraws[2] == 2 &&
                 groupedDraws[3] == 1 && groupedDraws[4] == 3,
                 "planner did not group opaque surfaces and edges") &&
    result;

  drawlist.truncate(2);
  planner.build(drawlist, plan);
  result = check(drawOperationCount(plan) == 2,
                 "planner did not rebuild from the current DrawList") && result;

  drawlist.clear();
  planner.build(drawlist, plan);
  result = check(drawOperationCount(plan) == 0,
                 "rebuilding an empty DrawList left stale draw operations") &&
    result;

  drawlist.addCommand(first);
  planner.build(drawlist, plan);
  result = check(drawOperationCount(plan) == 1 &&
                 plan.getOperation(1).type == SoRenderOperationType::DRAW &&
                 plan.getOperation(1).commandIndex == 0,
                 "reusing a cleared plan did not rebuild its operations") &&
    result;

  SoDrawList transparencyDrawList;
  SoRenderCommand opaque;
  SoRenderCommand transparentNear;
  SoRenderCommand transparentFar;
  opaque.viewMatrix.makeIdentity();
  opaque.modelMatrix.makeIdentity();
  transparentNear.viewMatrix.makeIdentity();
  transparentNear.modelMatrix.makeIdentity();
  transparentFar.viewMatrix.makeIdentity();
  transparentFar.modelMatrix.makeIdentity();
  transparentNear.opacityClass = SO_OPACITY_TRANSPARENT;
  transparentFar.opacityClass = SO_OPACITY_TRANSPARENT;
  transparentFar.modelMatrix.setTranslate(SbVec3f(0.0f, 0.0f, -2.0f));
  transparencyDrawList.addCommand(opaque);
  transparencyDrawList.addCommand(transparentNear);
  transparencyDrawList.addCommand(transparentFar);
  planner.build(transparencyDrawList, plan);
  std::vector<uint32_t> transparentDraws;
  for (int i = 0; i < plan.getNumOperations(); ++i) {
    if (plan.getOperation(i).type == SoRenderOperationType::DRAW) {
      transparentDraws.push_back(plan.getOperation(i).commandIndex);
    }
  }
  result = check(transparentDraws.size() == 3 &&
                 transparentDraws[0] == 0 &&
                 transparentDraws[1] == 2 &&
                 transparentDraws[2] == 1,
                 "planner did not schedule transparent commands back-to-front") &&
    result;

  // The model origin is not a geometry depth. These objects deliberately
  // place their origins on opposite sides of their actual local geometry.
  SoDrawList boundsDrawList;
  SoRenderCommand boundsNear;
  SoRenderCommand boundsFar;
  const float nearPosition[] = { 0.0f, 0.0f, -10.0f };
  const float farPosition[] = { 0.0f, 0.0f, 0.0f };
  boundsNear.geometry.topology = SO_TOPOLOGY_POINTS;
  boundsNear.geometry.vertexCount = 1;
  boundsNear.geometry.positions = nearPosition;
  boundsNear.geometry.hasBounds = TRUE;
  boundsNear.geometry.boundsCenter = SbVec3f(0.0f, 0.0f, -10.0f);
  boundsNear.opacityClass = SO_OPACITY_TRANSPARENT;
  boundsNear.viewMatrix.makeIdentity();
  boundsNear.modelMatrix.setTranslate(SbVec3f(0.0f, 0.0f, 9.0f));
  boundsFar.geometry.topology = SO_TOPOLOGY_POINTS;
  boundsFar.geometry.vertexCount = 1;
  boundsFar.geometry.positions = farPosition;
  boundsFar.geometry.hasBounds = TRUE;
  boundsFar.geometry.boundsCenter = SbVec3f(0.0f, 0.0f, 0.0f);
  boundsFar.opacityClass = SO_OPACITY_TRANSPARENT;
  boundsFar.viewMatrix.makeIdentity();
  boundsFar.modelMatrix.setTranslate(SbVec3f(0.0f, 0.0f, -3.0f));
  boundsDrawList.addCommand(boundsNear);
  boundsDrawList.addCommand(boundsFar);
  planner.build(boundsDrawList, plan);
  std::vector<uint32_t> boundsDraws;
  for (int i = 0; i < plan.getNumOperations(); ++i) {
    if (plan.getOperation(i).type == SoRenderOperationType::DRAW) {
      boundsDraws.push_back(plan.getOperation(i).commandIndex);
    }
  }
  result = check(boundsDraws.size() == 2 && boundsDraws[0] == 1 &&
                 boundsDraws[1] == 0,
                 "planner sorted transparency from model origins instead of bounds") &&
    result;

  drawlist.addCommand(second);
  SoDepthClearEvent event;
  event.sequence = 1;
  drawlist.addDepthClearEvent(event);
  planner.build(drawlist, plan);
  bool sawClear = false;
  bool sawEndBeforeClear = false;
  bool sawDrawAfterClear = false;
  for (int i = 0; i < plan.getNumOperations(); ++i) {
    const SoRenderOperation & operation = plan.getOperation(i);
    if (operation.type == SoRenderOperationType::END_DEPTH_SEGMENT && !sawClear) {
      sawEndBeforeClear = true;
    }
    if (operation.type == SoRenderOperationType::CLEAR_DEPTH &&
        operation.depthClearEventIndex == 0) {
      sawClear = true;
    }
    if (sawClear && operation.type == SoRenderOperationType::DRAW &&
        operation.commandIndex == 1) {
      sawDrawAfterClear = true;
    }
  }
  result = check(sawEndBeforeClear && sawClear && sawDrawAfterClear,
                 "planner did not preserve a depth-clear barrier") && result;
  return result ? 0 : 1;
}
