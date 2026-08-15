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


#include <Inventor/fields/SoSFBool.h>

TEST_CASE("SoSFBool_TestSuite.SoSFBool_initialized", "[SoSFBool_TestSuite]")
{
  SoSFBool field;
  do { INFO("SoSFBool class not initialized"); CHECK((SoSFBool::getClassTypeId() != SoType::badType())); } while (false);
  do { INFO("missing class initialization"); CHECK((field.getTypeId() != SoType::badType())); } while (false);
}

TEST_CASE("SoSFBool_TestSuite.textinput", "[SoSFBool_TestSuite]")
{
  SbBool ok;
  SoSFBool field;
  ok = field.set("TRUE");
  do { INFO("did not accept 'TRUE'"); CHECK((ok == TRUE)); } while (false);
  CHECK(((field.getValue()) == (TRUE)));
  ok = field.set("FALSE");
  do { INFO("did not accept 'FALSE'"); CHECK((ok == TRUE)); } while (false);
  CHECK(((field.getValue()) == (FALSE)));

  TestSuite::ResetReadErrorCount();
  static const char * filters[] = { "Invalid value", NULL };
  TestSuite::PushMessageSuppressFilters(filters);
  ok = field.set("MAYBE"); // emits two error messages
  do { INFO("did accept 'MAYBE'"); CHECK((ok == FALSE)); } while (false);
  do { INFO("did not emit error"); CHECK((TestSuite::GetReadErrorCount() == 1)); } while (false);
  TestSuite::PopMessageSuppressFilters();
  TestSuite::ResetReadErrorCount();

  ok = field.set("0");
  do { INFO("did not accept '0'"); CHECK((ok == TRUE)); } while (false);
  CHECK(((field.getValue()) == (FALSE)));
  ok = field.set("1");
  do { INFO("did not accept '1'"); CHECK((ok == TRUE)); } while (false);
  CHECK(((field.getValue()) == (TRUE)));

  static const char * filters2[] = { "Illegal value", NULL };
  TestSuite::PushMessageSuppressFilters(filters2);
  ok = field.set("2");
  do { INFO("did accept '2'"); CHECK((ok == FALSE)); } while (false);
  TestSuite::PopMessageSuppressFilters();
  TestSuite::ResetReadErrorCount();
}
