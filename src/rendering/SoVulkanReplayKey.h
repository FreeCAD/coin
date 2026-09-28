// src/rendering/SoVulkanReplayKey.h

#ifndef COIN_SOVULKANREPLAYKEY_H
#define COIN_SOVULKANREPLAYKEY_H

/*!
  \file SoVulkanReplayKey.h
  \brief Graph-fingerprint helpers for the Vulkan retained-IR replay.

  These back SoVulkanRenderManagerP's camera-only-frame replay: a hash over
  the render-affecting node ids (mixed with their pointer identity) that is
  deliberately invariant under camera / light / environment chatter, so a
  navigation frame leaves the retained draw list replayable.

  Extracted from the manager (previously an anonymous-namespace block in
  SoVulkanRenderManager.cpp) so the key, its node-class exclusions and the
  walk can be reasoned about and unit-tested in isolation.  The functions are
  header-inline; the pure graph walk has no dependency on the manager.
*/

#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLight.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransformSeparator.h>

#include <cstddef>
#include <cstdint>

namespace CoinVulkanReplay {

//! Order-sensitive hash mix used by the fingerprint walk.
inline void mixHash(uint64_t & h, uint64_t v)
{
  h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
}

/*!
  \brief True for a node whose id the graph fingerprint deliberately ignores.

  The camera-coupled chatter (camera / light / environment / rotation /
  transform-separator) and the plain container nodes.  Coin propagates a
  notification up the parent chain, so any changed node re-bumps every
  ancestor's node-id; FreeCAD re-aims the headlight rotation to follow the
  camera every navigation frame, and those container ids are re-bumped purely
  by propagation.  None of these nodes produce the rasterized fill-geometry in
  the retained main draw list (lighting is re-derived every frame by the
  backend), so excluding their ids only suppresses the camera-coupled chatter.
  Real geometry edits use transform / shape / selection nodes, which still
  fold their ids.  Shared with the scene-dirty sensor so the two stay
  consistent.
*/
inline bool fingerprintSkipsNodeId(const SoNode * node)
{
  return node->isOfType(SoCamera::getClassTypeId()) ||
    node->isOfType(SoLight::getClassTypeId()) ||
    node->isOfType(SoEnvironment::getClassTypeId()) ||
    node->isOfType(SoRotation::getClassTypeId()) ||
    node->isOfType(SoTransformSeparator::getClassTypeId()) ||
    node->getTypeId() == SoGroup::getClassTypeId() ||
    node->getTypeId() == SoSeparator::getClassTypeId();
}

//! Recursively fold (node pointer, SoNode::getNodeId()) of every reachable
//! node into \a h.  Camera-coupled infra is excluded via
//! fingerprintSkipsNodeId(), so a camera-only frame yields an unchanged hash
//! and the retained draw list replays.
inline void graphFingerprintWalk(SoNode * node, const SoNode * skip, uint64_t & h)
{
  if (!node || node == skip) return;
  mixHash(h, reinterpret_cast<uintptr_t>(node));
  if (!fingerprintSkipsNodeId(node)) {
    mixHash(h, static_cast<uint64_t>(node->getNodeId()));
  }
  if (node->isOfType(SoGroup::getClassTypeId())) {
    const SoGroup * group = static_cast<const SoGroup *>(node);
    const int num = group->getNumChildren();
    mixHash(h, static_cast<uint64_t>(num));
    for (int i = 0; i < num; ++i) {
      graphFingerprintWalk(group->getChild(i), skip, h);
    }
  }
}

} // namespace CoinVulkanReplay

#endif // COIN_SOVULKANREPLAYKEY_H
