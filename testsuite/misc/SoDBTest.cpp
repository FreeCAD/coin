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

TEST_CASE("SoDB_TestSuite.globalRealTimeField", "[SoDB_TestSuite]")
{
  //Need to do this here, since we do not have any manager that calls it for us.
  SoDB::getSensorManager()->processTimerQueue();
  SoSFTime * realtime = (SoSFTime *)SoDB::getGlobalField("realTime");

  REQUIRE((realtime != NULL));
  REQUIRE((realtime->getContainer() != NULL));

  // check that realtime field actually is initialized with something
  // close to actual time
  const double clockdiff =
    fabs(SbTime::getTimeOfDay().getValue() - realtime->getValue().getValue());
  do { INFO("realTime global field not close to actual time"); CHECK((clockdiff < 5.0)); } while (false);
}

// Do-nothing error handler for ignoring read errors while testing.
static void
readErrorHandler(const SoError * error, void * data)
{
}

#ifdef HAVE_VRML97
TEST_CASE("SoDB_TestSuite.readChildList", "[SoDB_TestSuite]")
{
  static const char scene[] = "#VRML V2.0 utf8\n"
                              "DEF TestGroup Group { children [Group{}, Group{}, Group{}] }";
  SoInput in;
  in.setBuffer(scene, strlen(scene));
  SoSeparator * root = SoDB::readAll(&in);
  REQUIRE((root));
  root->ref();
  SoGroup * group = (SoGroup *) SoNode::getByName("TestGroup");
  do { INFO("Unexpected number of children"); REQUIRE((group->getNumChildren() == 3)); } while (false);
  for (int i = 0; i < group->getNumChildren(); i++) {
    do { INFO("Unexpected type"); REQUIRE((group->getChild(i)->isOfType(SoGroup::getClassTypeId()))); } while (false);
  }
  root->unref();
}
#endif

TEST_CASE("SoDB_TestSuite.readEmptyChildList", "[SoDB_TestSuite]")
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
    do { INFO("Should have no children"); CHECK((group->getNumChildren() == 0)); } while (false);
  }
  do { INFO("Expected the import to fail"); CHECK((root == NULL)); } while (false);

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

TEST_CASE("SoDB_TestSuite.readNullChildList", "[SoDB_TestSuite]")
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
    do { INFO("Should have no children"); CHECK((field->getNumNodes() == 0)); } while (false);
  }
  do { INFO("Expected the import to fail"); CHECK((root == NULL)); } while (false);

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

TEST_CASE("SoDB_TestSuite.readInvalidChildList", "[SoDB_TestSuite]")
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
  do { INFO("Expected the import to fail"); CHECK((root == NULL)); } while (false);

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

TEST_CASE("SoDB_TestSuite.testAlternateRepNull", "[SoDB_TestSuite]")
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
  do { INFO("Import should succeed"); CHECK((root)); } while (false);
  root->ref();
  root->unref();

  SoReadError::setHandlerCallback(prevErrorCB, NULL);
}

TEST_CASE("SoDB_TestSuite.testInitCleanup", "[SoDB_TestSuite]")
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

TEST_CASE("SoDB_TestSuite.globalfield_import", "[SoDB_TestSuite]")
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
  do { INFO("failed to read scene graph with realTime global field"); CHECK((readok)); } while (false);
  if (!readok) { return; }

  g->ref();

  SoSFTime * realtime = (SoSFTime *)SoDB::getGlobalField("realTime");

  // supposed to get new value from file
  do { INFO("realTime global field value not updated from imported value"); CHECK((realtime->getValue().getValue() == 0.0)); } while (false);

  SoRotationXYZ * r = (SoRotationXYZ *)
    SoBase::getNamedBase("rotnode", SoRotationXYZ::getClassTypeId());
  assert(r);

  // check connection
  SoField * master;
  do { INFO("connection to realTime global field not set up properly"); CHECK(((r->angle.getNumConnections() == 1) &&
                      r->angle.getConnectedField(master) &&
                      (master == realtime))); } while (false);

  g->unref();
}

// *************************************************************************
