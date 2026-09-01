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


#include <Inventor/SbVec3s.h>
#include <Inventor/SbTypeInfo.h>

typedef SbVec3s ToTest;
TEST_CASE("SbVec3s_TestSuite.toString", "[SbVec3s_TestSuite]") {
  ToTest val(1,2,3);
  SbString str("1 2 3");
  do { INFO(std::string("Mismatch between ") +  val.toString().getString() + " and control string " + str.getString()); CHECK((str == val.toString())); } while (false);
}

TEST_CASE("SbVec3s_TestSuite.fromString", "[SbVec3s_TestSuite]") {
  ToTest foo;
  SbString test = "1 -2 3";
  ToTest trueVal(1,-2,3);
  foo.fromString(test);
  do { INFO(std::string("Mismatch between ") +  foo.toString().getString() + " and control " + trueVal.toString().getString()); CHECK((trueVal == foo)); } while (false);
}

TEST_CASE("SbVec3s_TestSuite.fromInvalidString", "[SbVec3s_TestSuite]") {
  ToTest foo;
  SbString test = "a,2,3";
  SbBool conversionOk = foo.fromString(test);
  do { INFO(std::string("Able to convert from ") + test.getString() + " which is not a valid " + SbTypeInfo<ToTest>::getTypeName() + " representation"); CHECK((conversionOk == FALSE)); } while (false);
}
