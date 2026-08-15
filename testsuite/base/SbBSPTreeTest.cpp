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


#include <Inventor/SbBSPTree.h>

BOOST_AUTO_TEST_SUITE(SbBSPTree_TestSuite);


BOOST_AUTO_TEST_CASE(SbBSPTree_initialized)
{
  SbBSPTree bsp;
  SbVec3f p0(0.0f, 0.0f, 0.0f);
  SbVec3f p1(1.0f, 0.0f, 0.0f);
  SbVec3f p2(2.0f, 0.0f, 0.0f);
  void * userdata0 = reinterpret_cast<void*> (&p0);
  void * userdata1 = reinterpret_cast<void*> (&p1);
  void * userdata2 = reinterpret_cast<void*> (&p2);

  BOOST_CHECK_MESSAGE(bsp.addPoint(p0, userdata0) == 0, "unexpected index");
  BOOST_CHECK_MESSAGE(bsp.addPoint(p1, userdata1) == 1, "unexpected index");
  BOOST_CHECK_MESSAGE(bsp.addPoint(p2, userdata2) == 2, "unexpected index");
  BOOST_CHECK_MESSAGE(bsp.addPoint(p2, userdata2) == 2, "unexpected index");
  BOOST_CHECK_MESSAGE(bsp.numPoints() == 3, "wrong number of points in the tree");

  BOOST_CHECK_MESSAGE(bsp.findPoint(p0) == 0, "wrong index");
  BOOST_CHECK_MESSAGE(bsp.getUserData(0) == userdata0, "wrong userdata");
  BOOST_CHECK_MESSAGE(bsp.findPoint(p1) == 1, "wrong index");
  BOOST_CHECK_MESSAGE(bsp.getUserData(1) == userdata1, "wrong userdata");
  BOOST_CHECK_MESSAGE(bsp.findPoint(p2) == 2, "wrong index");
  BOOST_CHECK_MESSAGE(bsp.getUserData(2) == userdata2, "wrong userdata");

  BOOST_CHECK_MESSAGE(bsp.numPoints() == 3, "wrong number of points in the tree");
  BOOST_CHECK_MESSAGE(bsp.getPointsArrayPtr()[0] == p0, "wrong point at index 0");
  BOOST_CHECK_MESSAGE(bsp.getPointsArrayPtr()[1] == p1, "wrong point at index 1");
  BOOST_CHECK_MESSAGE(bsp.getPointsArrayPtr()[2] == p2, "wrong point at index 2");

  BOOST_CHECK_MESSAGE(bsp.removePoint(p1) == 1, "unable to remove point");
  BOOST_CHECK_MESSAGE(bsp.numPoints() == 2, "wrong number of points after removePoint().");
  BOOST_CHECK_MESSAGE(bsp.getPointsArrayPtr()[0] == p0, "wrong point at index 0");
  BOOST_CHECK_MESSAGE(bsp.getUserData(0) == userdata0, "wrong userdata");
  BOOST_CHECK_MESSAGE(bsp.getPointsArrayPtr()[1] == p2, "wrong point at index 1");
  BOOST_CHECK_MESSAGE(bsp.getUserData(1) == userdata2, "wrong userdata");

  BOOST_CHECK_MESSAGE(bsp.removePoint(p0) >= 0, "unable to remove point");
  BOOST_CHECK_MESSAGE(bsp.removePoint(p2) >= 0, "unable to remove point");
  BOOST_CHECK_MESSAGE(bsp.numPoints() == 0, "wrong number of points after removing all points.");

}


BOOST_AUTO_TEST_SUITE_END();
