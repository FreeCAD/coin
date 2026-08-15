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

TEST_CASE("SbBSPTree_TestSuite.SbBSPTree_initialized", "[SbBSPTree_TestSuite]")
{
  SbBSPTree bsp;
  SbVec3f p0(0.0f, 0.0f, 0.0f);
  SbVec3f p1(1.0f, 0.0f, 0.0f);
  SbVec3f p2(2.0f, 0.0f, 0.0f);
  void * userdata0 = reinterpret_cast<void*> (&p0);
  void * userdata1 = reinterpret_cast<void*> (&p1);
  void * userdata2 = reinterpret_cast<void*> (&p2);

  do { INFO("unexpected index"); CHECK((bsp.addPoint(p0, userdata0) == 0)); } while (false);
  do { INFO("unexpected index"); CHECK((bsp.addPoint(p1, userdata1) == 1)); } while (false);
  do { INFO("unexpected index"); CHECK((bsp.addPoint(p2, userdata2) == 2)); } while (false);
  do { INFO("unexpected index"); CHECK((bsp.addPoint(p2, userdata2) == 2)); } while (false);
  do { INFO("wrong number of points in the tree"); CHECK((bsp.numPoints() == 3)); } while (false);

  do { INFO("wrong index"); CHECK((bsp.findPoint(p0) == 0)); } while (false);
  do { INFO("wrong userdata"); CHECK((bsp.getUserData(0) == userdata0)); } while (false);
  do { INFO("wrong index"); CHECK((bsp.findPoint(p1) == 1)); } while (false);
  do { INFO("wrong userdata"); CHECK((bsp.getUserData(1) == userdata1)); } while (false);
  do { INFO("wrong index"); CHECK((bsp.findPoint(p2) == 2)); } while (false);
  do { INFO("wrong userdata"); CHECK((bsp.getUserData(2) == userdata2)); } while (false);

  do { INFO("wrong number of points in the tree"); CHECK((bsp.numPoints() == 3)); } while (false);
  do { INFO("wrong point at index 0"); CHECK((bsp.getPointsArrayPtr()[0] == p0)); } while (false);
  do { INFO("wrong point at index 1"); CHECK((bsp.getPointsArrayPtr()[1] == p1)); } while (false);
  do { INFO("wrong point at index 2"); CHECK((bsp.getPointsArrayPtr()[2] == p2)); } while (false);

  do { INFO("unable to remove point"); CHECK((bsp.removePoint(p1) == 1)); } while (false);
  do { INFO("wrong number of points after removePoint()."); CHECK((bsp.numPoints() == 2)); } while (false);
  do { INFO("wrong point at index 0"); CHECK((bsp.getPointsArrayPtr()[0] == p0)); } while (false);
  do { INFO("wrong userdata"); CHECK((bsp.getUserData(0) == userdata0)); } while (false);
  do { INFO("wrong point at index 1"); CHECK((bsp.getPointsArrayPtr()[1] == p2)); } while (false);
  do { INFO("wrong userdata"); CHECK((bsp.getUserData(1) == userdata2)); } while (false);

  do { INFO("unable to remove point"); CHECK((bsp.removePoint(p0) >= 0)); } while (false);
  do { INFO("unable to remove point"); CHECK((bsp.removePoint(p2) >= 0)); } while (false);
  do { INFO("wrong number of points after removing all points."); CHECK((bsp.numPoints() == 0)); } while (false);

}
