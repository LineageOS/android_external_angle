# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'a4599ef7561abed83d45bab4c7492daeceef92a5',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'c062b6b8527271c6a238d0df710e109d651d82a1',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '449bc986ba6f4c5e10e32828783f9daef2a77644',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'd699296b4d4e3aa4c5b12077264b73a33c06c69a',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'c5b7a2fa18750e435e91e06a50cdc5451c5b9abd',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '90fd66f60fa7de10c91030f8c88b2a5c7c377784',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '58051062663477240484c8904459762ad544ba18',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '931cff66d7b23fc27645e9119e2e570e1d0975ef',
}

deps = {
  'glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
  },

  'spirv-cross/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@{spirv_cross_revision}',
  },

  'spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@{spirv_headers_revision}',
  },

  'spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@{spirv_tools_revision}',
  },

  'vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@{vulkan_headers_revision}',
  },

  'vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@{vulkan_loader_revision}',
  },

  'vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@{vulkan_tools_revision}',
  },

  'vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@{vulkan_validation_revision}',
  },
}
