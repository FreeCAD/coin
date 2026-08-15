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



#include <Inventor/nodes/SoShaderParameter.h>

BOOST_AUTO_TEST_SUITE(SoShaderParameter_TestSuite);


BOOST_AUTO_TEST_CASE(SoShaderParameter_initialized)
{
  {
    SoShaderParameter1f * parameter1f = new SoShaderParameter1f;
    assert(parameter1f);
    parameter1f->ref();
    BOOST_CHECK_MESSAGE(parameter1f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter1f->unref();
  }
  {
    SoShaderParameter1i * parameter1i = new SoShaderParameter1i;
    assert(parameter1i);
    parameter1i->ref();
    BOOST_CHECK_MESSAGE(parameter1i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter1i->unref();
  }
  {
    SoShaderParameter2f * parameter2f = new SoShaderParameter2f;
    assert(parameter2f);
    parameter2f->ref();
    BOOST_CHECK_MESSAGE(parameter2f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter2f->unref();
  }
  {
    SoShaderParameter2i * parameter2i = new SoShaderParameter2i;
    assert(parameter2i);
    parameter2i->ref();
    BOOST_CHECK_MESSAGE(parameter2i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter2i->unref();
  }
  {
    SoShaderParameter3f * parameter3f = new SoShaderParameter3f;
    assert(parameter3f);
    parameter3f->ref();
    BOOST_CHECK_MESSAGE(parameter3f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter3f->unref();
  }
  {
    SoShaderParameter3i * parameter3i = new SoShaderParameter3i;
    assert(parameter3i);
    parameter3i->ref();
    BOOST_CHECK_MESSAGE(parameter3i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter3i->unref();
  }
  {
    SoShaderParameter4f * parameter4f = new SoShaderParameter4f;
    assert(parameter4f);
    parameter4f->ref();
    BOOST_CHECK_MESSAGE(parameter4f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter4f->unref();
  }
  {
    SoShaderParameter4i * parameter4i = new SoShaderParameter4i;
    assert(parameter4i);
    parameter4i->ref();
    BOOST_CHECK_MESSAGE(parameter4i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parameter4i->unref();
  }

  {
    SoShaderParameterArray1f * parametera1f = new SoShaderParameterArray1f;
    assert(parametera1f);
    parametera1f->ref();
    BOOST_CHECK_MESSAGE(parametera1f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera1f->unref();
  }
  {
    SoShaderParameterArray1i * parametera1i = new SoShaderParameterArray1i;
    assert(parametera1i);
    parametera1i->ref();
    BOOST_CHECK_MESSAGE(parametera1i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera1i->unref();
  }
  {
    SoShaderParameterArray2f * parametera2f = new SoShaderParameterArray2f;
    assert(parametera2f);
    parametera2f->ref();
    BOOST_CHECK_MESSAGE(parametera2f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera2f->unref();
  }
  {
    SoShaderParameterArray2i * parametera2i = new SoShaderParameterArray2i;
    assert(parametera2i);
    parametera2i->ref();
    BOOST_CHECK_MESSAGE(parametera2i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera2i->unref();
  }
  {
    SoShaderParameterArray3f * parametera3f = new SoShaderParameterArray3f;
    assert(parametera3f);
    parametera3f->ref();
    BOOST_CHECK_MESSAGE(parametera3f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera3f->unref();
  }
  {
    SoShaderParameterArray3i * parametera3i = new SoShaderParameterArray3i;
    assert(parametera3i);
    parametera3i->ref();
    BOOST_CHECK_MESSAGE(parametera3i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera3i->unref();
  }
  {
    SoShaderParameterArray4f * parametera4f = new SoShaderParameterArray4f;
    assert(parametera4f);
    parametera4f->ref();
    BOOST_CHECK_MESSAGE(parametera4f->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera4f->unref();
  }
  {
    SoShaderParameterArray4i * parametera4i = new SoShaderParameterArray4i;
    assert(parametera4i);
    parametera4i->ref();
    BOOST_CHECK_MESSAGE(parametera4i->getTypeId() != SoType::badType(),
                        "missing class initialization");
    parametera4i->unref();
  }

  {
    SoShaderParameterMatrix * matrix = new SoShaderParameterMatrix;
    assert(matrix);
    matrix->ref();
    BOOST_CHECK_MESSAGE(matrix->getTypeId() != SoType::badType(),
                        "missing class initialization");
    matrix->unref();
  }
  {
    SoShaderParameterMatrixArray * matrixarray = new SoShaderParameterMatrixArray;
    assert(matrixarray);
    matrixarray->ref();
    BOOST_CHECK_MESSAGE(matrixarray->getTypeId() != SoType::badType(),
                        "missing class initialization");
    matrixarray->unref();
  }

  {
    SoShaderStateMatrixParameter * statematrix = new SoShaderStateMatrixParameter;
    assert(statematrix);
    statematrix->ref();
    BOOST_CHECK_MESSAGE(statematrix->getTypeId() != SoType::badType(),
                        "missing class initialization");
    statematrix->unref();
  }
}


BOOST_AUTO_TEST_SUITE_END();
