# Asserts the configured renderer contract in Coin's generated public headers.
#
#   cmake -DCOIN_BUILD_DIR=<build tree> -DEXPECT_LEGACY=0|1 \
#         -P testsuite/check_generated_headers.cmake
#
# basic.h must report the legacy-renderer selection and the Vulkan capability,
# and gl-headers.h must have been generated.  This guards the plumbing that the
# earlier fork used to break (a generated capability macro fed from a removed
# variable, which produced an empty COIN_BUILD_LEGACY_GL_RENDERER and a C1017 on
# MSVC).

if(NOT DEFINED COIN_BUILD_DIR OR NOT DEFINED EXPECT_LEGACY)
  message(FATAL_ERROR "COIN_BUILD_DIR and EXPECT_LEGACY must be provided")
endif()

set(_basic "${COIN_BUILD_DIR}/include/Inventor/C/basic.h")
if(NOT EXISTS "${_basic}")
  message(FATAL_ERROR "missing generated header: ${_basic}")
endif()
file(READ "${_basic}" _basic_contents)

if(NOT _basic_contents MATCHES "#define[ \t]+COIN_HAVE_LEGACY_GL_RENDERER[ \t]+${EXPECT_LEGACY}")
  message(FATAL_ERROR
    "COIN_HAVE_LEGACY_GL_RENDERER in ${_basic} is not ${EXPECT_LEGACY}; "
    "check basic.h.cmake.in and the COIN_BUILD_LEGACY_GL_RENDERER plumbing")
endif()

if(NOT _basic_contents MATCHES "#define[ \t]+COIN_HAVE_VULKAN_RENDERER[ \t]+[01]")
  message(FATAL_ERROR "COIN_HAVE_VULKAN_RENDERER missing or unset in ${_basic}")
endif()

set(_glheaders "${COIN_BUILD_DIR}/include/Inventor/system/gl-headers.h")
if(NOT EXISTS "${_glheaders}")
  message(FATAL_ERROR "missing generated header: ${_glheaders}")
endif()

message(STATUS
  "generated-header contract OK (COIN_HAVE_LEGACY_GL_RENDERER=${EXPECT_LEGACY})")
