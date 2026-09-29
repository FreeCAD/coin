// include/Inventor/rendering/SoRenderLegacyCompat.h

#ifndef COIN_SORENDERLEGACYCOMPAT_H
#define COIN_SORENDERLEGACYCOMPAT_H

// Opt-in source compatibility for the pre-convergence (fork) retained-render
// API.  Enable with -DCOIN_RENDER_LEGACY_API=1 (CMake:
// -DCOIN_BUILD_RENDER_LEGACY_API=ON).
//
// This exists so out-of-tree tests written against the older fork API keep
// building and, where the shim forwards to a canonical field, keep meaning
// while the converged stack model is adopted.  It is default-off, adds no
// behavior when disabled, and is intended to be dropped once the tests are
// migrated to the canonical API.  Every addition it guards forwards onto an
// existing canonical field rather than introducing a second source of truth.

#ifdef COIN_RENDER_LEGACY_API

#include <Inventor/SbBasic.h>
#include <cstdint>

//! Legacy draw-pass classification. Values line up with SoOpacityClass so the
//! canonical opacity classification remains authoritative for ordering.
enum SoRenderPassType : uint8_t {
  SO_RENDERPASS_OPAQUE = 0,
  SO_RENDERPASS_TRANSPARENT = 1,
  SO_RENDERPASS_OVERLAY = 2,
  SO_RENDERPASS_COUNT = 3
};

//! Legacy SoMaterialData::flags bits. Forward onto the canonical texture and
//! pixel-raster fields; see the producer/consumer guards in Coin.
static constexpr uint32_t SO_MAT_HAS_TEXTURE = 0x1u;
static constexpr uint32_t SO_MAT_IS_PIXEL_TEXT = 0x2u;
static constexpr uint32_t SO_MAT_IS_PIXEL_IMAGE = 0x4u;

#endif // COIN_RENDER_LEGACY_API

#endif // COIN_SORENDERLEGACYCOMPAT_H
