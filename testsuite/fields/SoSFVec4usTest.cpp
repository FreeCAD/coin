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


#include <Inventor/fields/SoSFVec4us.h>

TEST_CASE("SoSFVec4us_TestSuite.SoSFVec4us_initialized", "[SoSFVec4us_TestSuite]")
{
  SoSFVec4us field;
  do { INFO("SoSFVec4us class not initialized"); CHECK((SoSFVec4us::getClassTypeId() != SoType::badType())); } while (false);
  do { INFO("missing class initialization"); CHECK((field.getTypeId() != SoType::badType())); } while (false);
}

TEST_CASE("SoSFVec4us_TestSuite.textinput", "[SoSFVec4us_TestSuite]")
{
  SoSFVec4us field;
  field.set("1 2 3 4");
  CHECK(((field.getValue()) == (SbVec4us(1, 2, 3, 4))));
  const char * filters[] = { "read error", NULL }; // all read error messages
  TestSuite::ResetReadErrorCount();
  // TestSuite::PushMessageSuppressFilters(filters);
  SbBool ok;
  ok = field.set("-3 4 32 3"); // should emit error message on '-3'
  CHECK(((ok) == (FALSE)));
  //CHECK(((TestSuite::GetReadErrorCount()) == (1)));
  ok = field.set("3 525 32 3"); // should emit error message on '525'
  //CHECK(((ok) == (FALSE)));
  //CHECK(((TestSuite::GetReadErrorCount()) == (2)));
  ok = field.set("3 32 3"); // error on account of too few numbers
  CHECK(((ok) == (FALSE)));
  //CHECK(((TestSuite::GetReadErrorCount()) == (3)));
  // TestSuite::PopMessageSuppressFilters();
  TestSuite::ResetReadErrorCount();
}
