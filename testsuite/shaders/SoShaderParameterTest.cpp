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

TEST_CASE("SoShaderParameter_TestSuite.SoShaderParameter_initialized", "[SoShaderParameter_TestSuite]")
{
  {
    SoShaderParameter1f * parameter1f = new SoShaderParameter1f;
    assert(parameter1f);
    parameter1f->ref();
    do { INFO("missing class initialization"); CHECK((parameter1f->getTypeId() != SoType::badType())); } while (false);
    parameter1f->unref();
  }
  {
    SoShaderParameter1i * parameter1i = new SoShaderParameter1i;
    assert(parameter1i);
    parameter1i->ref();
    do { INFO("missing class initialization"); CHECK((parameter1i->getTypeId() != SoType::badType())); } while (false);
    parameter1i->unref();
  }
  {
    SoShaderParameter2f * parameter2f = new SoShaderParameter2f;
    assert(parameter2f);
    parameter2f->ref();
    do { INFO("missing class initialization"); CHECK((parameter2f->getTypeId() != SoType::badType())); } while (false);
    parameter2f->unref();
  }
  {
    SoShaderParameter2i * parameter2i = new SoShaderParameter2i;
    assert(parameter2i);
    parameter2i->ref();
    do { INFO("missing class initialization"); CHECK((parameter2i->getTypeId() != SoType::badType())); } while (false);
    parameter2i->unref();
  }
  {
    SoShaderParameter3f * parameter3f = new SoShaderParameter3f;
    assert(parameter3f);
    parameter3f->ref();
    do { INFO("missing class initialization"); CHECK((parameter3f->getTypeId() != SoType::badType())); } while (false);
    parameter3f->unref();
  }
  {
    SoShaderParameter3i * parameter3i = new SoShaderParameter3i;
    assert(parameter3i);
    parameter3i->ref();
    do { INFO("missing class initialization"); CHECK((parameter3i->getTypeId() != SoType::badType())); } while (false);
    parameter3i->unref();
  }
  {
    SoShaderParameter4f * parameter4f = new SoShaderParameter4f;
    assert(parameter4f);
    parameter4f->ref();
    do { INFO("missing class initialization"); CHECK((parameter4f->getTypeId() != SoType::badType())); } while (false);
    parameter4f->unref();
  }
  {
    SoShaderParameter4i * parameter4i = new SoShaderParameter4i;
    assert(parameter4i);
    parameter4i->ref();
    do { INFO("missing class initialization"); CHECK((parameter4i->getTypeId() != SoType::badType())); } while (false);
    parameter4i->unref();
  }

  {
    SoShaderParameterArray1f * parametera1f = new SoShaderParameterArray1f;
    assert(parametera1f);
    parametera1f->ref();
    do { INFO("missing class initialization"); CHECK((parametera1f->getTypeId() != SoType::badType())); } while (false);
    parametera1f->unref();
  }
  {
    SoShaderParameterArray1i * parametera1i = new SoShaderParameterArray1i;
    assert(parametera1i);
    parametera1i->ref();
    do { INFO("missing class initialization"); CHECK((parametera1i->getTypeId() != SoType::badType())); } while (false);
    parametera1i->unref();
  }
  {
    SoShaderParameterArray2f * parametera2f = new SoShaderParameterArray2f;
    assert(parametera2f);
    parametera2f->ref();
    do { INFO("missing class initialization"); CHECK((parametera2f->getTypeId() != SoType::badType())); } while (false);
    parametera2f->unref();
  }
  {
    SoShaderParameterArray2i * parametera2i = new SoShaderParameterArray2i;
    assert(parametera2i);
    parametera2i->ref();
    do { INFO("missing class initialization"); CHECK((parametera2i->getTypeId() != SoType::badType())); } while (false);
    parametera2i->unref();
  }
  {
    SoShaderParameterArray3f * parametera3f = new SoShaderParameterArray3f;
    assert(parametera3f);
    parametera3f->ref();
    do { INFO("missing class initialization"); CHECK((parametera3f->getTypeId() != SoType::badType())); } while (false);
    parametera3f->unref();
  }
  {
    SoShaderParameterArray3i * parametera3i = new SoShaderParameterArray3i;
    assert(parametera3i);
    parametera3i->ref();
    do { INFO("missing class initialization"); CHECK((parametera3i->getTypeId() != SoType::badType())); } while (false);
    parametera3i->unref();
  }
  {
    SoShaderParameterArray4f * parametera4f = new SoShaderParameterArray4f;
    assert(parametera4f);
    parametera4f->ref();
    do { INFO("missing class initialization"); CHECK((parametera4f->getTypeId() != SoType::badType())); } while (false);
    parametera4f->unref();
  }
  {
    SoShaderParameterArray4i * parametera4i = new SoShaderParameterArray4i;
    assert(parametera4i);
    parametera4i->ref();
    do { INFO("missing class initialization"); CHECK((parametera4i->getTypeId() != SoType::badType())); } while (false);
    parametera4i->unref();
  }

  {
    SoShaderParameterMatrix * matrix = new SoShaderParameterMatrix;
    assert(matrix);
    matrix->ref();
    do { INFO("missing class initialization"); CHECK((matrix->getTypeId() != SoType::badType())); } while (false);
    matrix->unref();
  }
  {
    SoShaderParameterMatrixArray * matrixarray = new SoShaderParameterMatrixArray;
    assert(matrixarray);
    matrixarray->ref();
    do { INFO("missing class initialization"); CHECK((matrixarray->getTypeId() != SoType::badType())); } while (false);
    matrixarray->unref();
  }

  {
    SoShaderStateMatrixParameter * statematrix = new SoShaderStateMatrixParameter;
    assert(statematrix);
    statematrix->ref();
    do { INFO("missing class initialization"); CHECK((statematrix->getTypeId() != SoType::badType())); } while (false);
    statematrix->unref();
  }
}
