#ifndef COIN_SOSTATE_H
#define COIN_SOSTATE_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#include <Inventor/SbBasic.h>
#include <cstdio>
#include <cassert>

class SoAction;
class SoTypeList;
class SoElement;

class COIN_DLL_API SoState {
public:
  SoState(SoAction * action,
          const SoTypeList & enabledelements);
  ~SoState(void);

  SoAction * getAction(void) const;

  SoElement * getElement(const int stackindex);
  const SoElement * getConstElement(const int stackindex) const;

  void push(void);
  void pop(void);

  void print(FILE * const file = stdout) const;

  SbBool isElementEnabled(const int stackindex) const;

  int getDepth(void) const;

  void setCacheOpen(const SbBool flag);
  SbBool isCacheOpen(void) const;

  SoElement * getElementNoPush(const int stackindex) const;

private:
  SoElement ** stack;
  int numstacks;
  SbBool cacheopen;
  class SoStateP * pimpl;  

  // Depth is here (not in the pimpl) so the inline getElement() fast path
  // below can read it without exposing the incomplete SoStateP type.
  int depth;

  // Out-of-line copy-on-write push branch for stale elements, called by getElement().
  SoElement * getElementPush(const int stackindex, SoElement * element);
};

// these methods are used very often, and is therefore inlined

inline SbBool
SoState::isElementEnabled(const int stackindex) const
{
  return (stackindex < this->numstacks) && (this->stack[stackindex] != NULL);
}

// getElement() (the library's hottest call) is defined at the end of
// SoElement.h: it dereferences SoElement, and SoElement.h is where SoElement
// and SoState are both complete (it includes SoState.h).  The copy-on-write
// push is out of line in SoState::getElementPush() (SoState.cpp).

inline const SoElement *
SoState::getConstElement(const int stackindex) const
{
  assert(this->isElementEnabled(stackindex));
  return this->stack[stackindex];
}

inline SbBool 
SoState::isCacheOpen(void) const
{
  return this->cacheopen;
}

inline SoElement *
SoState::getElementNoPush(const int stackindex) const
{
  assert(this->isElementEnabled(stackindex));
  return this->stack[stackindex];
}

#endif // !COIN_SOSTATE_H
