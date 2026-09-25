/*
 * Copyright (C) 2026 Carlos Lopez
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

// Graphics entry points of the host API (settings.hpp), filled by the mod module that can serve them.
//
// The host API lives in utils/settings.hpp so that every addon exports it without touching its own
// addon.cpp, but what these answer -- which resource RenoDX redirects, how the swap chain proxy pass
// encodes -- belongs to mods::swapchain. Neither header includes the other, so the slots sit here:
// mods::swapchain sets them when it attaches, and the host API reports "not available" while they are
// null, which is also the answer for an addon that does not use the swap chain proxy at all.
namespace renodx::utils::host_graphics {

// See RenoDxHostApi::resolve_clone.
inline bool (*resolve_clone)(void* native_resource, void** out_native_resource) = nullptr;

// See RenoDxHostApi::encode_for_swapchain.
inline bool (*encode_for_swapchain)(void* native_resource, uint32_t d3d12_state, void** out_native_resource,
                                    uint32_t* out_d3d12_state) = nullptr;

// See RenoDxHostApi::encode_ui_for_swapchain.
inline bool (*encode_ui_for_swapchain)(void* native_resource, uint32_t d3d12_state, void** out_native_resource,
                                       uint32_t* out_d3d12_state) = nullptr;

}  // namespace renodx::utils::host_graphics
