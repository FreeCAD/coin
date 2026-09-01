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


#include <Inventor/scxml/ScXMLCoinEvaluator.h>
#include <cmath>
#include <cfloat>
#include <Inventor/scxml/ScXMLStateMachine.h>

// FIXME: this is no longer possible in the MinimumEvaluator - move testcase to
// the CoinEvaluator.

template<class EXPECTED_TYPE>
struct DataObjDemangler {
};

template<>
struct DataObjDemangler<ScXMLRealDataObj> {
  typedef double RET_VAL;
  static RET_VAL getValue(ScXMLRealDataObj * obj) {
    return obj->getReal();
  }
};

template<>
struct DataObjDemangler<ScXMLBoolDataObj> {
  typedef bool RET_VAL;
  static RET_VAL getValue(ScXMLBoolDataObj * obj) {
    return obj->getBool();
  }
};

#define COIN_REQUIRE_MESSAGE( P , M) \
  do { INFO(M); REQUIRE((P)); } while (false);      \
  if (!(P))                          \
    return false;


template <class EXPECTED_TYPE>
bool
TestReturnValue(const std::string & evaluationString, typename DataObjDemangler< EXPECTED_TYPE >::RET_VAL retVal, ScXMLEvaluator * evaluator)
{
  ScXMLDataObj * res = NULL;

  res = evaluator->evaluate(evaluationString.c_str());
  COIN_REQUIRE_MESSAGE(res != NULL, std::string("Evaluation returned nothing for expression: ") + evaluationString);
  //FIXME: Should really remember to delete res before returning from this point on,
  //but don't bother about a memory leak when there are bigger fish to
  //fry. 20090613 BFG
  COIN_REQUIRE_MESSAGE(res->getTypeId() == EXPECTED_TYPE::getClassTypeId(), std::string("The type returned was incorrect for expression: ") + evaluationString);
  EXPECTED_TYPE * obj = static_cast<EXPECTED_TYPE *>(res);
  COIN_REQUIRE_MESSAGE(DataObjDemangler<EXPECTED_TYPE>::getValue(obj) == retVal, std::string("Return value mismatch for expression: ") + evaluationString);

  delete res;
  return true;
}


TEST_CASE("ScXMLCoinEvaluator_TestSuite.BasicExpressions", "[ScXMLCoinEvaluator_TestSuite]")
{
  std::unique_ptr<ScXMLStateMachine> sm(new ScXMLStateMachine);
  std::unique_ptr<ScXMLEvaluator> evaluator(new ScXMLCoinEvaluator);
  evaluator->setStateMachine(sm.get());

  TestReturnValue<ScXMLRealDataObj>("M_PI",M_PI,evaluator.get());
  TestReturnValue<ScXMLRealDataObj>("5.0",5.0,evaluator.get());
  TestReturnValue<ScXMLRealDataObj>("5 + 5",10.0,evaluator.get());
  TestReturnValue<ScXMLRealDataObj>("5 - 4",1.0,evaluator.get());
  TestReturnValue<ScXMLRealDataObj>("5 * 4",20.0,evaluator.get());
  TestReturnValue<ScXMLRealDataObj>("5 / 4",1.25,evaluator.get());
  TestReturnValue<ScXMLRealDataObj>("-5",-5.0,evaluator.get());

  TestReturnValue<ScXMLBoolDataObj>("True",TRUE,evaluator.get());
  TestReturnValue<ScXMLBoolDataObj>("false",FALSE,evaluator.get());
  TestReturnValue<ScXMLBoolDataObj>("!false",TRUE,evaluator.get());
  TestReturnValue<ScXMLBoolDataObj>("M_PI != M_LN2",TRUE,evaluator.get());
  TestReturnValue<ScXMLBoolDataObj>("M_PI == M_LN2",FALSE,evaluator.get());
  TestReturnValue<ScXMLBoolDataObj>("false && !false",FALSE,evaluator.get());
}
