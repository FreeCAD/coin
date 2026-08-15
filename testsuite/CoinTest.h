#ifndef COIN_TESTSUITE_COINTTEST_H
#define COIN_TESTSUITE_COINTTEST_H

#include <catch2/catch.hpp>

#include <sstream>
#include <string>

namespace CoinTest {

// Keep the string conversion helper used by existing Coin test diagnostics.
template <typename T>
inline std::string stringify(const T & value)
{
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

} // namespace CoinTest

#endif // !COIN_TESTSUITE_COINTTEST_H
