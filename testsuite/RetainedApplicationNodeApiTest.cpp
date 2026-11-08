#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/actions/SoIRRenderAction.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSubNode.h>

#include <iostream>

#if !defined(COIN_RETAINED_RENDERER_API_VERSION) || COIN_RETAINED_RENDERER_API_VERSION < 2
#error RetainedApplicationNodeApiTest requires the public application-node classification API
#endif

class PublicNonDrawingNode : public SoNode {
  SO_NODE_HEADER(PublicNonDrawingNode);

public:
  enum Behavior {
    VALID_EMPTY,
    DUPLICATE_MARKER,
    EMIT_AFTER_MARKER,
    MISMATCHED_NODE
  };

  static void initClass(void);
  PublicNonDrawingNode(void);

  Behavior behavior = VALID_EMPTY;
  SoNode * markerNode = nullptr;

  void IRRender(SoIRRenderAction * action) override
  {
    action->markApplicationNodeNonDrawing(
      this->behavior == MISMATCHED_NODE ? this->markerNode : this);
    if (this->behavior == DUPLICATE_MARKER) {
      action->markApplicationNodeNonDrawing(this);
    }
    else if (this->behavior == EMIT_AFTER_MARKER) {
      action->addCommand(SoRenderCommand());
    }
  }

protected:
  ~PublicNonDrawingNode() override {}
};

SO_NODE_SOURCE(PublicNonDrawingNode);

void
PublicNonDrawingNode::initClass(void)
{
  SO_NODE_INIT_CLASS(PublicNonDrawingNode, SoNode, "SoNode");
}

PublicNonDrawingNode::PublicNonDrawingNode(void)
{
  SO_NODE_CONSTRUCTOR(PublicNonDrawingNode);
}

int
main()
{
  SoDB::init();
  PublicNonDrawingNode::initClass();

  PublicNonDrawingNode * node = new PublicNonDrawingNode;
  node->ref();
  PublicNonDrawingNode * other = new PublicNonDrawingNode;
  other->ref();
  node->markerNode = other;
  SoIRRenderAction action(SbViewportRegion(64, 64));
  action.apply(static_cast<SoNode *>(node));

  int result = 0;
  if (action.hasUnsupportedRendering() ||
      action.getDrawList().getNumCommands() != 0) {
    std::cerr << "FAIL: public non-drawing application-node classification "
                 "did not admit an empty retained callback" << std::endl;
    result = 1;
  }

  node->behavior = PublicNonDrawingNode::DUPLICATE_MARKER;
  action.apply(static_cast<SoNode *>(node));
  if (!action.hasUnsupportedRendering()) {
    std::cerr << "FAIL: duplicate public non-drawing marker did not fail closed"
              << std::endl;
    result = 1;
  }

  node->behavior = PublicNonDrawingNode::EMIT_AFTER_MARKER;
  action.apply(static_cast<SoNode *>(node));
  if (!action.hasUnsupportedRendering()) {
    std::cerr << "FAIL: non-drawing node emitted a command without failing closed"
              << std::endl;
    result = 1;
  }

  node->behavior = PublicNonDrawingNode::MISMATCHED_NODE;
  action.apply(static_cast<SoNode *>(node));
  if (!action.hasUnsupportedRendering()) {
    std::cerr << "FAIL: mismatched public non-drawing marker did not fail closed"
              << std::endl;
    result = 1;
  }

  action.beginFrame();
  action.markApplicationNodeNonDrawing(node);
  if (!action.hasUnsupportedRendering()) {
    std::cerr << "FAIL: out-of-traversal public marker did not fail closed"
              << std::endl;
    result = 1;
  }

  node->behavior = PublicNonDrawingNode::VALID_EMPTY;
  action.apply(static_cast<SoNode *>(node));
  if (action.hasUnsupportedRendering() ||
      action.getDrawList().getNumCommands() != 0) {
    std::cerr << "FAIL: valid classification did not recover after failures"
              << std::endl;
    result = 1;
  }

  other->unref();
  node->unref();
  SoDB::finish();
  return result;
}
