#ifndef COIN_SOVERTEXCOLORELEMENT_H
#define COIN_SOVERTEXCOLORELEMENT_H

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

#ifndef COIN_INTERNAL
#error this is a private header file
#endif // !COIN_INTERNAL

#include <Inventor/elements/SoSubElement.h>
#include <Inventor/lists/SbList.h>

/*
  Private traversal state for packed vertex colors.

  SoVertexProperty writes packed colors through SoLazyElement, where the
  legacy representation replaces the active diffuse/transparency arrays.
  Deferred rendering needs to preserve the material opacity that was active
  before that replacement so it can compose it with the vertex alpha. This
  element carries that distinction through ordinary traversal state without
  changing the legacy element or public node APIs.
*/
class SoVertexColorElement : public SoElement {
  typedef SoElement inherited;

  SO_ELEMENT_HEADER(SoVertexColorElement);

public:
  static void initClass(void);

protected:
  virtual ~SoVertexColorElement();

public:
  virtual void init(SoState * state) override;
  virtual void push(SoState * state) override;
  virtual void pop(SoState * state,
                   const SoElement * prevTopElement) override;

  virtual SbBool matches(const SoElement * element) const override;
  virtual SoElement * copyMatchInfo(void) const override;

  // Capture material opacity before SoVertexProperty replaces packed colors.
  static void captureInheritedOpacities(SoState * state,
                                        SbList<float> & opacities);

  // Mark the current packed colors as independent vertex color data.
  static void setPacked(SoState * state, const SbList<float> & opacities);

  // Clear provenance when another node replaces the packed color state.
  static void clear(SoState * state);

  // Return whether the current packed colors came from SoVertexProperty.
  static SbBool isPacked(SoState * state);

  // Return the preserved opacity for a material index.
  static float getInheritedOpacity(SoState * state, int materialIndex);

private:
  SbBool packed;
  SbList<float> inheritedOpacities;
};

#endif // !COIN_SOVERTEXCOLORELEMENT_H
