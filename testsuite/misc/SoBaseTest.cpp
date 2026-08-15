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


#include <Inventor/misc/SoBase.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoToVRML2Action.h>
#include <Inventor/VRMLnodes/SoVRMLGroup.h>
BOOST_AUTO_TEST_SUITE(SoBase_TestSuite);


/*This test targets the implementation of SoWriterefCounter::getWriteName
  in the source file SoWriterefCounter.cpp. Test is put here because the
  SoWriterefCounter class is not part of the public API. It is being called
  from here
  */

 static char * buffer;
  static size_t buffer_size = 0;

//
// If function is copied from SoWriterefCounter.cpp as pricate classes cannot be called from the test suite
//
  static SbBool
dont_mangle_output_names(const SoBase *base)
{
  static int COIN_DONT_MANGLE_OUTPUT_NAMES = -1;

  // Always unmangle node names in VRML1 and VRML2
  if (base->isOfType(SoNode::getClassTypeId()) &&
      (((SoNode *)base)->getNodeType()==SoNode::VRML1 ||
     ((SoNode *)base)->getNodeType()==SoNode::VRML2)) return TRUE;

  if (COIN_DONT_MANGLE_OUTPUT_NAMES < 0) {
    COIN_DONT_MANGLE_OUTPUT_NAMES = 0;
    const char * env = coin_getenv("COIN_DONT_MANGLE_OUTPUT_NAMES");
    if (env) COIN_DONT_MANGLE_OUTPUT_NAMES = atoi(env);
  }
  return COIN_DONT_MANGLE_OUTPUT_NAMES ? TRUE : FALSE;
}

  static void *
  buffer_realloc(void * bufptr, size_t size)
  {
    buffer = (char *)realloc(bufptr, size);
    buffer_size = size;
    return buffer;
  }


BOOST_AUTO_TEST_CASE(checkWriteWithMultiref)
{
	SoDB::init();
	   SoNode* scenegraph;
       SoSeparator *root = new SoSeparator;
       root->ref();
       root->setName("root");
		scenegraph = root;
       SoSeparator *n0 = new SoSeparator;
       SoSeparator *a0 = new SoSeparator;
       SoSeparator *a1 = new SoSeparator;
       SoSeparator *a2 = new SoSeparator;
       SoSeparator *a3 = new SoSeparator;
       SoSeparator *b0 = new SoSeparator;
       SoSeparator *b1 = new SoSeparator;
       SoSeparator *b2 = new SoSeparator;
       SoSeparator *b3 = new SoSeparator;
       SoSeparator *b4 = new SoSeparator;
       SoSeparator *c0 = new SoSeparator;

       a2->setName(SbName("MyName"));
       b0->setName(SbName("MyName"));
       b1->setName(SbName("MyName"));
       b2->setName(SbName("MyName"));
       b4->setName(SbName("MyName"));
       c0->setName(SbName("MyName"));

       root->addChild(n0);
       root->addChild(n0);
       root->addChild(a0);
       a0->addChild(b0);
       a0->addChild(b1);
       root->addChild(b0);
       root->addChild(a1);
       a1->addChild(b2);
       a1->addChild(b1);
       root->addChild(b1);
       root->addChild(a2);
       root->addChild(a2);
       root->addChild(a3);
       a3->addChild(b3);
       b3->addChild(c0);
       b3->addChild(c0);
       a3->addChild(b4);
       a3->addChild(a2);



/*
       Correct output file if dont_mangle_names:

#VRML V1.0 ascii

VRMLGroup {
  children
    DEF root VRMLGroup {
      children [
        DEF _+0 VRMLGroup {
        },
        USE _+0,
        DEF _+1 VRMLGroup {
          children [
            DEF MyName VRMLGroup {
            },
            DEF MyName+2 VRMLGroup {
            } ]
        },
        USE MyName,
        DEF _+3 VRMLGroup {
          children [
            DEF MyName+4 VRMLGroup {
            },
            USE MyName+2 ]
        },
        USE MyName+2,
        DEF MyName+5 VRMLGroup {
        },
        USE MyName+5,
        DEF _+6 VRMLGroup {
          children [
            DEF _+7 VRMLGroup {
              children [
                DEF MyName+8 VRMLGroup {
                },
                USE MyName+8 ]
            },
            DEF MyName+9 VRMLGroup {
            },
            USE MyName+5 ]
        } ]
    }
}
    */

	   /* correct outputfile for default OIV behavior:
#VRML V1.0 ascii

DEF root Separator {
  DEF _+0 Separator {
  }
  USE _+0
  Separator {
    DEF MyName Separator {
    }
    DEF MyName+1 Separator {
    }
  }
  USE MyName
  Separator {
    DEF MyName+2 Separator {
    }
    USE MyName+1
  }
  USE MyName+1
  DEF MyName+3 Separator {
  }
  USE MyName+3
  Separator {
    Separator {
      DEF MyName+4 Separator {
      }
      USE MyName+4
    }
    DEF MyName+5 Separator {
    }
    USE MyName+3
  }
}
*/

#ifdef HAVE_VRML97
	    SoVRMLGroup *newroot;
	   for(int j=0;j<2;j++) {
		   if(j==1) {
	SoToVRML2Action tovrml2;
      tovrml2.apply(root);
      newroot = tovrml2.getVRML2SceneGraph();
	  newroot->ref();
	  scenegraph = newroot;
		   }


	   buffer = (char*)malloc(1024);
       SoOutput out;
       // out.openFile("out.wrl");
	   out.setBuffer(buffer, 1024,buffer_realloc);
       out.setHeaderString(SbString("#VRML V1.0 ascii"));
       SoWriteAction wra(&out);
       wra.apply(scenegraph);
	   SbString s(buffer);
	   free(buffer);

	   int pos;
	   SbString ss;

	   SbList<SbString> node_names(15);
	   if(j==1)
		BOOST_CHECK_MESSAGE(dont_mangle_output_names(scenegraph)==TRUE,"don't mangle should be TRUE");
	   else
		BOOST_CHECK_MESSAGE(dont_mangle_output_names(scenegraph)==FALSE,"don't mangle should be FALSE");

	   if(dont_mangle_output_names(scenegraph)) {
		   node_names.append("_+0");
		   node_names.append("_+0");
		   node_names.append("_+1");
		   node_names.append("MyName");
		   node_names.append("MyName+2");
		   node_names.append("MyName");
		   node_names.append("_+3");
		   node_names.append("MyName+4");
		   node_names.append("MyName+2");
		   node_names.append("MyName+2");
		   node_names.append("MyName+5");
		   node_names.append("MyName+5");
		   node_names.append("_+6");
		   node_names.append("_+7");
		   node_names.append("MyName+8");
		   node_names.append("MyName+8");
		   node_names.append("MyName+9");
		   node_names.append("MyName+5");
	   }
	   else {
		   node_names.append("_+0");
		   node_names.append("_+0");
		   node_names.append("MyName");
		   node_names.append("MyName+1");
		   node_names.append("MyName");
		   node_names.append("MyName+2");
		   node_names.append("MyName+1");
		   node_names.append("MyName+1");
		   node_names.append("MyName+3");
		   node_names.append("MyName+3");
		   node_names.append("MyName+4");
		   node_names.append("MyName+4");
		   node_names.append("MyName+5");
		   node_names.append("MyName+3");
	   }

	   int list_index = 0;
	   SbBool fail = false;
	   ss = s;
	   for(int i=0;i<node_names.getLength();i++) {
		   pos = ss.find(node_names[i]);
		   if(pos==-1) {
			   fail = true;
			   break;
		   }
		   ss = ss.getSubString(pos+node_names[i].getLength(),ss.getLength());
	   }


	   BOOST_CHECK_MESSAGE(!fail,"Check failed, written node names should match test template");

	   }


       root->unref();
	   newroot->unref();
#endif
 }


BOOST_AUTO_TEST_SUITE_END();
