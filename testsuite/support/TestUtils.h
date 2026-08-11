#ifndef COIN_TEST_TESTUTILS_H
#define COIN_TEST_TESTUTILS_H

#include <iostream>

#ifndef COIN_TEST_SKIP_RETURN_CODE
#error COIN_TEST_SKIP_RETURN_CODE must match the CTest SKIP_RETURN_CODE property
#endif

namespace coin_test {

inline int
skip(const char * reason)
{
  std::cout << "SKIP: " << reason << std::endl;
  return COIN_TEST_SKIP_RETURN_CODE;
}

inline int
fail(const char * message)
{
  std::cerr << "FAIL: " << message << std::endl;
  return 1;
}

inline bool
check(const bool condition, const char * message)
{
  if (!condition) fail(message);
  return condition;
}

} // namespace coin_test

#endif // COIN_TEST_TESTUTILS_H
