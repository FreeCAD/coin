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

// Keep generated tests on the same feature configuration as the Coin target.
// setup.h is private, so temporarily provide its internal-build guard.
#ifndef COIN_INTERNAL
#define COIN_INTERNAL
#define COIN_TESTSUITE_UNDEF_COIN_INTERNAL
#endif
#include "setup.h"
#ifdef COIN_TESTSUITE_UNDEF_COIN_INTERNAL
#undef COIN_INTERNAL
#undef COIN_TESTSUITE_UNDEF_COIN_INTERNAL
#endif
#include "CoinTest.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <memory>
#include "TestSuiteUtils.h"
#include "TestSuiteMisc.h"
#include <Inventor/misc/SoRefPtr.h>
using namespace SIM::Coin3D::Coin;
using namespace SIM::Coin3D::Coin::TestSuite;


#include <Inventor/draggers/SoTransformerDragger.h>
#include <Inventor/SbDict.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/nodes/SoSeparator.h>
BOOST_AUTO_TEST_SUITE(SoTransformerDragger_TestSuite);


static
SoCallbackAction::Response
register_cb(void * data, SoCallbackAction * action, const SoNode * node)
{
  assert(data);
  SbDict * dict = static_cast<SbDict *>(data);
  dict->enter(reinterpret_cast<uintptr_t>(node), NULL);
  return SoCallbackAction::CONTINUE;
}

static
void
ensure_unique_cb(uintptr_t entry, void * value, void * data)
{
  SbDict * copydict = static_cast<SbDict *>(data);
  void * val = NULL;
  BOOST_ASSERT(!copydict->find(entry, val));
}

BOOST_AUTO_TEST_CASE(dragger_deep_copy)
{
  SbDict origdict, copydict;

  SoSeparator * root = new SoSeparator;
  root->setName("dragger_deep_copy_root");
  root->ref();
  root->addChild(new SoTransformerDragger);

  SoSeparator * copy = static_cast<SoSeparator *>(root->copy());
  assert(copy);
  copy->setName("dragger_deep_copy_copy");
  copy->ref();

  {
    SoCallbackAction cba;
    cba.setCallbackAll(TRUE);

    cba.addPreCallback(SoNode::getClassTypeId(), register_cb, &origdict);
    cba.apply(root);
  }

  {
    SoCallbackAction cba;
    cba.setCallbackAll(TRUE);

    cba.addPreCallback(SoNode::getClassTypeId(), register_cb, &copydict);
    cba.apply(copy);
  }

  SbPList keys, values;

  origdict.makePList(keys, values);
  const int origdictsize = keys.getLength();

  keys.truncate(0);
  values.truncate(0);
  copydict.makePList(keys, values);
  const int copydictsize = keys.getLength();

  BOOST_ASSERT(origdictsize == copydictsize);

  // make sure pointer sets have an empty union
  origdict.applyToAll(ensure_unique_cb, &copydict);

  root->unref();
  copy->unref();
}


BOOST_AUTO_TEST_SUITE_END();
