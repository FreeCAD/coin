# source files
set(COIN_RENDERING_FILES
	SoGL.cpp
	SoGLBigImage.cpp
	SoGLDriverDatabase.cpp
	SoGLImage.cpp
	SoGLCubeMapImage.cpp
	SoGLContext.cpp
	SoGLNurbs.cpp
	SoRenderManager.cpp
	SoRenderManagerP.cpp
	SoRenderIR.cpp
	SoRenderBackend.cpp
	# SoGLRenderBackend.cpp is intentionally NOT part of libCoin: it is a
	# reference DrawList consumer used only by the testsuite. The production
	# OpenGL viewport renders through SoRenderManager/SoGLRenderAction, not the
	# IR, so the class is compiled into the GL testsuite targets instead (see
	# testsuite/CMakeLists.txt). This keeps the installed renderer surface the
	# Vulkan path only.
	SoOffscreenRenderer.cpp
	SoOffscreenCGData.cpp
	SoOffscreenGLXData.cpp
	SoOffscreenWGLData.cpp
	SoVBO.cpp
	SoVertexArrayIndexer.cpp
	CoinOffscreenGLCanvas.cpp
)

if(COIN_BUILD_VULKAN_RENDERER)
	set(COIN_VULKAN_RENDERING_FILES
		SoVulkanConfig.cpp
		SoVulkanGpuTimers.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendCore.cpp
		SoVulkanRenderBackend/SoVulkanRenderPassCache.cpp
		SoVulkanRenderBackend/SoVulkanPipelineCache.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendPipeline.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendGeometry.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendTexture.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendCommand.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendWideLine.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendGeometryLod.cpp
		SoVulkanRenderBackend/SoVulkanRenderBackendFrame.cpp
		SoVulkanRenderManager.cpp
		SoVulkanRenderBackend/VmaImpl.cpp
	)
endif()

# Files excluded from public API documentation, included in complete documentation.
set(COIN_RENDERING_INTERNAL_FILES
	SoGL.h
	SoGL.cpp
	SoGLContext.cpp
	SoGLNurbs.h
	SoGLNurbs.cpp
	SoRenderManagerP.h
	SoRenderManagerP.cpp
	SoOffscreenCGData.h
	SoOffscreenCGData.cpp
	SoOffscreenGLXData.h
	SoOffscreenGLXData.cpp
	SoOffscreenWGLData.h
	SoOffscreenWGLData.cpp
	SoVBO.h
	SoVBO.cpp
	SoVertexArrayIndexer.h
	SoVertexArrayIndexer.cpp
	CoinOffscreenGLCanvas.h
	CoinOffscreenGLCanvas.cpp
	SoRenderIRP.h
	SoRenderBackend.h
	SoGLRenderBackend.h
	SoVulkanConfig.h
	SoVulkanResult.h
	SoVulkanRenderBackend.h
	SoVulkanRenderBackend/SoVulkanRenderBackendP.h
	SoVulkanRenderBackend/SoVulkanRenderPassCache.h
	SoVulkanShared.h
	SoVulkanRenderManager.h
)

# build library
if(COIN_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${COIN_RENDERING_FILES} ${COIN_VULKAN_RENDERING_FILES})
		set(COIN_SOURCE ${COIN_SOURCE} rendering/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(rendering OBJECT ${COIN_RENDERING_FILES})
	if(COIN_BUILD_VULKAN_RENDERER)
		# Dedicated object library so C++17 (std::optional, std::clamp) is
		# requested only for the Vulkan sources, leaving the core and legacy-GL
		# sources (and every other Coin target) at Coin's default standard.
		add_library(vulkanrendering OBJECT ${COIN_VULKAN_RENDERING_FILES})
		target_compile_features(vulkanrendering PRIVATE cxx_std_17)
	endif()
endif()

# define documentation
foreach(SRCFILE ${COIN_RENDERING_FILES} ${COIN_VULKAN_RENDERING_FILES})
	set(COIN_DOCUMENTATION_FILES ${COIN_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/rendering/${SRCFILE} CACHE INTERNAL "COIN_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${COIN_RENDERING_INTERNAL_FILES})
	set(COIN_INTERNAL_DOCUMENTATION_FILES ${COIN_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/rendering/${SRCFILE} CACHE INTERNAL "COIN_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
