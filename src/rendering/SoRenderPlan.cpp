
#include "rendering/SoRenderPlan.h"

#include <algorithm>

namespace {

enum class OpaqueGroup : uint8_t {
  TRIANGLES,
  NATIVE_LINES,
  NONE
};

OpaqueGroup
classifyOpaqueGroup(const SoRenderCommand & command,
                    const SoGeometryDesc & geometry)
{
  const SoTextureData & texture = command.material.texture;
  const bool textured = texture.cacheKey != 0 || texture.pixels != NULL;

  // Reordering is restricted to ordinary opaque depth-writing geometry.
  // Specialized raster state remains insertion ordered because equal-depth
  // results or viewport-local effects can depend on command order.
  const bool groupable = command.opacityClass == SO_OPACITY_OPAQUE &&
    geometry.cacheKey != 0 &&
    command.material.shadingModel == SO_SHADING_UNLIT && !textured &&
    command.material.opacity == 1.0f &&
    command.material.diffuse[3] == 1.0f &&
    !command.state.blend.enabled && command.state.depth.enabled &&
    command.state.depth.writeEnabled &&
    command.state.alphaTest.policy == SO_ALPHA_TEST_POLICY_NONE &&
    command.state.raster.visible &&
    command.state.raster.fillMode == SO_RASTER_FILL &&
    !command.state.raster.viewportOverride &&
    !command.state.raster.polygonOffsetFilled &&
    !command.state.raster.polygonOffsetLines &&
    !command.state.raster.polygonOffsetPoints &&
    !command.state.useCommandMatrices && !command.pixelRaster.enabled;
  if (!groupable) return OpaqueGroup::NONE;
  if (geometry.topology == SO_TOPOLOGY_TRIANGLES) {
    return OpaqueGroup::TRIANGLES;
  }
  if (geometry.topology == SO_TOPOLOGY_LINES &&
      command.state.raster.lineWidth <= 1.0f &&
      command.state.raster.linePattern == 0xFFFF) {
    return OpaqueGroup::NATIVE_LINES;
  }
  return OpaqueGroup::NONE;
}

} // namespace

void
SoRenderPlanner::build(const SoDrawList & drawlist,
                       SoRenderPlan & plan) const
{
  plan.operations.clear();
  const uint32_t commandCount = static_cast<uint32_t>(drawlist.getNumCommands());
  const std::vector<SoDepthClearEvent> & events = drawlist.getDepthClearEvents();
  plan.operations.reserve(commandCount + events.size() * 2 + 4);

  struct PlannedCommand {
    uint32_t commandIndex = 0;
    SoOpacityClass opacity = SO_OPACITY_OPAQUE;
    float depth = 0.0f;
    OpaqueGroup opaqueGroup = OpaqueGroup::NONE;
    uint64_t geometryKey = 0;
    SoLightingHandle lightingHandle = 0;
  };

  const auto depthOf = [&drawlist](const uint32_t commandIndex) {
    const SoRenderCommand & command = drawlist.getCommand(
      static_cast<int>(commandIndex));
    const SoGeometryDesc & geometry = drawlist.getCommandGeometry(command);
    const SbVec3f localCenter = geometry.hasBounds
      ? geometry.boundsCenter : SbVec3f(0.0f, 0.0f, 0.0f);
    SbVec3f worldCenter;
    SbVec3f eyeCenter;
    command.modelMatrix.multVecMatrix(localCenter, worldCenter);
    command.viewMatrix.multVecMatrix(worldCenter, eyeCenter);
    return -eyeCenter[2];
  };

  const auto emitSegment = [&drawlist, &plan, &depthOf](
    const SoRenderStage stage, const uint32_t begin, const uint32_t end) {
    std::vector<PlannedCommand> commands;
    for (uint32_t commandIndex = begin; commandIndex < end; ++commandIndex) {
      const SoRenderCommand & command = drawlist.getCommand(
        static_cast<int>(commandIndex));
      if (command.stage != stage) continue;
      PlannedCommand planned;
      planned.commandIndex = commandIndex;
      planned.opacity = command.opacityClass;
      planned.depth = depthOf(commandIndex);
      const SoGeometryDesc & geometry =
        drawlist.getCommandGeometry(command);
      planned.geometryKey = geometry.cacheKey;
      planned.lightingHandle = command.lightingHandle;
      planned.opaqueGroup = classifyOpaqueGroup(command, geometry);
      commands.push_back(planned);
    }
    std::stable_sort(commands.begin(), commands.end(),
      [](const PlannedCommand & lhs, const PlannedCommand & rhs) {
        if (lhs.opacity != rhs.opacity) {
          return lhs.opacity == SO_OPACITY_OPAQUE;
        }
        if (lhs.opacity == SO_OPACITY_OPAQUE) return false;
        return lhs.depth > rhs.depth;
      });
    std::stable_sort(commands.begin(), commands.end(),
      [](const PlannedCommand & lhs, const PlannedCommand & rhs) {
        if (lhs.opaqueGroup != rhs.opaqueGroup) {
          return lhs.opaqueGroup < rhs.opaqueGroup;
        }
        if (lhs.opaqueGroup == OpaqueGroup::NONE) return false;
        if (lhs.geometryKey != rhs.geometryKey) {
          return lhs.geometryKey < rhs.geometryKey;
        }
        return lhs.lightingHandle < rhs.lightingHandle;
      });
    for (const PlannedCommand & command : commands) {
      SoRenderOperation draw;
      draw.type = SoRenderOperationType::DRAW;
      draw.commandIndex = command.commandIndex;
      plan.operations.push_back(draw);
    }
  };

  const SoRenderStage stages[] = {
    SoRenderStage::Background,
    SoRenderStage::Main,
    SoRenderStage::AfterMain,
    SoRenderStage::Foreground
  };
  for (const SoRenderStage stage : stages) {
    std::vector<size_t> stageEvents;
    for (size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex) {
      if (events[eventIndex].stage == stage) stageEvents.push_back(eventIndex);
    }
    uint32_t begin = 0;
    for (size_t nextEvent = 0; nextEvent <= stageEvents.size();
         ++nextEvent) {
      const uint32_t end = nextEvent < stageEvents.size()
        ? std::min(events[stageEvents[nextEvent]].sequence, commandCount)
        : commandCount;
      emitSegment(stage, begin, end);
      if (nextEvent == stageEvents.size()) break;

      SoRenderOperation barrier;
      barrier.type = SoRenderOperationType::END_DEPTH_SEGMENT;
      plan.operations.push_back(barrier);

      SoRenderOperation clear;
      clear.type = SoRenderOperationType::CLEAR_DEPTH;
      clear.depthClearEventIndex = static_cast<uint32_t>(
        stageEvents[nextEvent]);
      plan.operations.push_back(clear);
      begin = end;
    }

    SoRenderOperation stageEnd;
    stageEnd.type = SoRenderOperationType::END_DEPTH_SEGMENT;
    plan.operations.push_back(stageEnd);
  }

  if (plan.operations.empty() ||
      plan.operations.back().type != SoRenderOperationType::END_DEPTH_SEGMENT) {
    SoRenderOperation frameEnd;
    frameEnd.type = SoRenderOperationType::END_DEPTH_SEGMENT;
    plan.operations.push_back(frameEnd);
  }
}
