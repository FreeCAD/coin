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


#include <cassert>
#include <Inventor/SbDPLine.h>
#include <Inventor/SbDPPlane.h>
#include <Inventor/SbLine.h>
#include <Inventor/SbPlane.h>
#include <Inventor/SbTypeInfo.h>
#include <cfloat>
#include <algorithm>
#include <cmath>
BOOST_AUTO_TEST_SUITE(SbDPPlane_TestSuite);


using namespace SIM::Coin::TestSuite;

float slew(float Start, float End, int steps, int step) {
  const float S = log(Start<0?-Start:Start);
  const float E = log(End<0?-End:End);

  --steps;

  float res=0;

  assert(Start<End);

  if (Start<0 && End < 0 ) {
    res=-exp(S+step*(E-S)/steps);
  }
  else if(Start>0 && End>0) {
    res=exp(S+step*(E-S)/steps);
  }
  else {
    float ZeroPoint = S*steps/(E+S);
    if(step<ZeroPoint) {
      res=-exp(S-2*step*S/ZeroPoint);
    }
    else if (step==ZeroPoint) {
      res=0;
    }
    else {
      res=exp(2*E*(step-ZeroPoint)/(steps-ZeroPoint)-E);
    }
  }

  return res;
}

BOOST_AUTO_TEST_CASE(signCorrect)
{
  SbDPPlane plane1(SbVec3d(0.0, 0.0, 1.0), 3.0);
  SbDPPlane plane2(SbVec3d(1.0, 0.0, 0.0), 21.0);
  SbDPLine line;
  plane1.intersect(plane2, line);

  SbVec3d intersect = line.getPosition();
  SbVec3d vec(21, 0, 3);

  check_compare(intersect,vec, "SbDPPlane SignCorrect", .1f);
}

BOOST_AUTO_TEST_CASE(equalityToFloatPlane)
{
  const float delX = 1;
  const float delY = .1f;

  const float XMax = (float)pow(2.,FLT_MAX_EXP/3.);
  const float XMin = -XMax;

  const float YMax = (float)pow(2.,FLT_MAX_EXP/3.);
  const float YMin = -YMax;

#ifdef TEST_SUITE_QUICK
  const int XSteps = 4;
  const int YSteps = 4;
#endif //TEST_SUITE_QUICK
#ifdef TEST_SUITE_THOROUG
  const int XSteps = 6;
  const int YSteps = 6;
#endif //TEST_SUITE_THOROUG
#ifdef TEST_SUITE_EXPANSIVE
  const int XSteps = 10;
  const int YSteps = 10;
#endif //TEST_SUITE_EXPANSIVE

  int count=0;

  for (int x1=0;x1<XSteps;++x1) {
    float X1=slew(XMin,XMax,XSteps,x1);
    for (int x2=0;x2<XSteps;++x2) {
      float X2=slew(XMin,XMax,XSteps,x2);
      for (int x3=0;x3<XSteps;++x3) {
        float X3=slew(XMin,XMax,XSteps,x3);
        SbVec3f fv1(X1,X2,X3);
        SbVec3d dv1(X1,X2,X3);

        for (int x4=0;x4<XSteps;++x4) {
          float X4=slew(XMin,XMax,XSteps,x4);

          SbPlane fp1(fv1,X4);
          SbDPPlane dp1(dv1,X4);

          check_compare(fp1.getDistanceFromOrigin(),dp1.getDistanceFromOrigin(), "Distance from origin differs", 64);
          check_compare(fp1.getNormal(),dp1.getNormal(),"Comparing normals yields different results",.000001f);
          for (int y1=0;y1<YSteps;++y1) {
            float Y1=slew(YMin,YMax,YSteps,y1);
            for (int y2=0;y2<YSteps;++y2) {
              float Y2=slew(YMin,YMax,YSteps,y2);
              for (int y3=0;y3<YSteps;++y3) {
                float Y3=slew(YMin,YMax,YSteps,y3);
                SbVec3f fv2(Y1,Y2,Y3);
                SbVec3d dv2(Y1,Y2,Y3);

                //A bit arbitrary, this holds
                const float tol = .03f;
                BOOST_CHECK_MESSAGE(
                                    floatEquals(fp1.getDistance(fv2),(float)dp1.getDistance(dv2),tol)||
                                    fabs(fp1.getDistance(fv2)-dp1.getDistance(dv2))/fabs(dp1.getDistanceFromOrigin())<tol,
                                    "Distance from plane is significantly different");
                for (int y4=0;y4<YSteps;++y4) {
                  float Y4=slew(YMin,YMax,YSteps,y3);
                  SbPlane fp2(fv2,Y4);
                  SbDPPlane dp2(dv2,Y4);

                  SbLine fLine;
                  SbDPLine dLine;
                  bool failed=false;
                  if (!fp1.intersect(fp2, fLine)) {
                    failed = true;
                  }
                  if (!dp1.intersect(dp2, dLine)) {
                    BOOST_CHECK_MESSAGE(failed,"Float intersection worked, but double intersection failed");
                    failed = true;
                  }
                  if (failed)
                    continue;


                  SbVec3f fDir(fLine.getDirection());
                  SbVec3d dDir(dLine.getDirection());

                  check_compare(fDir,dDir, "Intersection direction differs", .004f);
                }
              }
            }
          }
        }
      }
    }
  }

}


BOOST_AUTO_TEST_SUITE_END();
