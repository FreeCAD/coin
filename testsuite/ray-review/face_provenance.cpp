#include "rendering/SoCpuRayOwnedSnapshotP.h"
#include <Inventor/SoDB.h>

#include <future>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace rb = retained_bridge;

static void check(bool ok, const char * message)
{
  if (!ok) throw std::runtime_error(message);
}

static rb::OwnedFrame capture(const SoDrawList & list)
{
  rb::Envelope envelope;
  envelope.viewport = SbViewportRegion(32, 32);
  envelope.cameraVolume.ortho(-1, 1, -1, 1, 1, 10);
  envelope.cameraVolume.getMatrices(envelope.viewMatrix, envelope.projectionMatrix);
  envelope.expectedGeneration = list.getGeneration();
  envelope.expectedContentRevision = list.getContentRevision();
  rb::OwnedFrame frame;
  rb::Status status;
  if (!rb::capture(list, envelope, rb::Limits{}, frame, status))
    throw std::runtime_error(status.detail);
  return frame;
}

using Frames = std::pair<rb::OwnedFrame, rb::OwnedFrame>;

static Frames makeFrames()
{
  std::vector<float> positions, normals;
  for (int x = 0; x < 15; ++x) {
    const float left = float(x);
    positions.insert(positions.end(), {left, 0, 0, left + 1, 0, 0, left, 1, 0});
    normals.insert(normals.end(), {0, 0, 1, 0, 0, 1, 0, 0, 1});
  }
  SoDrawList list;
  list.clear();
  SoGeometryResource mesh;
  mesh.geometry.topology = SO_TOPOLOGY_TRIANGLES;
  mesh.geometry.vertexCount = mesh.geometry.normalCount = 45;
  mesh.geometry.positions = positions.data();
  mesh.geometry.normals = normals.data();
  mesh.elementRanges.push_back({SO_PICK_FACE, 10, 0, 42});
  mesh.elementRanges.push_back({SO_PICK_FACE, 27, 42, 3});
  const auto handle = list.addGeometryResource(mesh);
  SoRenderCommand cmd;
  cmd.modelMatrix.makeIdentity();
  cmd.viewMatrix.makeIdentity();
  cmd.projMatrix.makeIdentity();
  cmd.geometry = mesh.geometry;
  cmd.geometryHandle = handle;
  cmd.instanceId = 101;
  cmd.pick.useResourceElementRanges = true;
  list.addCommand(cmd);
  cmd.instanceId = 202;
  cmd.modelMatrix[3][0] = 20;
  cmd.material.diffuse[0] = 0.5f;
  cmd.pick.useResourceElementRanges = false;
  cmd.pick.elementRanges.push_back({SO_PICK_FACE, 81, 0, 42});
  cmd.pick.elementRanges.push_back({SO_PICK_FACE, 93, 42, 3});
  list.addCommand(cmd);
  auto original = capture(list);
  auto & edited = list.getCommand(1);
  edited.pick.elementRanges.clear();
  edited.modelMatrix[3][0] = 30;
  edited.material.diffuse[0] = 0.25f;
  return {std::move(original), capture(list)};
}

static void checkRanges(const std::vector<rb::ElementRange> & ranges, int first, int last)
{
  check(ranges.size() == 2, "expected two face ranges");
  const auto & a = ranges[0];
  const auto & b = ranges[1];
  check(a.type == SO_PICK_FACE && a.elementIndex == first && a.drawStart == 0 &&
        a.drawCount == 42, "14 triangles must retain their producer face");
  check(b.type == SO_PICK_FACE && b.elementIndex == last && b.drawStart == 42 &&
        b.drawCount == 3, "adjacent triangle must retain its own producer face");
}

static void inspect(const rb::OwnedFrame & frame, bool edited)
{
  check(frame.resources.size() == 1 && frame.commands.size() == 2, "shared mesh lost");
  const auto & mesh = frame.resources.front();
  check(mesh.positions.size() == 45 && mesh.positions.back()[0] == 14, "owned geometry lost");
  const auto & a = frame.commands[0];
  const auto & b = frame.commands[1];
  check(a.resourceId == mesh.resourceId && b.resourceId == mesh.resourceId, "mesh reference changed");
  check(a.instanceId == 101 && b.instanceId == 202, "instance identity changed");
  check(a.model[3][0] == 0 && b.model[3][0] == (edited ? 30 : 20), "transform changed");
  check(a.material.diffuse[0] == double(0.8f) &&
        b.material.diffuse[0] == (edited ? 0.25 : 0.5), "material changed");
  checkRanges(mesh.elementRanges, 10, 27);
  checkRanges(a.elementRanges, 10, 27);
  if (edited) check(b.elementRanges.empty(), "absent command metadata was invented");
  else checkRanges(b.elementRanges, 81, 93);
}

int main()
{
  try {
    SoDB::init();
    auto frames = makeFrames(); // All borrowed arrays and the DrawList have been destroyed.
    std::async(std::launch::async, [frames = std::move(frames)] {
      inspect(frames.first, false);
      inspect(frames.second, true);
    }).get();
    return 0;
  } catch (const std::exception & e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
