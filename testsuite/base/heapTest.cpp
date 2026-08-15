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


#include <Inventor/C/base/heap.h>
#include <sstream>
BOOST_AUTO_TEST_SUITE(heap_TestSuite);

class mock_up {
public:
  typedef struct wrapped_value {
    double x;
  } wrapped_value;

  static void heap_print_cb(void * v, SbString& str) {
    wrapped_value* value = reinterpret_cast<wrapped_value*>(v);
    std::ostringstream oss;
    oss << value->x;
    str += oss.str().c_str();
  }

  static double heap_evaluate_cb(void * v)
  {
    wrapped_value* value = reinterpret_cast<wrapped_value*>(v);
    return value->x;
  }

  // strict weak ordering is needed for compare callbacks
  static int min_heap_compare_cb(void * lhs, void * rhs)
  {
    return heap_evaluate_cb(lhs) < heap_evaluate_cb(rhs) ? 1 : 0;
  }

  static int max_heap_compare_cb(void * lhs, void * rhs)
  {
    return heap_evaluate_cb(lhs) > heap_evaluate_cb(rhs) ? 1 : 0;
  }
};

BOOST_AUTO_TEST_CASE(min_heap) {
  mock_up::wrapped_value val[] = {3, 2, 1, 15, 5, 4, 45};
  cc_heap* heap = cc_heap_construct(256, reinterpret_cast<cc_heap_compare_cb*>(mock_up::min_heap_compare_cb), TRUE);
  for (int i = 0, n = sizeof(val) / sizeof(val[0]); i < n; ++i)
    cc_heap_add(heap, &val[i]);
  SbString result;
  cc_heap_print(heap, reinterpret_cast<cc_heap_print_cb*>(mock_up::heap_print_cb), result, FALSE);
  cc_heap_destruct(heap);
  heap = NULL;
  SbString str("1 3 2 15 5 4 45 ");
  BOOST_CHECK_MESSAGE(str == result,
    std::string("Mismatch between ") + result.getString() + " and control string " + str.getString());
}

BOOST_AUTO_TEST_CASE(max_heap) {
  mock_up::wrapped_value val[] = {3, 2, 1, 15, 5, 4, 45};
  cc_heap* heap = cc_heap_construct(256, reinterpret_cast<cc_heap_compare_cb*>(mock_up::max_heap_compare_cb), TRUE);
  for (int i = 0, n = sizeof(val) / sizeof(val[0]); i < n; ++i)
    cc_heap_add(heap, &val[i]);
  SbString result;
  cc_heap_print(heap, reinterpret_cast<cc_heap_print_cb*>(mock_up::heap_print_cb), result, FALSE);
  cc_heap_destruct(heap);
  heap = NULL;
  SbString str("45 5 15 2 3 1 4 ");
  BOOST_CHECK_MESSAGE(str == result,
    std::string("Mismatch between ") + result.getString() + " and control string " + str.getString());
}

BOOST_AUTO_TEST_CASE(heap_add) {
  mock_up::wrapped_value val[] = {3, 2, 1, 15, 5, 4, 45};
  cc_heap* heap = cc_heap_construct(256, reinterpret_cast<cc_heap_compare_cb*>(mock_up::min_heap_compare_cb), TRUE);
  for (int i = 0, n = sizeof(val) / sizeof(val[0]); i < n; ++i)
    cc_heap_add(heap, &val[i]);
  double added_value = 12;
  cc_heap_add(heap, &added_value);
  SbString result;
  cc_heap_print(heap, reinterpret_cast<cc_heap_print_cb*>(mock_up::heap_print_cb), result, FALSE);
  cc_heap_destruct(heap);
  heap = NULL;
  SbString str("1 3 2 12 5 4 45 15 ");
  BOOST_CHECK_MESSAGE(str == result,
    std::string("Mismatch between ") + result.getString() + " and control string " + str.getString());
}

BOOST_AUTO_TEST_CASE(heap_remove) {
  mock_up::wrapped_value val[] = {3, 2, 1, 15, 5, 4, 45};
  cc_heap* heap = cc_heap_construct(256, reinterpret_cast<cc_heap_compare_cb*>(mock_up::min_heap_compare_cb), TRUE);
  for (int i = 0, n = sizeof(val) / sizeof(val[0]); i < n; ++i)
    cc_heap_add(heap, &val[i]);
  cc_heap_remove(heap, &val[3]);
  SbString result;
  cc_heap_print(heap, reinterpret_cast<cc_heap_print_cb*>(mock_up::heap_print_cb), result, FALSE);
  cc_heap_destruct(heap);
  heap = NULL;
  SbString str("1 3 2 45 5 4 ");
  BOOST_CHECK_MESSAGE(str == result,
    std::string("Mismatch between ") + result.getString() + " and control string " + str.getString());
}

BOOST_AUTO_TEST_CASE(heap_update) {
  mock_up::wrapped_value val[] = {3, 2, 1, 15, 5, 4, 45};
  cc_heap* heap = cc_heap_construct(256, reinterpret_cast<cc_heap_compare_cb*>(mock_up::min_heap_compare_cb), TRUE);
  for (int i = 0, n = sizeof(val) / sizeof(val[0]); i < n; ++i)
    cc_heap_add(heap, &val[i]);
  val[3].x = 1;
  cc_heap_update(heap, &val[3]);
  SbString result;
  cc_heap_print(heap, reinterpret_cast<cc_heap_print_cb*>(mock_up::heap_print_cb), result, FALSE);
  cc_heap_destruct(heap);
  heap = NULL;
  SbString str("1 1 2 3 5 4 45 ");
  BOOST_CHECK_MESSAGE(str == result,
    std::string("Mismatch between ") + result.getString() + " and control string " + str.getString());
}

BOOST_AUTO_TEST_SUITE_END();
