/**************************************************************************\
 * Copyright (c) 2026 The Coin3D contributors                          *
 *                                                                        *
 * This file is part of Coin.                                            *
 *                                                                        *
 * Coin is free software; you can redistribute it and/or modify it under *
 * the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your      *
 * option) any later version.                                            *
\**************************************************************************/

#include "SoVertexColorElement.h"

#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/misc/SoState.h>

#include <algorithm>
#include <cassert>

SO_ELEMENT_SOURCE(SoVertexColorElement);

void
SoVertexColorElement::initClass(void)
{
  SO_ELEMENT_INIT_CLASS(SoVertexColorElement, inherited);
}

SoVertexColorElement::~SoVertexColorElement()
{
}

void
SoVertexColorElement::init(SoState * state)
{
  inherited::init(state);
  this->packed = FALSE;
  this->inheritedOpacities.truncate(0);
}

void
SoVertexColorElement::push(SoState * state)
{
  inherited::push(state);
  const SoVertexColorElement * previous =
    static_cast<const SoVertexColorElement *>(this->getNextInStack());
  this->packed = previous->packed;
  this->inheritedOpacities = previous->inheritedOpacities;
}

void
SoVertexColorElement::pop(SoState * state,
                            const SoElement * prevTopElement)
{
  inherited::pop(state, prevTopElement);
}

SbBool
SoVertexColorElement::matches(const SoElement * /* element */) const
{
  assert(0 && "should never be called.");
  return TRUE;
}

SoElement *
SoVertexColorElement::copyMatchInfo(void) const
{
  assert(0 && "should never be called.");
  return NULL;
}

void
SoVertexColorElement::captureInheritedOpacities(
  SoState * state, SbList<float> & opacities)
{
  opacities.truncate(0);
  if (!state->isElementEnabled(classStackIndex)) return;

  const SoVertexColorElement * element =
    static_cast<const SoVertexColorElement *>(
      SoElement::getConstElement(state, classStackIndex));
  if (element->packed) {
    opacities = element->inheritedOpacities;
    return;
  }

  // A packed color written by another node is itself vertex data, not a
  // material opacity source. Start a new composition from neutral opacity.
  SoLazyElement * lazy = SoLazyElement::getInstance(state);
  if (lazy->isPacked()) {
    opacities.append(1.0f);
    return;
  }

  const int count = std::max(1, lazy->getNumTransparencies());
  for (int i = 0; i < count; ++i) {
    opacities.append(1.0f - SoLazyElement::getTransparency(state, i));
  }
}

void
SoVertexColorElement::setPacked(SoState * state,
                                  const SbList<float> & opacities)
{
  if (!state->isElementEnabled(classStackIndex)) return;

  SoVertexColorElement * element =
    static_cast<SoVertexColorElement *>(
      SoElement::getElement(state, classStackIndex));
  element->packed = TRUE;
  element->inheritedOpacities = opacities;
  if (element->inheritedOpacities.getLength() == 0) {
    // captureInheritedOpacities() normally always supplies a neutral entry.
    // Keep the state conservative if a future caller violates that contract.
    element->packed = FALSE;
  }
}

void
SoVertexColorElement::clear(SoState * state)
{
  if (!state->isElementEnabled(classStackIndex)) return;

  SoVertexColorElement * element =
    static_cast<SoVertexColorElement *>(
      SoElement::getElement(state, classStackIndex));
  element->packed = FALSE;
  element->inheritedOpacities.truncate(0);
}

SbBool
SoVertexColorElement::isPacked(SoState * state)
{
  if (!state->isElementEnabled(classStackIndex)) return FALSE;

  const SoVertexColorElement * element =
    static_cast<const SoVertexColorElement *>(
      SoElement::getConstElement(state, classStackIndex));
  return element->packed && SoLazyElement::getInstance(state)->isPacked();
}

float
SoVertexColorElement::getInheritedOpacity(SoState * state,
                                            int materialIndex)
{
  if (!state->isElementEnabled(classStackIndex)) return 1.0f;

  const SoVertexColorElement * element =
    static_cast<const SoVertexColorElement *>(
      SoElement::getConstElement(state, classStackIndex));
  if (!element->packed ||
      !SoLazyElement::getInstance(state)->isPacked() ||
      element->inheritedOpacities.getLength() == 0) {
    return 1.0f;
  }
  const int index = std::max(
    0, std::min(materialIndex, element->inheritedOpacities.getLength() - 1));
  return element->inheritedOpacities[index];
}
