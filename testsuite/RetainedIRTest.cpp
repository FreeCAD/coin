#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoIRRenderAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShaderProgram.h>

#include <iostream>

static int
runTest()
{
  SoIRRenderAction action(SbViewportRegion(64, 64));
  SoSeparator * root = new SoSeparator;
  root->ref();

  int result = 0;
  action.apply(root);
  if (action.getDrawList().getNumCommands() != 0) {
    std::cerr << "FAIL: empty scene emitted retained commands" << std::endl;
    result = 1;
  }

  SoRenderCommand command;
  command.objectId = 42;
  command.geometry.topology = SO_TOPOLOGY_TRIANGLES;
  command.geometry.vertexCount = 3;
  command.geometry.vertexStride = sizeof(float) * 3;
  command.material.diffuse.setValue(1.0f, 0.0f, 0.0f, 1.0f);
  action.getMutableDrawList().addCommand(command);
  if (action.getDrawList().getNumCommands() != 1 ||
      action.getDrawList().getCommand(0).geometry.vertexCount != 3) {
    std::cerr << "FAIL: retained command was not stored" << std::endl;
    result = 1;
  }

  action.getMutableDrawList().buildPickLUT();
  if (action.getDrawList().getPickLUT().size() != 1 ||
      action.getDrawList().resolvePickId(1) == NULL) {
    std::cerr << "FAIL: retained pick lookup table did not resolve its command"
              << std::endl;
    result = 1;
  }

  const SoPickLUTEntry * pickEntry = action.getDrawList().resolvePickId(1);
  if (!pickEntry || pickEntry->objectId == 0) {
    std::cerr << "FAIL: retained pick entry did not carry stable object identity"
              << std::endl;
    result = 1;
  }

  SoSelectionState selection;
  SoSelectionTarget selected;
  selected.commandIndex = 0;
  selected.type = SO_PICK_OBJECT;
  selected.elementIndex = -1;
  selected.color = SbColor4f(1.0f, 0.0f, 0.0f, 0.5f);
  selection.selected.push_back(selected);
  if (selection.selected.size() != 1 ||
      selection.selected[0].commandIndex != 0 ||
      selection.selected[0].color[0] != 1.0f) {
    std::cerr << "FAIL: selection target did not retain frame identity"
              << std::endl;
    result = 1;
  }

  action.getMutableDrawList().getCommand(0).pick.pickable = false;
  action.getMutableDrawList().buildPickLUT();
  if (!action.getDrawList().getPickLUT().empty()) {
    std::cerr << "FAIL: non-pickable command entered the pick lookup table"
              << std::endl;
    result = 1;
  }
  action.getMutableDrawList().clear();
  if (action.getDrawList().resolvePickId(1) != NULL) {
    std::cerr << "FAIL: clearing a draw list left a stale pick lookup" << std::endl;
    result = 1;
  }

  SoShaderProgram * shader = new SoShaderProgram;
  shader->ref();
  action.apply(shader);
  if (!action.hasUnsupportedRendering() ||
      action.getUnsupportedNode() != shader ||
      action.getUnsupportedReason() == NULL) {
    std::cerr << "FAIL: unsupported retained shader semantics were silently ignored"
              << std::endl;
    result = 1;
  }
  shader->unref();

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
