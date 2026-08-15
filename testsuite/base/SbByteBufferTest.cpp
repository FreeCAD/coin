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

#include <Inventor/SbByteBuffer.h>

BOOST_AUTO_TEST_SUITE(SbByteBuffer_TestSuite);

BOOST_AUTO_TEST_CASE(pushUnique)
{

  static const char A [] = "ABC";
  static const char B [] = "XYZ";
  static char C [sizeof(A)+sizeof(B)-1];
  strcpy(C,A);
  strcat(C,B);

  SbByteBuffer a(sizeof(A)-1,A);
  SbByteBuffer b(sizeof(B)-1,B);
  SbByteBuffer c=a;

  c.push(b);

  bool allOk=true;
  for (size_t i=0;i<sizeof(A)-1;++i) {
    if (a[i]!=A[i])
      allOk=false;
  }
  for (size_t i=0;i<sizeof(B)-1;++i) {
    if (b[i]!=B[i])
      allOk=false;
  }
  BOOST_CHECK_MESSAGE(c.size()==sizeof(C)-1,"Concatenation does not have correct size");
  for (size_t i=0;i<sizeof(C)-1;++i) {
    if (c[i]!=C[i])
      allOk=false;
  }

  BOOST_CHECK_MESSAGE(allOk,
                      std::string("Concatenation of ") + A + " and " + B + " is not " + C
                      );
}

BOOST_AUTO_TEST_CASE(pushOnEmpty)
{
  SbByteBuffer a;
  SbByteBuffer b("foo");

  BOOST_CHECK_MESSAGE(a.empty(),
                      std::string("Size of empty buffer is") + ::CoinTest::stringify(a.size())
                      );

  a.push(b);

  BOOST_CHECK_MESSAGE(a.size() == b.size(),
                      "Size of buffers differ"
                      );

  for (size_t i=0; i < b.size(); ++i) {
    if(a[i]!=b[i]) {
      printf("Mjau %lu: %c != %c \n",(unsigned long)i,a[i],b[i]);
    }
  }

  BOOST_CHECK_MESSAGE(a == b,
                      "Byte representations differ"
                      );

}

BOOST_AUTO_TEST_SUITE_END();
