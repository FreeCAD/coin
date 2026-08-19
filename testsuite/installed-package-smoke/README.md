# Installed package smoke test

This project verifies Coin's installed CMake package and public headers from
outside the Coin build tree. It is configured after Coin has been installed,
so it is intentionally not added to the main `testsuite` directory with
`add_subdirectory()`.

The continuous-integration workflow configures it with `CMAKE_PREFIX_PATH`
pointing at the temporary Coin installation, then builds the resulting
consumer executable.
