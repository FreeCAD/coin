param([string]$BuildDir=(Join-Path (Split-Path $PSScriptRoot -Parent) 'coin-preview-build'))
$ErrorActionPreference='Stop'
cmake -S $PSScriptRoot -B $BuildDir -G 'Visual Studio 17 2022' -A x64 -DCMAKE_CXX_STANDARD=20 -DCOIN_BUILD_TESTS=ON -DCOIN_BUILD_VISUAL_TESTS=OFF -DCOIN_FETCH_TEST_DEPENDENCIES=OFF -DCOIN_BUILD_DOCUMENTATION=OFF -DHAVE_SOUND=OFF -DCOIN_BUILD_MSVC_MP=ON
if($LASTEXITCODE){exit $LASTEXITCODE}
cmake --build $BuildDir --config Release -- /m:2 /p:CL_MPCount=4
if($LASTEXITCODE){exit $LASTEXITCODE}
ctest --test-dir $BuildDir -C Release --output-on-failure --timeout 180
exit $LASTEXITCODE
