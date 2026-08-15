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


#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/errors/SoReadError.h>
#include <Inventor/fields/SoMFNode.h>
#include <Inventor/fields/SoSFTime.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoRotationXYZ.h>
BOOST_AUTO_TEST_SUITE(SoDB_TestSuite);


BOOST_AUTO_TEST_CASE(globalRealTimeField)
{
  //Need to do this here, since we do not have any manager that calls it for us.
  SoDB::getSensorManager()->processTimerQueue();
  SoSFTime * realtime = (SoSFTime *)SoDB::getGlobalField("realTime");

  BOOST_REQUIRE(realtime != NULL);
  BOOST_REQUIRE(realtime->getContainer() != NULL);

  // check that realtime field actually is initialized with something
  // close to actual time
  const double clockdiff =
    fabs(SbTime::getTimeOfDay().getValue() - realtime->getValue().getValue());
  BOOST_CHECK_MESSAGE(clockdiff < 5.0,
                      "realTime global field not close to actual time");
}

// Do-nothing error handler for ignoring read errors while testing.
static void
readErrorHandler(const SoError * error, void * data)
{
}

#ifdef HAVE_VRML97
BOOST_AUTO_TEST_CASE(readChildList)
{
  static const char scene[] = "#VRML V2.0 utf8\n"
                              "DEF TestGroup Group { children [Group{}, Group{}, Group{}] }";
  SoInput in;
  in.setBuffer(scene, strlen(scene));
  SoSeparator * root = SoDB::readAll(&in);
  BOOST_REQUIRE(root);
  root->ref();
  SoGroup * group = (SoGroup *) SoNode::getByName("TestGroup");
  BOOST_REQUIRE_MESSAGE(group->getNumChildren() == 3, "Unexpected number of children");
  for (int i = 0; i < group->getNumChildren(); i++) {
    BOOST_REQUIRE_MESSAGE(group->getChild(i)->isOfType(SoGroup::getClassTypeId()), "Unexpected type");
  }
  root->unref();
}
#endif

BOOST_AUTO_TEST_CASE(readEmptyChildList)
{
  // FIXME: We are forced to restore the global state before terminating,
  // or independent tests could fail. (sveinung 20071108)
  SoErrorCB * prevErrorCB = SoReadError::getHandlerCallback();
  SoReadError::setHandlerCallback(readErrorHandler, NULL);

  static const char scene[] = "#VRML V2.0 utf8\n"
                              "DEF TestGroup Group { children }";
  SoInput in;
  in.setBuffer(scene, strlen(scene));
  SoSeparator * root = SoDB::readAll(&in);
  if (root) {
    SoGroup * group = (SoGroup *) SoNode::getByName("TestGroup");
    BOOST_CHECK_MESSAGE(group->getNumChildren() == 0, "Should have no children");
  }
  BOOST_CHECK_MESSAGE(root == NULL, "Expected the import to fail");

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

BOOST_AUTO_TEST_CASE(readNullChildList)
{
  // FIXME: We are forced to restore the global state before terminating,
  // or independent tests could fail. (sveinung 20071108)
  SoErrorCB * prevErrorCB = SoReadError::getHandlerCallback();
  SoReadError::setHandlerCallback(readErrorHandler, NULL);

  static const char scene[] = "#VRML V2.0 utf8\n"
                              "PROTO Object [ field MFNode testChildren NULL ] { }\n"
                              "DEF TestObject Object { }";
  SoInput in;
  in.setBuffer(scene, strlen(scene));
  SoSeparator * root = SoDB::readAll(&in);
  if (root) {
    SoNode * object = (SoNode *) SoNode::getByName("TestObject");
    SoMFNode * field = (SoMFNode *) object->getField("testChildren");
    BOOST_CHECK_MESSAGE(field->getNumNodes() == 0, "Should have no children");
  }
  BOOST_CHECK_MESSAGE(root == NULL, "Expected the import to fail");

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

BOOST_AUTO_TEST_CASE(readInvalidChildList)
{
  // FIXME: We are forced to restore the global state before terminating,
  // or independent tests could fail. (sveinung 20071108)
  SoErrorCB * prevErrorCB = SoReadError::getHandlerCallback();
  SoReadError::setHandlerCallback(readErrorHandler, NULL);

  static const char scene[] = "#VRML V2.0 utf8\n"
                              "Group { children[0] }";
  SoInput in;
  in.setBuffer(scene, strlen(scene));
  SoSeparator * root = SoDB::readAll(&in);
  BOOST_CHECK_MESSAGE(root == NULL, "Expected the import to fail");

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

BOOST_AUTO_TEST_CASE(testAlternateRepNull)
{
  // FIXME: We are forced to restore the global state before terminating,
  // or independent tests could fail. (sveinung 20071108)
  SoErrorCB * prevErrorCB = SoReadError::getHandlerCallback();
  SoReadError::setHandlerCallback(readErrorHandler, NULL);

  static const char scene[] = "#Inventor V2.1 ascii\n"
                              "ExtensionNode { fields [ SFNode alternateRep ] }";
  SoInput in;
  in.setBuffer(scene, strlen(scene));
  SoSeparator * root = SoDB::readAll(&in);
  BOOST_CHECK_MESSAGE(root, "Import should succeed");
  root->ref();
  root->unref();

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

BOOST_AUTO_TEST_CASE(testInitCleanup)
{
  // init already called
  SoDB::finish();

  SoDB::init();
  SoDB::finish();


  //FIXME: This is not a true unittest, as it touches state for other functions.
  // init for the conntinuing test running
  SoDB::init();
  SoNodeKit::init();
  SoInteraction::init();
  SIM::Coin3D::Coin::TestSuite::Init();

}

// *************************************************************************

// Tests whether or not our mechanisms with the realTime global field
// works correctly upon references to it in imported iv-files.
//
// (This test might be better suited in the SoGlobalField.cpp file,
// but that code doesn't implement a public API part, which causes
// hickups for the automated test-suite code-grabbing & setup.)
//
// -mortene

BOOST_AUTO_TEST_CASE(globalfield_import)
{
  char scene[] =
    "#Inventor V2.1 ascii\n\n"
    "DEF rotnode RotationXYZ {"
    "   angle = GlobalField {"
    "      type \"SFTime\""
    "      realTime 0"
    "   }"
    "   . realTime "
    "}";

  SoInput * in = new SoInput;
  in->setBuffer(scene, strlen(scene));
  SoNode * g = NULL;
  const SbBool readok = SoDB::read(in, g);

  delete in;

  // just to see that we're correct with the syntax
  BOOST_CHECK_MESSAGE(readok,
                      "failed to read scene graph with realTime global field");
  if (!readok) { return; }

  g->ref();

  SoSFTime * realtime = (SoSFTime *)SoDB::getGlobalField("realTime");

  // supposed to get new value from file
  BOOST_CHECK_MESSAGE(realtime->getValue().getValue() == 0.0,
                      "realTime global field value not updated from imported value");

  SoRotationXYZ * r = (SoRotationXYZ *)
    SoBase::getNamedBase("rotnode", SoRotationXYZ::getClassTypeId());
  assert(r);

  // check connection
  SoField * master;
  BOOST_CHECK_MESSAGE((r->angle.getNumConnections() == 1) &&
                      r->angle.getConnectedField(master) &&
                      (master == realtime),
                      "connection to realTime global field not set up properly");

  g->unref();
}

// *************************************************************************


BOOST_AUTO_TEST_SUITE_END();
