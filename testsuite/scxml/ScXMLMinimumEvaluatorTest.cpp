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


#include <Inventor/scxml/ScXMLMinimumEvaluator.h>
#include <cmath>
#include <cfloat>
#include <Inventor/scxml/ScXMLStateMachine.h>
#include <Inventor/scxml/ScXMLDocument.h>
#include <Inventor/scxml/ScXML.h>
#include <Inventor/scxml/ScXMLElt.h>
#include <Inventor/SbByteBuffer.h>
#include <Inventor/SbString.h>
BOOST_AUTO_TEST_SUITE(ScXMLMinimumEvaluator_TestSuite);


BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(MimimumExpressions,1);

BOOST_AUTO_TEST_CASE(MimimumExpressions)
{
  std::unique_ptr<ScXMLStateMachine> sm(new ScXMLStateMachine);
  std::unique_ptr<ScXMLEvaluator> evaluator(new ScXMLMinimumEvaluator);

  ScXMLDataObj * res = NULL;
  ScXMLBoolDataObj * boolobj = NULL;

  //FIXME, this test is not finished. BFG 20090831

  static const char foo [] =
    "<scxml version=\"1.0\" profile=\"minimum\" name=\"foo\" initial=\"active\"><state id=\"active\"></state><state id=\"inactive\"></state></scxml>";
  /*
  ScXMLDocument * doc = ScXMLDocument::readBuffer(SbByteBuffer(sizeof(foo),foo));
  assert(doc->getRoot());
  sm->setDescription(doc);
  sm->setName("<memory buffer>");
  sm->initialize();
  assert(sm->isActive() && !sm->isFinished());
  printf("%d\n", sm->getNumActiveStates());
  const ScXMLElt * elt = sm->getActiveState(0);
  printf("%s\n",elt->getXMLAttribute("id"));
  evaluator->setStateMachine(sm.get());
  res = evaluator->evaluate("In('active')");
  */

}


BOOST_AUTO_TEST_SUITE_END();
